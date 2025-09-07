/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "pass/ToSourcePass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace {
// 私有辅助函数
void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty()) {
        return;
    }
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        // 这里+to.length()是为了防止to中有重叠部分，例如从"aa"替换成"a"
        start_pos += to.length();
    }
}
/**
 * 判断是否是 自动生成的临时变量名 (可能重复， 比如迭代器变量)
 */
inline bool MaybeRepated(const std::string& id)
{
    static std::vector<std::string> keys{"$iter-", "$stop-compiler", "iter-compiler"};
    for (auto& key : keys) {
        if (id.find(key) != std::string::npos) {
            return true;
        }
    }
    return false;
}
/**
 * 规范化标识符
 *
 * 1. 给未编码的临时变量名添加id，避免冲突
 * 2. 替换一些特殊符号：比如：$, - 的替换
 */
inline void NormalizedId(std::string& id)
{
    static int counter = 0; // 全局id
    if (MaybeRepated(id)) {
        id += std::to_string(counter++);
    }
    replaceAll(id, "$", "");
    replaceAll(id, "-", "_");
}
/**
 * 标识符转换函数
 */
inline std::string Id(const Identifier& id)
{
    std::string res = id.Val();
    NormalizedId(res);
    return res;
}
} // namespace

///  Ast2SourceException 实现函数

Ast2SourceException::Ast2SourceException(const std::string& msg) noexcept : message(msg)
{
}

const char* Ast2SourceException::what() const noexcept
{
    return message.c_str();
}

namespace {
/*
 * 获取文件名不包括后缀： xxx.cj -> xxx
 */
std::string GetFileNameWithoutSuffix(const std::string& fname)
{
    size_t pos = fname.find_last_of('.');
    if (pos != std::string::npos) {
        return fname.substr(0, pos);
    } else {
        return fname;
    }
}
} // namespace

void ToSourcePass::Run(AstNode& node)
{
    Traverse(node, visitor);
}

/// ToSourcePass 实现函数
void ToSourcePass::Visit(const File& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For File imports: ", node.imports.size());
    std::string fp = config.out + "/" + GetFileNameWithoutSuffix(node.fileName) + config.suffix;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    // package declaration
    TryPrintNode(node.package.get());
    // import statements
    PRT().PVec<ImportSpec>(node.imports, [this](const ImportSpec& imp) { Traverse(imp, visitor); }, "", "", "\n");
    // toplevel decls
    PRT().PVec<Decl>(node.decls, [this](const Decl& decl) {
        if (!config.Focus(decl)) {
            return;
        }
        Traverse(decl, visitor);
        PRT().PNL(2);
    });
    ofs.close();
}

void ToSourcePass::Visit(const PackageSpec& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For PackageSpec: ", node.packageName.Val());
    TryPrintNode(node.modifier.get(), "", " ");
    PRT().PVal("package ");
    PRT().PVec<std::string>(node.prefixPaths, [this](const std::string& pre) { PRT().PVal(pre); }, ".", "", ".");
    PRT().PVal(Id(node.packageName));
    PRT().PNL(2);
}

namespace {
/**
 * Check if the import is std.core.*.
 * @param ic ImportContent to check.
 * @return true if it is std.core.*, false otherwise.
 */
inline bool IsImportStdCore(const ImportContent& ic)
{
    // std.core.*
    auto& paths = ic.prefixPaths;
    return ic.kind == ImportKind::IMPORT_ALL && paths.size() == 2 && paths[0] == "std" && paths[1] == "core";
}

inline bool IsDuplicatedImport(const ImportSpec& node)
{
    if (node.content.kind == ImportKind::IMPORT_MULTI) {
        // Import multi-packages is desugared as serveral import single-packages
        return true;
    }
    if (IsImportStdCore(node.content)) {
        // import std.core.*
        return true;
    }
    return false;
}
} // namespace

void ToSourcePass::Visit(const ImportSpec& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ImportSpec");
    if (IsDuplicatedImport(node)) {
        // Import multi-packages is desugared as serveral import single-packages, import std.core.* is implicit import!
        return;
    }
    VisitNodes(node.annotations);
    TryPrintNode(node.modifier.get(), "", " ");
    PRT().PVal("import ");
    Traverse(node.content, visitor);
    PRT().PNL();
}

void ToSourcePass::Visit(const ImportContent& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ImportContent");
    PRT().PVec<std::string>(node.prefixPaths, [this](const std::string& pre) { PRT().PVal(pre); }, ".", "", ".");
    if (node.kind == ImportKind::IMPORT_SINGLE) {
        // import xxx.a
        PRT().PVal(Id(node.identifier));
    } else if (node.kind == ImportKind::IMPORT_ALL) {
        PRT().PVal("*");
    } else if (node.kind == ImportKind::IMPORT_ALIAS) {
        // import xxx.a as b
        PRT().PSVals(" ", Id(node.identifier), "as", Id(node.aliasName));
    }
}

void ToSourcePass::Visit(const Annotation& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For Annotation");
    PRT().PVals("@", Id(node.identifier));
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "[", "]");
    PRT().PNL();
}

void ToSourcePass::Visit(const Modifier& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For Modifier");
    PRT().PVal(Tk2Str(node.modifier));
}

// Decls
namespace {
inline std::string GetVarKeyword(const VarDeclAbstract& node)
{
    if (node.isConst) {
        return "const";
    } else if (node.isVar) {
        return "var";
    } else {
        return "let";
    }
}

/**
 * @brief prop是否有body。
 * @return 有返回 true，否则返回 false。
 */
inline bool HasBody(const PropDecl& propDecl)
{
    // 语义分析后可能会有空的 getter
    return !propDecl.getters.empty() && !propDecl.getters[0]->TestAttr(Attribute::ABSTRACT);
}
} // namespace

void ToSourcePass::Visit(const VarDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For VarDecl: ", node.identifier.Val());
    PrintDecl(node);
    if (TryPrintEnumConstructor(node)) {
        return;
    }
    auto id = Id(node.identifier);
    // Record: variable -> normalized id
    if (MaybeRepated(node.identifier.Val())) {
        desugaredVarId.emplace(&node, id);
    }
    PRT().PVals(GetVarKeyword(node), " ", id);
    PrintVarType(node);
    TryPrintNode(node.initializer.get(), " = ");
}

void ToSourcePass::Visit(const VarWithPatternDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For VarWithPatternDecl: ", node.identifier.Val());
    PrintDecl(node);
    TryPrintNode(node.irrefutablePattern.get(), GetVarKeyword(node) + " ");
    PrintVarType(node);
    TryPrintNode(node.initializer.get(), " = ");
}

void ToSourcePass::Visit(const PropDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For PropDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("prop ");
    PRT().PVal(Id(node.identifier));
    TryPrintType(node.type.get());
    if (!HasBody(node)) {
        return;
    }
    PRT().PValNL(" {");
    PRT().Indent();
    TryPrintNode(node.getters[0].get(), "", "", true);
    if (!node.setters.empty()) {
        TryPrintNode(node.setters[0].get(), "", "", true);
    }
    PRT().Unindent();
    PRT().PVal("}");
}

void ToSourcePass::Visit(const FuncParam& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For FuncParam");
    PrintDecl(node);
    if (node.hasLetOrVar) {
        PRT().PVals(GetVarKeyword(node), " ");
    }
    PRT().PVal(Id(node.identifier));
    if (node.isNamedParam) {
        PRT().PVal("!");
    }
    PrintVarType(node);
    TryPrintNode(node.initializer.get(), " = ");
}

void ToSourcePass::Visit(const FuncParamList& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For FuncParamList: ", node.params.size());
    PRT().PVec<FuncParam>(
        node.params, [this](const FuncParam& param) { Traverse(param, visitor); }, ", ", "(", ")", true);
}

namespace {
inline Ptr<Ty> TryGetRetTy(Ptr<Ty> ty)
{
    if (ty->IsFunc()) {
        return Cast<FuncTy*>(ty)->retTy;
    }
    return nullptr;
}
} // namespace

void ToSourcePass::Visit(const FuncBody& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For FuncBody");
    TryPrintGenericParams(node.generic.get());
    VisitNode(node.paramLists[0]);
    if (!TryPrintType(node.retType) && config.Sema()) {
        TryPrintTy(TryGetRetTy(node.ty));
    }
    TryPrintGenericConstraints(node.generic);
    PrintBlock(node.body);
}

void ToSourcePass::Visit(const FuncDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For FuncDecl: ", node.identifier.Val());
    PrintDecl(node);
    if (TryPrintEnumConstructor(node) || TryPrintConstructor(node) || TryPrintGetter(node) || TryPrintSetter(node)) {
        return;
    }
    // General func.
    PRT().PVals("func ", Id(node.identifier));
    TryPrintNode(node.funcBody);
}

VisitResult ToSourcePass::Before(const MainDecl& node)
{
    if (!node.desugarDecl) {
        return VisitResult::Cont();
    }
    Logger::Get().Debug("ToSourcePass::Before", "For MainDecl");
    // 存在解糖节点
    if (config.Desugar()) {
        Logger::Get().Debug("ToSourcePass::Before", "For Desugared Decl of MainDecl");
        if (node.TestAttr(Attribute::UNSAFE)) {
            PRT().PVal("unsafe ");
        }
        VisitNode(node.desugarDecl);
    } else {
        // 还原原节点
        Logger::Get().Debug("ToSourcePass::Before", "For Recover Desugared Decl of MainDecl");
        PrintDecl(node);
        PRT().PVal("main");
        TryPrintNode(node.desugarDecl->funcBody);
    }
    return VisitResult::Skip();
}

void ToSourcePass::Visit(const MainDecl& node, VisitResult& res)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For MainDecl");
    PrintDecl(node);
    PRT().PVal("main");
    AH_CHECK_NULL(node.funcBody);
    Visit(*node.funcBody, res);
}

void ToSourcePass::Visit(const PrimaryCtorDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For PrimaryCtorDecl: ", node.identifier.Val());
    if (config.Sema()) {
        return; // PrimaryCtorDecl is desugared in Sema
    }
    PrintDecl(node);
    PRT().PVal(Id(node.identifier));
    AH_CHECK_NULL(node.funcBody);
    TryPrintNode(node.funcBody.get());
}

void ToSourcePass::Visit(const ClassDecl& node, VisitResult& res)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ClassDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "class");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void ToSourcePass::Visit(const InterfaceDecl& node, VisitResult& res)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For InterfaceDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "interface");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void ToSourcePass::Visit(const StructDecl& node, VisitResult& res)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For StructDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "struct");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void ToSourcePass::Visit(const EnumDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For EnumDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "enum");
    PRT().PValNL(" {").Indent();
    PRT().PVec<Decl>(node.constructors, [this](const Decl& decl) {
        PRT().PVal("| ");
        Traverse(decl, visitor);
        PRT().PNL();
    });
    PRT().PNL();
    PrintDecls(node.members);
    PRT().Unindent();
    PRT().PVal("}");
}

void ToSourcePass::Visit(const ExtendDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ExtendDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("extend");
    TryPrintGenericParams(node.generic.get());
    TryPrintNode(node.extendedType.get(), " ");
    PrintInheritedTypes(node.inheritedTypes);
    TryPrintGenericConstraints(node.generic.get());
    PrintInheritableDeclBody(node.members);
}

void ToSourcePass::Visit(const TypeAliasDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For TypeAliasDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("type ");
    PRT().PVal(Id(node.identifier));
    TryPrintNode(node.type.get(), " = ");
}

// Type
void ToSourcePass::Visit(const PrimitiveType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For PrimitiveType");
    PRT().PVal(node.str);
}

void ToSourcePass::Visit(const RefType& node, VisitResult& res)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For RefType");
    if (node.ref.identifier.Val() == "" && config.Sema() && node.ty) {
        PrintTy(*node.ty);
    } else {
        PRT().PVal(Id(node.ref.identifier));
        PRT().PVec<AstNode>(
            node.typeArguments, [this](const AstNode& node) { Traverse(node, visitor); }, ", ", "<", ">");
    }
}

VisitResult ToSourcePass::Before(const OptionType& node)
{
    if (!config.Desugar() || !node.desugarType) {
        // desugar is false && node.desugarType is not nullptr is okay, because node.componentType is not nullptr.
        return VisitResult::Cont();
    }
    Logger::Get().Debug("ToSourcePass::Before", "For OptionType: desugar: ", node.desugarType != nullptr);
    TryPrintNode(node.desugarType);
    return VisitResult::Skip();
}

void ToSourcePass::Visit(const OptionType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For OptionType");
    std::string preQuest(node.questNum, '?');
    TryPrintNode(node.componentType.get(), preQuest);
}

void ToSourcePass::Visit(const TupleType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For TupleType");
    PRT().PVec<Type>(node.fieldTypes, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "(", ")");
}

void ToSourcePass::Visit(const QualifiedType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For QualifiedType");
    VisitNode(node.baseType);
    PRT().PVals(".", Id(node.field));
    PRT().PVec<Type>(node.typeArguments, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "<", ">");
}

void ToSourcePass::Visit(const ThisType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ThisType");
    PRT().PVal("This");
}

void ToSourcePass::Visit(const VArrayType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For VArrayType");
    PRT().PVal("VArray<");
    VisitNode(node.typeArgument);
    PRT().PVal(", ");
    VisitNode(node.constantType);
    PRT().PVal(">");
}

void ToSourcePass::Visit(const ParenType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ParenType");
    TryPrintNode(node.type.get(), "(", ")");
}

void ToSourcePass::Visit(const ConstantType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ConstantType");
    TryPrintNode(node.constantExpr.get(), "$");
}

void ToSourcePass::Visit(const FuncType& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For FuncType");
    PRT().PVec<Type>(node.paramTypes, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "(", ")", true);
    TryPrintNode(node.retType.get(), " -> ");
}

// Pattern
void ToSourcePass::Visit(const WildcardPattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For WildcardPattern");
    PRT().PVal("_");
}

void ToSourcePass::Visit(const ConstPattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ConstPattern");
    VisitNode(node.literal);
}

void ToSourcePass::Visit(const EnumPattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For EnumPattern");

    AH_CHECK_NULL(node.constructor);
    // RefExpr 单独处理
    auto ctor = node.constructor.get();
    if (ctor->astKind == AstKind::REF_EXPR) {
        auto refExpr = Cast<RefExpr*>(ctor.get());
        PRT().PVal(Id(refExpr->ref.identifier));
        PrintInstArgs(*refExpr, true);
    } else {
        VisitNode(ctor);
    }
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, ", ", "(", ")");
}

void ToSourcePass::Visit(const VarPattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For VarPattern", node.varDecl->identifier.Val());
    PRT().PVal(Id(node.varDecl->identifier));
}

void ToSourcePass::Visit(const VarOrEnumPattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For VarOrEnumPattern: ", node.identifier.Val());
    PRT().PVal(Id(node.identifier));
    // pattern 是 解糖后才有的？
    // VisitNode(node.pattern);
}

void ToSourcePass::Visit(const TypePattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For TypePattern");
    VisitNode(node.pattern);
    TryPrintType(node.type.get());
}

void ToSourcePass::Visit(const TuplePattern& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For TuplePattern");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, ", ", "(", ")");
}

// Expr
void ToSourcePass::Visit(const Block& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For Block: ", node.body.size());
    PRT().PVec<AstNode>(node.body, [this](const AstNode& node) {
        auto res = Traverse(node, visitor);
        PRT().PNL();
    });
}

VisitResult ToSourcePass::Before(const RefExpr& node)
{
    if (!config.Sema()) {
        return VisitResult::Cont();
    }
    auto target = node.ref.target;
    if (auto it = desugaredVarId.find(target); it != desugaredVarId.end()) {
        Logger::Get().Debug("ToSourcePass::Before", "For RefExpr: ", node.ref.identifier.Val());
        PRT().PVal(it->second);
        return VisitResult::Skip();
    }
    return VisitResult::Cont();
}

void ToSourcePass::Visit(const RefExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For RefExpr: ", node.ref.identifier.Val());
    PRT().PVal(Id(node.ref.identifier));
    PrintInstArgs(node);
}

void ToSourcePass::Visit(const FuncArg& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For FuncArg");
    VisitNode(node.expr);
}

void ToSourcePass::Visit(const CallExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For CallExpr");
    if (TryRecoverCallExpr(node)) {
        return;
    }
    // General func call
    VisitNode(node.baseFunc);
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")", true);
}

void ToSourcePass::Visit(const ReturnExpr& node, VisitResult& res)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ReturnExpr");
    // return in init, skip
    auto body = node.refFuncBody;
    if (body && body->funcDecl && body->funcDecl->TestAttr(Attribute::CONSTRUCTOR)) {
        res.status = false;
        return;
    }
    PRT().PVal("return");
    if (node.expr) {
        PRT().PVal(" ");
        VisitNode(node.expr);
    }
}

void ToSourcePass::Visit(const LitConstExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For LitConstExpr");
    PRT().PVal(node.ToString());
}

void ToSourcePass::Visit(const ArrayLit& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ArrayLit");
    PRT().PVec<AstNode>(node.children, [this](const AstNode& expr) { Traverse(expr, visitor); }, ", ", "[", "]", true);
}

void ToSourcePass::Visit(const TupleLit& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For TupleLit");
    PRT().PVec<AstNode>(node.children, [this](const AstNode& expr) { Traverse(expr, visitor); }, ", ", "(", ")", true);
}

void ToSourcePass::Visit(const TypeConvExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For TypeConvExpr");
    VisitNode(node.type);
    TryPrintNode(node.expr, "(", ")");
}

namespace {
// 检查 expr 是否是对 Enum 类型的引用
inline bool IsRefEnum(const Expr& expr)
{
    if (expr.astKind != AstKind::REF_EXPR) {
        return false;
    }
    return Cast<const RefExpr&>(expr).ref.target->astKind == AstKind::ENUM_DECL;
}
} // namespace

void ToSourcePass::Visit(const MemberAccess& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For MemberAccess");
    VisitNode(node.baseExpr);
    PRT().PVals(".", Id(node.field));
    if (!config.Sema() || !IsRefEnum(*node.baseExpr)) {
        PrintInstArgs(node, node.isPattern);
    }
}

void ToSourcePass::Visit(const LambdaExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For LambdaExpr");
    auto& body = *node.funcBody;
    PRT().PVal("{");
    AH_ASSERT(body.paramLists.size() == 1);
    auto& params = body.paramLists[0]->params;
    PRT().PVec<FuncParam>(params, [this](const FuncParam& param) { Traverse(param, visitor); }, ", ", " ");
    PRT().PWI([this, &body] { VisitNode(body.body); }, " =>", "}");
}

void ToSourcePass::Visit(const MatchCase& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For MatchCase");
    PRT().PVal("case ");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, " | ");
    TryPrintNode(node.patternGuard.get(), " where ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void ToSourcePass::Visit(const MatchCaseOther& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For MatchCaseOther");
    TryPrintNode(node.matchExpr.get(), "case ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void ToSourcePass::Visit(const MatchExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For MatchExpr");
    PRT().PVal("match ");
    // match with selector
    TryPrintNode(node.selector.get(), "(", ")");
    PRT().PValNL(" {").Indent();
    PRT().PVec<MatchCase>(node.matchCases, [this](const MatchCase& mc) { Traverse(mc, visitor); });
    PRT().PVec<MatchCaseOther>(node.matchCaseOthers, [this](const MatchCaseOther& mco) { Traverse(mco, visitor); });
    PRT().Unindent();
    PRT().PVal("}");
}

void ToSourcePass::Visit(const IsExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For IsExpr");
    VisitNode(node.leftExpr);
    PRT().PVal(" is ");
    VisitNode(node.isType);
}

void ToSourcePass::Visit(const AsExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For AsExpr");
    VisitNode(node.leftExpr);
    PRT().PVal(" as ");
    VisitNode(node.asType);
}

VisitResult ToSourcePass::Before(const AssignExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    Logger::Get().Debug("ToSourcePass::Before", "For AssignExpr");
    if (config.Desugar()) {
        Traverse(*node.desugarExpr, visitor);
    } else {
        // desugared: x.[](i, y) -> x[i] = v
        auto& callExpr = Cast<const CallExpr&>(node.desugarExpr.get());
        AH_ASSERT(callExpr.args.size() == 2);
        AH_ASSERT(callExpr.baseFunc->astKind == AstKind::MEMBER_ACCESS);
        auto& ma = Cast<const MemberAccess&>(callExpr.baseFunc.get());
        VisitNode(ma.baseExpr);
        TryPrintNode(callExpr.args[0].get(), "[", "]");
        PRT().PVal(" = ");
        VisitNode(callExpr.args[1]);
    }
    return VisitResult::Skip();
}

void ToSourcePass::Visit(const AssignExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For AssignExpr");
    VisitNode(node.leftValue);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void ToSourcePass::Visit(const IncOrDecExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For IncOrDecExpr");
    TryPrintNode(node.expr.get(), "", Tk2Str(node.op));
}

VisitResult ToSourcePass::Before(const UnaryExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    Logger::Get().Debug("ToSourcePass::Before", "For UnaryExpr");
    if (config.Desugar()) {
        Traverse(*node.desugarExpr, visitor);
    } else {
        // desugared: val.!() -> !val
        auto& callExpr = Cast<const CallExpr&>(node.desugarExpr.get());
        AH_ASSERT(callExpr.args.size() == 0);
        AH_ASSERT(callExpr.baseFunc->astKind == AstKind::MEMBER_ACCESS);
        auto& ma = Cast<const MemberAccess&>(callExpr.baseFunc.get());
        TryPrintNode(ma.baseExpr, Tk2Str(node.op));
    }
    return VisitResult::Skip();
}

void ToSourcePass::Visit(const UnaryExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For UnaryExpr");
    TryPrintNode(node.expr.get(), Tk2Str(node.op));
}

VisitResult ToSourcePass::Before(const BinaryExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    Logger::Get().Debug("ToSourcePass::Before", "For BinaryExpr");
    if (config.Desugar()) {
        Traverse(*node.desugarExpr, visitor);
    } else {
        auto& callExpr = Cast<const CallExpr&>(node.desugarExpr.get());
        AH_ASSERT(callExpr.args.size() == 0);
        AH_ASSERT(callExpr.baseFunc->astKind == AstKind::MEMBER_ACCESS);
        auto& ma = Cast<const MemberAccess&>(callExpr.baseFunc.get());
        PRT().PVals(" ", Tk2Str(node.op), " ");
        VisitNode(callExpr.args[0]);
    }
    return VisitResult::Skip();
}
void ToSourcePass::Visit(const BinaryExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For BinaryExpr");
    VisitNode(node.leftExpr);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void ToSourcePass::Visit(const ThrowExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ThrowExpr");
    PRT().PVal("throw ");
    VisitNode(node.expr);
}

VisitResult ToSourcePass::Before(const SubscriptExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    Logger::Get().Debug("ToSourcePass::Before", "For SubscriptExpr");
    if (config.Desugar()) {
        Traverse(*node.desugarExpr, visitor);
    } else {
        // desugared: a.[](i) -> a[i]
        auto& callExpr = Cast<const CallExpr&>(node.desugarExpr.get());
        AH_ASSERT(callExpr.args.size() == 0);
        AH_ASSERT(callExpr.baseFunc->astKind == AstKind::MEMBER_ACCESS);
        auto& ma = Cast<const MemberAccess&>(callExpr.baseFunc.get());
        VisitNode(ma.baseExpr);
        TryPrintNode(callExpr.args[0], "[", "]");
    }
    return VisitResult::Skip();
}

void ToSourcePass::Visit(const SubscriptExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For SubscriptExpr");
    VisitNode(node.baseExpr);
    PRT().PVec<Expr>(node.indexExprs, [this](const Expr& expr) { Traverse(expr, visitor); }, ", ", "[", "]");
}

void ToSourcePass::Visit(const JumpExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For JumpExpr");
    if (node.isBreak) {
        PRT().PVal("break");
    } else {
        PRT().PVal("continue");
    }
}

void ToSourcePass::Visit(const RangeExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For RangeExpr");
    TryPrintNode(node.startExpr.get());
    PRT().PVal("..");
    TryPrintNode(node.stopExpr.get());
    TryPrintNode(node.stepExpr.get(), " : ");
}

void ToSourcePass::Visit(const LetPatternDestructor& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For LetPatternDestructor");
    PRT().PVal("let ");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, " | ");
    TryPrintNode(node.initializer, " <- ");
}

void ToSourcePass::Visit(const IfExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For IfExpr");
    PRT().PVal("if ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.thenBody); }, " {", "}");
    TryPrintNode(node.elseBody.get(), " else ");
}

void ToSourcePass::Visit(const WhileExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For WhileExpr");
    PRT().PVal("while ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.body); }, " {", "}");
}

void ToSourcePass::Visit(const DoWhileExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For DoWhileExpr");
    PRT().PWI([this, &node] { VisitNode(node.body); }, "do {", "} while ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PNL();
}

void ToSourcePass::Visit(const ForInExpr& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For ForInExpr");
    if (TryPrintDesugaredForInExpr(node)) {
        return;
    }
    TryPrintNode(node.pattern.get(), "for (", " in ");
    TryPrintNode(node.inExpression.get());
    TryPrintNode(node.patternGuard.get());
    PRT().PWI([this, &node] { VisitNode(node.body); }, ") {", "}");
}

// Generic
void ToSourcePass::Visit(const Generic& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For Generic");
    PRT().PVec<GenericParamDecl>(
        node.typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, visitor); }, ", ", "<", ">");
    PRT().PVec<GenericConstraint>(
        node.genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, visitor); }, ", ", " where ");
}

void ToSourcePass::Visit(const GenericParamDecl& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For GenericParamDecl");
    PRT().PVal(Id(node.identifier));
}

void ToSourcePass::Visit(const GenericConstraint& node, VisitResult&)
{
    Logger::Get().Debug("ToSourcePass::Visit", "For GenericConstraint");
    VisitNode(node.type);
    PRT().PVal(" <: ");
    PRT().PVec<Type>(node.upperBounds, [this](const Type& tp) { Traverse(tp, visitor); }, " & ");
}

/// 私有实现函数
namespace {
void CreateDirIfNotExists(const std::string& path)
{
    if (fs::exists(path)) {
        return;
    }
    // 创建目录（包括父目录）
    if (!fs::create_directories(path)) {
        throw Ast2SourceException("Failed to create directory: " + path);
    }
}
} // namespace

ToSourcePass::ToSourcePass(PassConfig config) : Pass(config), prt(ofs, config.indent)
{
    CreateDirIfNotExists(config.out);
    RegisterHandlers();
}

void ToSourcePass::RegisterHandlers()
{
    static std::unordered_map<std::string, AstKind> name2kind{
#define AST_INFO(KIND, STR, DEF) {#DEF, AstKind::KIND},
#include "wrapper/AstInfo.inc"
#undef AST_INFO
    };
// 定义注册代码片段
#define GEN_REG_BEFORE_HANDLER(N)                                                                                      \
    visitor.RegBefore(name2kind.at(#N), [this](const AstNode& node) { return this->Before(Cast<const N&>(node)); })

#define GEN_REG_VISIT_HANDLER(N)                                                                                       \
    visitor.RegVisit(                                                                                                  \
        name2kind.at(#N), [this](const AstNode& node, VisitResult& res) { this->Visit(Cast<const N&>(node), res); })

    // 使用宏生成代码
    // 递归展开需要重写的解糖节点
    EXPAND4(GEN_REG_BEFORE_HANDLER, MainDecl, AssignExpr, UnaryExpr, BinaryExpr);
    EXPAND3(GEN_REG_BEFORE_HANDLER, RefExpr, SubscriptExpr, OptionType);

    // 递归展开需要重写的节点
    EXPAND3(GEN_REG_VISIT_HANDLER, Annotation, Modifier, File);
    EXPAND3(GEN_REG_VISIT_HANDLER, PackageSpec, ImportSpec, ImportContent);
    // Decl
    EXPAND4(GEN_REG_VISIT_HANDLER, VarDecl, VarWithPatternDecl, PropDecl, FuncParam);
    EXPAND4(GEN_REG_VISIT_HANDLER, FuncParamList, FuncBody, FuncDecl, MainDecl);
    EXPAND4(GEN_REG_VISIT_HANDLER, PrimaryCtorDecl, ClassDecl, InterfaceDecl, StructDecl);
    EXPAND3(GEN_REG_VISIT_HANDLER, EnumDecl, ExtendDecl, TypeAliasDecl);
    // Type
    EXPAND4(GEN_REG_VISIT_HANDLER, PrimitiveType, RefType, TupleType, OptionType);
    EXPAND4(GEN_REG_VISIT_HANDLER, QualifiedType, ThisType, VArrayType, ParenType);
    EXPAND2(GEN_REG_VISIT_HANDLER, ConstantType, FuncType);
    // Pattern
    EXPAND4(GEN_REG_VISIT_HANDLER, WildcardPattern, ConstPattern, EnumPattern, VarPattern);
    EXPAND3(GEN_REG_VISIT_HANDLER, TypePattern, VarOrEnumPattern, TuplePattern);
    // Expr
    EXPAND4(GEN_REG_VISIT_HANDLER, Block, FuncArg, MatchCase, MatchCaseOther);
    EXPAND4(GEN_REG_VISIT_HANDLER, MemberAccess, CallExpr, IncOrDecExpr, RangeExpr);
    EXPAND4(GEN_REG_VISIT_HANDLER, LitConstExpr, ArrayLit, ReturnExpr, LambdaExpr);
    EXPAND4(GEN_REG_VISIT_HANDLER, MatchExpr, IsExpr, AsExpr, ThrowExpr);
    EXPAND4(GEN_REG_VISIT_HANDLER, JumpExpr, LetPatternDestructor, TupleLit, TypeConvExpr);
    EXPAND4(GEN_REG_VISIT_HANDLER, IfExpr, DoWhileExpr, WhileExpr, ForInExpr);
    EXPAND4(GEN_REG_VISIT_HANDLER, AssignExpr, UnaryExpr, BinaryExpr, RefExpr);
    EXPAND1(GEN_REG_VISIT_HANDLER, SubscriptExpr);
    // Generic
    EXPAND3(GEN_REG_VISIT_HANDLER, Generic, GenericParamDecl, GenericConstraint);
}

// 辅助打印函数
/**
 * @brief 打印节点。
 * @param pnode 节点指针。
 * @param pre 前缀字符串。
 * @param suf 后缀字符串。
 * @param withNL 是否追加空行。
 */
inline void ToSourcePass::TryPrintNode(
    const Ptr<AstNode> pnode, const std::string& pre, const std::string& suf, bool withNL)
{
    if (!pnode) {
        return;
    }
    PRT().PVal(pre);
    Traverse(*pnode, visitor);
    PRT().PVal(suf);
    if (withNL) {
        PRT().PNL();
    }
}

/**
 * @brief 辅助打印声明节点。
 * @param node 声明节点的引用。
 */
void ToSourcePass::PrintDecl(const Decl& node)
{
    PrintAnnotations(node);
    VisitNode(node.annotationsArray);
    PrintModifiers(node);
}

namespace {
// 关注的注解属性映射表
std::unordered_map<std::string, Attribute> focusAttrsMap = {{"C", Attribute::C}, {"public", Attribute::PUBLIC},
    {"protected", Attribute::PROTECTED}, {"private", Attribute::PRIVATE}, {"internal", Attribute::INTERNAL}};
} // namespace

/**
 * @brief 辅助打印注解列表。
 *  注解打印规则:
 * 1. 用户代码注解列表： node.annotations
 * 2. 配置忽略打印的列表： config.ignoreAnnotations
 * 3. 语义后置的注解列表： config.focusAnnotationAttrs
 * 规则描述:
 * (node.annotations + config.focusAnnotationAttrs) - config.ignoreAnnotations
 */
void ToSourcePass::PrintAnnotations(const Decl& node)
{
    std::unordered_set<std::string> annotations;
    for (auto& anno : node.annotations) {
        auto& annoName = anno->identifier.Val();
        if (!config.ignoreAnnotations.count(annoName)) {
            Traverse(*anno, visitor);
            annotations.insert(annoName);
        }
    }
    if (!config.Sema()) {
        return;
    }
    // 补充打印语义后的缺少的注解
    PRT().PVec<std::string>(config.focusAnnotationAttrs, [&node, &annotations, this](const std::string& anno) {
        // node 有关注的属性 没有打印过 也没有忽略
        if (node.TestAttr(focusAttrsMap.at(anno)) && !annotations.count(anno) &&
            !config.ignoreAnnotations.count(anno)) {
            PRT().PValNL("@" + anno);
        }
    });
}

namespace {
inline bool NeedAddMoidifier(const Decl& node)
{
    if (node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    if (node.IsFunc()) {
        auto& fn = Cast<const FuncDecl&>(node);
        if (fn.isGetter || fn.isSetter) {
            return false;
        }
    }
    if (node.IsFuncOrProp() && node.outerDecl && node.outerDecl->astKind == AstKind::INTERFACE_DECL) {
        return false;
    }
    return true;
}
} // namespace

/**
 * @brief 辅助打印修饰符列表。
 */
void ToSourcePass::PrintModifiers(const Decl& node)
{
    std::unordered_set<std::string> modifiers;
    PRT().PVec<Modifier>(
        node.modifiers,
        [this, &modifiers](const Modifier& mod) {
            modifiers.insert(Tk2Str(mod.modifier));
            Traverse(mod, visitor);
        },
        " ", "", " ");

    if (!config.Sema() || !config.focusModifierWhiteList.count(node.astKind) || !NeedAddMoidifier(node)) {
        return;
    }
    // 补充打印语义后的缺少的修饰符
    PRT().PVec<std::string>(config.focusModifierAttrs, [&node, &modifiers, this](const std::string& mod) {
        // node 有关注的属性 没有打印过
        if (node.TestAttr(focusAttrsMap.at(mod)) && !modifiers.count(mod)) {
            PRT().PVals(mod, " ");
        }
    });
}

/**
 * @brief 辅助打印代码块。
 * @param pnode : Block节点
 */
inline void ToSourcePass::PrintBlock(const Ptr<Block> pnode)
{
    if (!pnode) {
        return;
    }
    PRT().PWI([this, pnode] { Traverse(*pnode, visitor); }, " {", "}");
}

/**
 * @brief 尝试作为构造函数打印。
 * @param decl 声明的引用: FuncDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToSourcePass::TryPrintConstructor(const FuncDecl& node)
{
    if (!node.TestAttr(Attribute::CONSTRUCTOR)) {
        return false;
    }
    Logger::Get().Debug("ToSourcePass::TryPrintConstructor", "For FuncDecl is constructor");
    PRT().PVal(Id(node.identifier));
    VisitNode(node.funcBody->paramLists[0]);
    PrintBlock(node.funcBody->body);
    return true;
}
/**
 * @brief 尝试作为getter打印。
 * @param decl 枚举声明的引用: VarDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToSourcePass::TryPrintGetter(const FuncDecl& node)
{
    if (!node.isGetter) {
        return false;
    }
    Logger::Get().Debug("ToSourcePass::TryPrintGetter", "For FuncDecl is getter");
    PRT().PVal("get()");
    PrintBlock(node.funcBody->body);
    return true;
}
/**
 * @brief 尝试作为setter打印。
 * @param decl 枚举声明的引用: VarDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToSourcePass::TryPrintSetter(const FuncDecl& node)
{
    if (!node.isSetter) {
        return false;
    }
    Logger::Get().Debug("ToSourcePass::TryPrintSetter", "For FuncDecl is setter");
    AH_ASSERT(node.funcBody->paramLists[0]->params.size() == 1);
    PRT().PVals("set(", Id(node.funcBody->paramLists[0]->params[0]->identifier), ")");
    PrintBlock(node.funcBody->body);
    return true;
}

/**
 * @brief 尝试作为Enum构造器打印。
 * @param decl 枚举声明的引用: VarDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToSourcePass::TryPrintEnumConstructor(const VarDecl& node)
{
    if (!node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    Logger::Get().Debug("ToSourcePass::TryPrintEnumConstructor", "For VarDecl as EnumConstructor");
    PRT().PVal(node.identifier.Val());
    return true;
}

/**
 * @brief 尝试作为Enum构造器打印。
 * @param decl 枚举声明的引用: FuncDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToSourcePass::TryPrintEnumConstructor(const FuncDecl& node)
{
    if (!node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    Logger::Get().Debug("ToSourcePass::TryPrintEnumConstructor", "For FuncDecl as EnumConstructor");
    PRT().PVal(Id(node.identifier));
    AH_ASSERT(node.funcBody->paramLists[0]->params.size() > 0);
    auto& params = node.funcBody->paramLists[0]->params;
    PRT().PVec<FuncParam>(
        params, [this](const FuncParam& param) { TryPrintNode(param.type.get()); }, ", ", "(", ")", true);
    return true;
}

/**
 * @brief 辅助打印继承类型。
 */
inline void ToSourcePass::PrintInheritedTypes(const std::vector<OwnedPtr<Type>>& types)
{
    PRT().PVec<Type>(types, [this](const Type& ty) { Traverse(ty, visitor); }, " & ", " <: ");
}

/**
 * @brief 辅助打印可继承类型头部。
 * @param node 声明引用 (可继承类型: Class, Struct, Interface, Enum)
 */
void ToSourcePass::PrintInheritableDeclHeader(const InheritableDecl& node, const std::string& keyword)
{
    PrintDecl(node);
    PRT().PVals(keyword, " ", Id(node.identifier)); // class, interface, struct, enum
    TryPrintGenericParams(node.generic.get());
    PrintInheritedTypes(node.inheritedTypes);
    TryPrintGenericConstraints(node.generic.get());
}

/**
 * @brief 辅助打印一组声明。
 */
inline void ToSourcePass::PrintDecls(const std::vector<OwnedPtr<Decl>>& decls)
{
    PRT().PVec<Decl>(decls, [this](const Decl& decl) {
        Traverse(decl, visitor);
        PRT().PNL(2);
    });
}

/**
 * @brief 辅助打印可继承类型定义体。
 */
void ToSourcePass::PrintInheritableDeclBody(const std::vector<OwnedPtr<Decl>>& members)
{
    PRT().PValNL(" {");
    PRT().Indent();
    PrintDecls(members);
    PRT().Unindent();
    PRT().PVal("}");
}

/**
 * @brief 辅助打印泛型参数。
 * @param generic 泛型节点指针。
 */
void ToSourcePass::TryPrintGenericParams(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    Logger::Get().Debug("ToSourcePass::Visit", "For GenericParams");
    PRT().PVec<GenericParamDecl>(
        generic->typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, visitor); }, ", ", "<", ">");
}

/**
 * @brief 辅助打印泛型约束。
 * @param generic 泛型节点指针。
 */
void ToSourcePass::TryPrintGenericConstraints(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    Logger::Get().Debug("ToSourcePass::Visit", "For GenericConstraints");
    PRT().PVec<GenericConstraint>(
        generic->genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, visitor); }, ", ", " where ");
}

/**
 * @brief 尝试打印泛型实例参数。
 * @param ref 引用表达式: RefExpr or MemberAccess
 * @param isPattern 是否是在 pattern 中 (enum pattern 不允许打印泛型参数)
 */
void ToSourcePass::PrintInstArgs(const NameReferenceExpr& ref, bool isPattern)
{
    if (!ref.typeArguments.empty()) {
        PRT().PVec<Type>(ref.typeArguments, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "<", ">");
    } else if (config.Sema() && !isPattern) {
        PRT().PVec<Ty>(ref.instTys, [this](const Ty& ty) { PrintTy(ty); }, ", ", "<", ">");
    }
}

/**
 * @brief 辅助打印 变量的类型标注。
 */
inline void ToSourcePass::PrintVarType(const VarDeclAbstract& node)
{
    if (!TryPrintType(node.type.get()) && config.Sema()) {
        TryPrintTy(node.ty);
    }
}

/**
 * @brief 辅助打印 Type 节点
 * @param type 类型节点指针。
 */
bool ToSourcePass::TryPrintType(const Ptr<Type> type)
{
    if (!type) {
        return false;
    }
    if (type->astKind != AstKind::TYPE) {
        // 合法的 Type 语法节点
        PRT().PVal(": ");
        Traverse(*type, visitor);
        return true;
    }
    if (config.Sema()) {
        return TryPrintTy(type->ty);
    }
    return false;
}

/**
 * @brief 辅助打印 Ty 标注。
 */
inline bool ToSourcePass::TryPrintTy(const Ptr<Ty> ty)
{
    if (!ty) {
        return false;
    }
    PRT().PVal(": ");
    PrintTy(*ty);
    return true;
}

/**
 * @brief 辅助打印 Ty 语义信息。
 * @param ty 语义信息引用。
 */
void ToSourcePass::PrintTy(const Ty& ty)
{

    // If the format is incorrect, need to adjust it.
    switch (ty.kind) {
        case TypeKind::TYPE_CSTRING:
            PRT().PVal("CString");
            return;
        case TypeKind::TYPE_POINTER:
            PRT().PVal("CPointer");
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "<", ">");
            return;
        case TypeKind::TYPE_CLASS:
        case TypeKind::TYPE_INTERFACE:
        case TypeKind::TYPE_STRUCT:
        case TypeKind::TYPE_ENUM:
            PRT().PVal(ty.name);
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "<", ">");
            return;
        case TypeKind::TYPE_TUPLE:
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "(", ")");
            return;
        case TypeKind::TYPE_GENERICS:
            PRT().PVal(ty.name);
            return;
        default:
            break;
    };
    PRT().PVal(ty.String());
}

// 解糖辅助函数
/**
 * @brief 尝试还原解糖后的调用表达式。
 */
bool ToSourcePass::TryRecoverCallExpr(const CallExpr& node)
{
    if (config.Sema()) {
        return TryPrintInitCall(node) || TryRecoverOverloadCallExpr(node) || TryRecoverPropCallExpr(node);
    }
    return false;
}

namespace {
/**
 * Try get the base func identifier.
 */
inline std::string TryGetCallRef(const CallExpr& node)
{
    if (node.baseFunc->astKind == AstKind::REF_EXPR) {
        return Cast<const RefExpr&>(node.baseFunc.get()).ref.identifier.Val();
    }
    return "";
}
/**
 * Check whether the call is a overload call.
 */
bool IsInitCall(const CallExpr& node)
{
    if (node.callKind == CallKind::CALL_STRUCT_CREATION || node.callKind == CallKind::CALL_OBJECT_CREATION ||
        // 处理编译器生成的错误类型节点
        node.callKind == CallKind::CALL_INVALID) {
        return TryGetCallRef(node) == "init";
    }
    return false;
}
/**
 * Check whether the call is a overload call.
 */
inline bool IsOverloadCall(const CallExpr& node)
{
    if (!node.resolvedFunction) {
        return false;
    }
    if (node.resolvedFunction->op == TokenKind::ILLEGAL) {
        return false;
    }
    return node.baseFunc->astKind == AstKind::MEMBER_ACCESS;
}

/**
 * Check whether the call is a prop call.
 */
inline bool IsPropCall(const CallExpr& node)
{
    if (!node.resolvedFunction) {
        return false;
    }
    return node.resolvedFunction->isGetter || node.resolvedFunction->isSetter;
}

inline Ptr<Ty> TryGetBaseTy(Ptr<Ty> ty)
{
    Ptr<Ty> res = ty;
    while (res->IsCoreOptionType()) {
        res = res->typeArgs[0];
    }
    return res;
}
} // namespace

/**
 * @brief 尝试还原构造函数调用表达式。
 */
bool ToSourcePass::TryPrintInitCall(const CallExpr& node)
{
    if (!IsInitCall(node)) {
        return false;
    }
    AH_CHECK_NULL(node.ty);
    // 构造函数调用 init() -> A(), TODO: 应该缩小下范围， 构造函数内的init不需要替换
    // 特殊场景处理: init() -> ??A 是解糖表达式
    PrintTy(*TryGetBaseTy(node.ty));
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")", true);
    return true;
}

/**
 * @brief 尝试打印解糖后重载调用表达式。
 *
 * 解糖后：a.[](x) or a.[](x, y) or a.+(b)
 * =========================
 * 还原为： a[x] or a[x] = y or a + v
 *
 * @param node 调用表达式节点的引用。
 */
bool ToSourcePass::TryRecoverOverloadCallExpr(const CallExpr& node)
{
    if (!IsOverloadCall(node)) {
        return false;
    }
    auto fn = node.resolvedFunction;
    auto op = fn->op;
    Logger::Get().Debug("ToSourcePass::TryRecoverOverloadCallExpr", "For Overload operator: ", Tk2Str(op));
    AH_ASSERT(IsOverloadCall(node));
    auto& ma = Cast<const MemberAccess&>(node.baseFunc.get());
    auto base = ma.baseExpr.get();
    // 可以重载的操作符有
    if (op == TokenKind::NOT) {
        // 一元 !
        TryPrintNode(base, "!");
    } else if (op == TokenKind::LSQUARE) {
        // []
        Traverse(*base, visitor);
        AH_ASSERT(node.args.size() == 1 || node.args.size() == 2);
        if (node.args.size() == 1) {
            // x[i]
            TryPrintNode(node.args[0].get(), "[", "]");
        } else {
            // x[i] = v
            TryPrintNode(node.args[0].get(), "[", "]");
            PRT().PVal(" = ");
            VisitNode(node.args[1]);
        }
    } else if (op == TokenKind::LPAREN) {
        // ()
        Traverse(*base, visitor);
        PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")");
    } else {
        // 二元： + - * / % ** << >> < <= > >= == != & ^ |
        Traverse(*base, visitor);
        AH_ASSERT(node.args.size() == 1);
        TryPrintNode(node.args[0].get(), " " + Tk2Str(op) + " ");
    }
    // 打印个注释在这里
    PRT().PVal(" /* Desugared ");
    Traverse(ma, visitor);
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")", true);
    PRT().PVal(" */ ");
    return true;
}

/**
 * @brief 尝试打印解糖后属性调用表达式。
 *
 * 解糖后: x.$aget() or $aget()
 * ==============================
 * 还原为: x.a or a
 *
 * @param node 调用表达式节点的引用。
 */
bool ToSourcePass::TryRecoverPropCallExpr(const CallExpr& node)
{
    if (!IsPropCall(node)) {
        return false;
    }
    Logger::Get().Debug("ToSourcePass::TryRecoverPropCallExpr", "For Property CallExpr");
    // TODO: 测试特殊的 prop call, 比如静态属性调用
    auto fn = node.resolvedFunction;
    auto propDecl = fn->propDecl;
    AH_CHECK_NULL(propDecl);
    bool isMem = node.baseFunc->astKind == AstKind::MEMBER_ACCESS;
    if (isMem) {
        auto& ma = Cast<const MemberAccess&>(node.baseFunc.get());
        TryPrintNode(ma.baseExpr.get(), "", ".");
    }
    PRT().PVal(Id(propDecl->identifier));
    if (fn->isSetter) {
        // setter
        AH_ASSERT(node.args.size() == 1);
        TryPrintNode(node.args[0], " = ");
    }
    return true;
}

/**
 * @brief 打印解糖后的 For-In 表达式（范围形式）。
 *
 * for (x in start..stop:step)
 * ============================
 * var $iter-i = startExpr
 * var $stop-compiler = stopExpr
 * while ($iter-i < $stop-compiler) {
 *     let i = $iter-i
 *     foo(i)
 *     $iter-i += stepExpr
 * }
 *
 * @param node For-In 表达式节点的引用。
 */
bool ToSourcePass::TryPrintDesugaredForInExpr(const ForInExpr& node)
{
    // TODO: 适配 默认 desugar false
    if (!config.Sema() || !config.Desugar()) {
        return false;
    }
    if (node.forInKind == ForInKind::FORIN_RANGE) {
        PrintDesugaredForInRange(node);
    } else if (node.forInKind == ForInKind::FORIN_STRING) {
        PrintDesugaredForInString(node);
    } else if (node.desugarExpr) {
        // Default: ForInKind::FORIN_ITER
        PrintDesugaredForInIterator(node);
    } else {
        return false;
    }
    return true;
}

/**
 * @brief 打印解糖后的 For-In 表达式（范围形式）。
 *
 * for (x in start..stop:step)
 * ============================
 * var $iter-i = startExpr
 * var $stop-compiler = stopExpr
 * while ($iter-i < $stop-compiler) {
 *     let i = $iter-i
 *     foo(i)
 *     $iter-i += stepExpr
 * }
 *
 * @param node For-In 表达式节点的引用。
 */
void ToSourcePass::PrintDesugaredForInRange(const ForInExpr& node)
{
    Logger::Get().Debug("ToSourcePass::PrintDesugaredForInRange", "For ForInExpr with Range");
    // ASSERT
    AH_CHECK_NULL(node.pattern);
    AH_ASSERT(node.pattern->astKind == AstKind::VAR_PATTERN);
    auto& varPat = Cast<const VarPattern&>(node.pattern.get());
    Ptr<Expr> inExpr = node.inExpression.get();
    AH_CHECK_NULL(inExpr);
    AH_ASSERT(inExpr->astKind == AstKind::BLOCK);
    auto& block = Cast<const Block&>(inExpr);
    AH_ASSERT(block.body.size() == 4);

    TryPrintNode(block.body[0], "", "", true); // var $iter-i = startExpr
    TryPrintNode(block.body[1], "", "", true); // var $stop-compiler = stopExpr
    TryPrintNode(block.body[3], "while (", ") {", true);
    PRT().Indent();
    TryPrintNode(varPat.varDecl, "", "", true); // let i = $iter-i;
    Traverse(*node.body, visitor);              // foo(i);
    TryPrintNode(block.body[2], "", "", true);  // $iter-i += stepExpr;
    PRT().Unindent();
    PRT().PVal("}");
}

/**
 * @brief 打印解糖后的 For-In 表达式（迭代器形式）。
 *
 * for (x in expr)
 * ============================
 * var $iter-compiler = expr.iterator()
 * while (true) {
 *    match (iter-compiler.next()) {
 *         case None: => break
 *         case Some(v-compiler): => match (v-compiler) {
 *            case v: Int64 => foo(v)
 *            case _ => continue
 *         }
 *    }
 * }
 *
 * @param node For-In 表达式节点的引用。
 */
void ToSourcePass::PrintDesugaredForInIterator(const ForInExpr& node)
{
    Logger::Get().Debug("ToSourcePass::PrintDesugaredForInIterator", "For ForInExpr with Iterator");
    AH_CHECK_NULL(node.desugarExpr);
    AH_ASSERT(node.desugarExpr->astKind == AstKind::BLOCK);
    auto& block = Cast<const Block&>(node.desugarExpr.get());
    AH_ASSERT(block.body.size() == 2);
    Traverse(block, visitor);
}

/**
 * @brief 打印解糖后的 For-In 表达式（字符串形式）。
 *
 * for (x in "hello")
 * ============================
 * var $iter-compiler = 0
 * let tmp1 = "hello"
 * let tmp2 = tmp1.$sizeget()
 * while ($iter-compiler < $tmp2) {
 *     let x = $tmp1[$iter-compiler]
 *     println(x)
 *     $iter-compiler = $iter-compiler + 1
 * }
 *
 * @param node For-In 表达式节点的引用。
 */
void ToSourcePass::PrintDesugaredForInString(const ForInExpr& node)
{
    Logger::Get().Debug("ToSourcePass::PrintDesugaredForInString", "For ForInExpr with String");
    AH_CHECK_NULL(node.pattern);
    AH_ASSERT(node.pattern->astKind == AstKind::VAR_PATTERN);
    auto& varPat = Cast<const VarPattern&>(node.pattern.get());
    Ptr<Expr> inExpr = node.inExpression.get();
    AH_CHECK_NULL(inExpr);
    AH_ASSERT(inExpr->astKind == AstKind::BLOCK);
    auto& block = Cast<const Block&>(inExpr);
    AH_ASSERT(block.body.size() == 3);

    TryPrintNode(block.body[0], "", "", true); // var $iter-compiler = 0
    TryPrintNode(block.body[1], "", "", true); // let tmp1 = "hello"
    TryPrintNode(block.body[2], "", "", true); // let tmp2 = tmp1.$sizeget()

    auto loopVar = desugaredVarId.at(Cast<VarDecl*>(block.body[0].get()));
    auto& stopVar = Cast<const VarDecl&>(block.body[2].get());
    PRT().PVals("while (", loopVar, " < ", Id(stopVar.identifier), ") {").PNL();
    PRT().Indent();
    TryPrintNode(varPat.varDecl, "", "", true); // let i = $iter-i;
    Traverse(*node.body, visitor);              // foo(i);
    PRT().PVals(loopVar, " = ", loopVar, " + 1").PNL();
    PRT().Unindent();
    PRT().PVal("}");
}

Printer& ToSourcePass::PRT()
{
    return prt;
}

/// ToSourcePassBuilder 实现方法
ToSourcePassBuilder& ToSourcePassBuilder::Output(const std::string& out)
{
    config.out = out;
    return *this;
}

ToSourcePassBuilder& ToSourcePassBuilder::Suffix(const std::string& suffix)
{
    config.suffix = suffix;
    return *this;
}

ToSourcePassBuilder& ToSourcePassBuilder::Indent(int indent)
{
    config.indent = indent;
    return *this;
}

ToSourcePassBuilder& ToSourcePassBuilder::EnableDesugar()
{
    config.flags |= PassConfig::DESUGAR_FLAG;
    return *this;
}

ToSourcePassBuilder& ToSourcePassBuilder::EnableSema()
{
    config.flags |= PassConfig::SEMA_FLAG;
    return *this;
}

namespace {
/**
 * @brief 将字符串键映射到AstKind
 */
const std::unordered_map<std::string, AstKind> key2DeclKind{{"func", AstKind::FUNC_DECL},
    {"class", AstKind::CLASS_DECL}, {"interface", AstKind::INTERFACE_DECL}, {"struct", AstKind::STRUCT_DECL},
    {"var", AstKind::VAR_DECL}};
} // namespace

ToSourcePassBuilder& ToSourcePassBuilder::Focus(const std::vector<std::string>& kinds)
{
    for (auto& kind : kinds) {
        config.focusDecls.insert(key2DeclKind.at(kind));
    }
    return *this;
}

/**
 * @brief 设置关注的注解属性。
 * @param attrs 关注的注解属性名称列表。
 */
ToSourcePassBuilder& ToSourcePassBuilder::FocusAnnotationAttrs(const std::vector<std::string>& attrs)
{
    config.focusAnnotationAttrs.insert(attrs.begin(), attrs.end());
    return *this;
}

/**
 * @brief 设置关注的修饰符属性。
 * @param attrs 关注的修饰符属性名称列表。
 */
ToSourcePassBuilder& ToSourcePassBuilder::FocusModifierAttrs(
    const std::vector<std::string>& attrs, const std::vector<std::string>& kinds)
{
    config.focusModifierAttrs.insert(attrs.begin(), attrs.end());
    for (auto& kind : kinds) {
        config.focusModifierWhiteList.insert(key2DeclKind.at(kind));
    }
    return *this;
}

/**
 * @brief 设置忽略的顶层声明。
 * @param decls 忽略的声明标识符列表。
 */
ToSourcePassBuilder& ToSourcePassBuilder::IgnoreDecls(const std::vector<std::string>& decls)
{
    config.ignoreDecls.insert(decls.begin(), decls.end());
    return *this;
}

/**
 * @brief 设置忽略的注解。
 * @param annos 忽略的注解名称列表。
 */
ToSourcePassBuilder& ToSourcePassBuilder::IgnoreAnnotations(const std::vector<std::string>& annos)
{
    config.ignoreAnnotations.insert(annos.begin(), annos.end());
    return *this;
}

std::unique_ptr<ToSourcePass> ToSourcePassBuilder::Build()
{
    return std::unique_ptr<ToSourcePass>(new ToSourcePass(config));
}
