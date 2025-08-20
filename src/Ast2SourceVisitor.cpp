/**
 * @file
 *
 * This file implementation of Ast2SourceVisitor.
 */
#include "Ast2SourceVisitor.h"
#include "Logger.h"
#include <filesystem>

namespace fs = std::filesystem;
using AstKind = Cangjie::AST::ASTKind;
using Cangjie::Identifier;
using Cangjie::TokenKind;
using Cangjie::AST::Attribute;
using Cangjie::AST::CallKind;
using Cangjie::AST::Expr;
using Cangjie::AST::ImportKind;
using Cangjie::AST::InheritableDecl;
using Cangjie::AST::Pattern;
using Cangjie::AST::Ty;
using Cangjie::AST::Type;

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
/**
 * TokenKind 映射字符串辅助函数
 */
inline std::string Tk2Str(Cangjie::TokenKind tk)
{
    return Cangjie::TOKENS[static_cast<int>(tk)];
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
/// Ast2SourceVisitor 实现函数
void Ast2SourceVisitor::Visit(const File& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For File imports: ", node.imports.size());
    std::string fp = out + "/" + GetFileNameWithoutSuffix(node.fileName) + suffix;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    // package declaration
    TryPrintNode(node.package.get());
    // import statements
    PRT().PVec<ImportSpec>(node.imports, [this](const ImportSpec& imp) { Traverse(imp, *this); }, "", "", "\n");
    // toplevel decls
    PRT().PVec<Decl>(node.decls, [this](const Decl& decl) {
        if (!IsFocused(decl)) {
            return;
        }
        Traverse(decl, *this);
        PRT().PNL(2);
    });
    ofs.close();
}

void Ast2SourceVisitor::Visit(const PackageSpec& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For PackageSpec: ", node.packageName.Val());
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

void Ast2SourceVisitor::Visit(const ImportSpec& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ImportSpec");
    if (IsDuplicatedImport(node)) {
        // Import multi-packages is desugared as serveral import single-packages, import std.core.* is implicit import!
        return;
    }
    VisitNodes(node.annotations);
    TryPrintNode(node.modifier.get(), "", " ");
    PRT().PVal("import ");
    Traverse(node.content, *this);
    PRT().PNL();
}

void Ast2SourceVisitor::Visit(const ImportContent& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ImportContent");
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

void Ast2SourceVisitor::Visit(const Annotation& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Annotation");
    PRT().PVals("@", Id(node.identifier));
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "[", "]");
    PRT().PNL();
}

void Ast2SourceVisitor::Visit(const Modifier& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Modifier");
    PRT().PVal(Tk2Str(node.modifier));
}

// Decls
namespace {
using Cangjie::AST::VarDeclAbstract;
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

void Ast2SourceVisitor::Visit(const VarDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarDecl: ", node.identifier.Val());
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

void Ast2SourceVisitor::Visit(const VarWithPatternDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarWithPatternDecl: ", node.identifier.Val());
    PrintDecl(node);
    TryPrintNode(node.irrefutablePattern.get(), GetVarKeyword(node) + " ");
    PrintVarType(node);
    TryPrintNode(node.initializer.get(), " = ");
}

void Ast2SourceVisitor::Visit(const PropDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For PropDecl: ", node.identifier.Val());
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

void Ast2SourceVisitor::Visit(const FuncParam& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParam");
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

void Ast2SourceVisitor::Visit(const FuncParamList& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParamList: ", node.params.size());
    PRT().PVec<FuncParam>(
        node.params, [this](const FuncParam& param) { Traverse(param, *this); }, ", ", "(", ")", true);
}

namespace {
inline Ptr<Ty> TryGetRetTy(Ptr<Ty> ty)
{
    if (ty->IsFunc()) {
        return static_cast<Cangjie::AST::FuncTy*>(ty.get())->retTy;
    }
    return nullptr;
}
} // namespace

void Ast2SourceVisitor::Visit(const FuncBody& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncBody");
    TryPrintGenericParams(node.generic.get());
    VisitNode(node.paramLists[0]);
    if (!TryPrintType(node.retType) && OpenSema()) {
        TryPrintTy(TryGetRetTy(node.ty));
    }
    TryPrintGenericConstraints(node.generic);
    PrintBlock(node.body);
}

void Ast2SourceVisitor::Visit(const FuncDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncDecl: ", node.identifier.Val());
    PrintDecl(node);
    if (TryPrintEnumConstructor(node) || TryPrintConstructor(node) || TryPrintGetter(node) || TryPrintSetter(node)) {
        return;
    }
    // General func.
    PRT().PVals("func ", Id(node.identifier));
    TryPrintNode(node.funcBody);
}

void Ast2SourceVisitor::Visit(const MainDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MainDecl");
    if (OpenDesugar() && node.desugarDecl) {
        Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Desugared Decl of MainDecl");
        if (node.TestAttr(Attribute::UNSAFE)) {
            PRT().PVal("unsafe ");
        }
        VisitNode(node.desugarDecl);
    } else {
        PrintDecl(node);
        PRT().PVal("main");
        AH_CHECK_NULL(node.funcBody);
        Visit(*node.funcBody, res);
    }
}

void Ast2SourceVisitor::Visit(const PrimaryCtorDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For PrimaryCtorDecl: ", node.identifier.Val());
    if (OpenSema()) {
        return; // PrimaryCtorDecl is desugared in Sema
    }
    PrintDecl(node);
    PRT().PVal(Id(node.identifier));
    AH_CHECK_NULL(node.funcBody);
    TryPrintNode(node.funcBody.get());
}

void Ast2SourceVisitor::Visit(const ClassDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ClassDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "class");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void Ast2SourceVisitor::Visit(const InterfaceDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For InterfaceDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "interface");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void Ast2SourceVisitor::Visit(const StructDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For StructDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "struct");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void Ast2SourceVisitor::Visit(const EnumDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For EnumDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "enum");
    PRT().PValNL(" {").Indent();
    PRT().PVec<Decl>(node.constructors, [this](const Decl& decl) {
        PRT().PVal("| ");
        Traverse(decl, *this);
        PRT().PNL();
    });
    PRT().PNL();
    PrintDecls(node.members);
    PRT().Unindent();
    PRT().PVal("}");
}

void Ast2SourceVisitor::Visit(const ExtendDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ExtendDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("extend");
    TryPrintGenericParams(node.generic.get());
    TryPrintNode(node.extendedType.get(), " ");
    PrintInheritedTypes(node.inheritedTypes);
    TryPrintGenericConstraints(node.generic.get());
    PrintInheritableDeclBody(node.members);
}

void Ast2SourceVisitor::Visit(const TypeAliasDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For TypeAliasDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("type ");
    PRT().PVal(Id(node.identifier));
    TryPrintNode(node.type.get(), " = ");
}

// Type
void Ast2SourceVisitor::Visit(const PrimitiveType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For PrimitiveType");
    PRT().PVal(node.str);
}

void Ast2SourceVisitor::Visit(const RefType& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For RefType");
    if (node.ref.identifier.Val() == "" && OpenSema() && node.ty) {
        PrintTy(*node.ty);
    } else {
        PRT().PVal(Id(node.ref.identifier));
        PRT().PVec<AstNode>(node.typeArguments, [this](const AstNode& node) { Traverse(node, *this); }, ", ", "<", ">");
    }
}

void Ast2SourceVisitor::Visit(const OptionType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For OptionType");
    std::string preQuest(node.questNum, '?');
    TryPrintNode(node.componentType.get(), preQuest);
}

void Ast2SourceVisitor::Visit(const TupleType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For TupleType");
    PRT().PVec<Type>(node.fieldTypes, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "(", ")");
}

void Ast2SourceVisitor::Visit(const QualifiedType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For QualifiedType");
    VisitNode(node.baseType);
    PRT().PVals(".", Id(node.field));
    PRT().PVec<Type>(node.typeArguments, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "<", ">");
}

void Ast2SourceVisitor::Visit(const ThisType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ThisType");
    PRT().PVal("This");
}

void Ast2SourceVisitor::Visit(const VArrayType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VArrayType");
    PRT().PVal("VArray<");
    VisitNode(node.typeArgument);
    PRT().PVal(", ");
    VisitNode(node.constantType);
    PRT().PVal(">");
}

void Ast2SourceVisitor::Visit(const ParenType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ParenType");
    TryPrintNode(node.type.get(), "(", ")");
}

void Ast2SourceVisitor::Visit(const ConstantType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ConstantType");
    TryPrintNode(node.constantExpr.get(), "$");
}

void Ast2SourceVisitor::Visit(const FuncType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncType");
    PRT().PVec<Type>(node.paramTypes, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "(", ")", true);
    TryPrintNode(node.retType.get(), " -> ");
}

// Pattern
void Ast2SourceVisitor::Visit(const WildcardPattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For WildcardPattern");
    PRT().PVal("_");
}

void Ast2SourceVisitor::Visit(const ConstPattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ConstPattern");
    VisitNode(node.literal);
}

void Ast2SourceVisitor::Visit(const EnumPattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For EnumPattern");

    AH_CHECK_NULL(node.constructor);
    // RefExpr 单独处理
    auto ctor = node.constructor.get();
    if (ctor->astKind == AstKind::REF_EXPR) {
        auto refExpr = static_cast<RefExpr*>(ctor.get());
        PRT().PVal(Id(refExpr->ref.identifier));
        PrintInstArgs(*refExpr, true);
    } else {
        VisitNode(ctor);
    }
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, *this); }, ", ", "(", ")");
}

void Ast2SourceVisitor::Visit(const VarPattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarPattern", node.varDecl->identifier.Val());
    PRT().PVal(Id(node.varDecl->identifier));
}

void Ast2SourceVisitor::Visit(const VarOrEnumPattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarOrEnumPattern: ", node.identifier.Val());
    PRT().PVal(Id(node.identifier));
    // pattern 是 解糖后才有的？
    // VisitNode(node.pattern);
}

void Ast2SourceVisitor::Visit(const TypePattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For TypePattern");
    VisitNode(node.pattern);
    TryPrintType(node.type.get());
}

void Ast2SourceVisitor::Visit(const TuplePattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For TuplePattern");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, *this); }, ", ", "(", ")");
}

// Expr
void Ast2SourceVisitor::Visit(const Block& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Block: ", node.body.size());
    PRT().PVec<AstNode>(node.body, [this](const AstNode& node) {
        auto res = Traverse(node, *this);
        if (res.status) {
            PRT().PNL();
        }
    });
}

void Ast2SourceVisitor::Visit(const RefExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For RefExpr: ", node.ref.identifier.Val());
    if (TryPrintDesugaredRef(node)) {
        return;
    }
    PRT().PVal(Id(node.ref.identifier));
    PrintInstArgs(node);
}

void Ast2SourceVisitor::Visit(const FuncArg& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncArg");
    VisitNode(node.expr);
}

void Ast2SourceVisitor::Visit(const CallExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For CallExpr");
    if (TryRecoverCallExpr(node)) {
        return;
    }
    // General func call
    VisitNode(node.baseFunc);
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")", true);
}

void Ast2SourceVisitor::Visit(const ReturnExpr& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ReturnExpr");
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

void Ast2SourceVisitor::Visit(const LitConstExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For LitConstExpr");
    PRT().PVal(node.ToString());
}

void Ast2SourceVisitor::Visit(const ArrayLit& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ArrayLit");
    PRT().PVec<AstNode>(node.children, [this](const AstNode& expr) { Traverse(expr, *this); }, ", ", "[", "]", true);
}

void Ast2SourceVisitor::Visit(const TupleLit& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For TupleLit");
    PRT().PVec<AstNode>(node.children, [this](const AstNode& expr) { Traverse(expr, *this); }, ", ", "(", ")", true);
}

namespace {
// 检查 expr 是否是对 Enum 类型的引用
inline bool IsRefEnum(const Expr& expr)
{
    if (expr.astKind != AstKind::REF_EXPR) {
        return false;
    }
    return static_cast<const RefExpr*>(&expr)->ref.target->astKind == AstKind::ENUM_DECL;
}
} // namespace

void Ast2SourceVisitor::Visit(const MemberAccess& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MemberAccess");
    VisitNode(node.baseExpr);
    PRT().PVals(".", Id(node.field));
    if (!OpenSema() || !IsRefEnum(*node.baseExpr)) {
        PrintInstArgs(node, node.isPattern);
    }
}

void Ast2SourceVisitor::Visit(const LambdaExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For LambdaExpr");
    auto& body = *node.funcBody;
    PRT().PVal("{");
    AH_ASSERT(body.paramLists.size() == 1);
    auto& params = body.paramLists[0]->params;
    PRT().PVec<FuncParam>(params, [this](const FuncParam& param) { Traverse(param, *this); }, ", ", " ");
    PRT().PWI([this, &body] { VisitNode(body.body); }, " =>", "}");
}

void Ast2SourceVisitor::Visit(const MatchCase& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchCase");
    PRT().PVal("case ");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, *this); }, " | ");
    TryPrintNode(node.patternGuard.get(), " where ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void Ast2SourceVisitor::Visit(const MatchCaseOther& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchCaseOther");
    TryPrintNode(node.matchExpr.get(), "case ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void Ast2SourceVisitor::Visit(const MatchExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchExpr");
    PRT().PVal("match ");
    // match with selector
    TryPrintNode(node.selector.get(), "(", ")");
    PRT().PValNL(" {").Indent();
    PRT().PVec<MatchCase>(node.matchCases, [this](const MatchCase& mc) { Traverse(mc, *this); });
    PRT().PVec<MatchCaseOther>(node.matchCaseOthers, [this](const MatchCaseOther& mco) { Traverse(mco, *this); });
    PRT().Unindent();
    PRT().PVal("}");
}

void Ast2SourceVisitor::Visit(const IsExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For IsExpr");
    VisitNode(node.leftExpr);
    PRT().PVal(" is ");
    VisitNode(node.isType);
}

void Ast2SourceVisitor::Visit(const AsExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For AsExpr");
    VisitNode(node.leftExpr);
    PRT().PVal(" as ");
    VisitNode(node.asType);
}

void Ast2SourceVisitor::Visit(const AssignExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For AssignExpr");
    if (OpenDesugar() && node.desugarExpr) {
        Traverse(*node.desugarExpr, *this);
    } else {
        VisitNode(node.leftValue);
        PRT().PVals(" ", Tk2Str(node.op), " ");
        VisitNode(node.rightExpr);
    }
}

void Ast2SourceVisitor::Visit(const IncOrDecExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For IncOrDecExpr");
    TryPrintNode(node.expr.get(), "", Tk2Str(node.op));
}

void Ast2SourceVisitor::Visit(const UnaryExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For UnaryExpr");
    TryPrintNode(node.expr.get(), Tk2Str(node.op));
}

void Ast2SourceVisitor::Visit(const BinaryExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For BinaryExpr");
    VisitNode(node.leftExpr);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void Ast2SourceVisitor::Visit(const ThrowExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ThrowExpr");
    PRT().PVal("throw ");
    VisitNode(node.expr);
}

void Ast2SourceVisitor::Visit(const SubscriptExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For SubscriptExpr");
    if (OpenDesugar() && node.desugarExpr) {
        Traverse(*node.desugarExpr, *this);
    } else {
        VisitNode(node.baseExpr);
        PRT().PVec<Expr>(node.indexExprs, [this](const Expr& expr) { Traverse(expr, *this); }, ", ", "[", "]");
    }
}

void Ast2SourceVisitor::Visit(const JumpExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For JumpExpr");
    if (node.isBreak) {
        PRT().PVal("break");
    } else {
        PRT().PVal("continue");
    }
}

void Ast2SourceVisitor::Visit(const RangeExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For RangeExpr");
    TryPrintNode(node.startExpr.get());
    PRT().PVal("..");
    TryPrintNode(node.stopExpr.get());
    TryPrintNode(node.stepExpr.get(), " : ");
}

void Ast2SourceVisitor::Visit(const LetPatternDestructor& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For LetPatternDestructor");
    PRT().PVal("let ");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, *this); }, " | ");
    TryPrintNode(node.initializer, " <- ");
}

void Ast2SourceVisitor::Visit(const IfExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For IfExpr");
    PRT().PVal("if ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.thenBody); }, " {", "}");
    TryPrintNode(node.elseBody.get(), " else ");
}

void Ast2SourceVisitor::Visit(const WhileExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For WhileExpr");
    PRT().PVal("while ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.body); }, " {", "}");
}

void Ast2SourceVisitor::Visit(const DoWhileExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For DoWhileExpr");
    PRT().PWI([this, &node] { VisitNode(node.body); }, "do {", "} while ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PNL();
}

void Ast2SourceVisitor::Visit(const ForInExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ForInExpr");
    if (TryPrintDesugaredForInExpr(node)) {
        return;
    }
    TryPrintNode(node.pattern.get(), "for (", " in ");
    TryPrintNode(node.inExpression.get());
    TryPrintNode(node.patternGuard.get());
    PRT().PWI([this, &node] { VisitNode(node.body); }, ") {", "}");
}

// Generic
void Ast2SourceVisitor::Visit(const Generic& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Generic");
    PRT().PVec<GenericParamDecl>(
        node.typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, *this); }, ", ", "<", ">");
    PRT().PVec<GenericConstraint>(
        node.genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, *this); }, ", ", " where ");
}

void Ast2SourceVisitor::Visit(const GenericParamDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For GenericParamDecl");
    PRT().PVal(Id(node.identifier));
}

void Ast2SourceVisitor::Visit(const GenericConstraint& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For GenericConstraint");
    VisitNode(node.type);
    PRT().PVal(" <: ");
    PRT().PVec<Type>(node.upperBounds, [this](const Type& tp) { Traverse(tp, *this); }, " & ");
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

Ast2SourceVisitor::Ast2SourceVisitor(
    const std::string& out, const std::string& suffix, int indent, Flag flags, std::unordered_set<AstKind>&& focusDecls)
    : out(out), prt(ofs, indent), suffix(suffix), flags(flags), focusDecls(std::move(focusDecls))
{
    CreateDirIfNotExists(out);
}

// 辅助打印函数 (重命名 PrintXXX)
/**
 * @brief 打印节点。
 * @param pnode 节点指针。
 * @param pre 前缀字符串。
 * @param suf 后缀字符串。
 * @param withNL 是否追加空行。
 */
inline void Ast2SourceVisitor::TryPrintNode(
    const Ptr<AstNode> pnode, const std::string& pre, const std::string& suf, bool withNL)
{
    if (!pnode) {
        return;
    }
    PRT().PVal(pre);
    Traverse(*pnode, *this);
    PRT().PVal(suf);
    if (withNL) {
        PRT().PNL();
    }
}

/**
 * @brief 辅助打印声明节点。
 * @param node 声明节点的引用。
 */
void Ast2SourceVisitor::PrintDecl(const Decl& node)
{
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    PRT().PVec<Modifier>(node.modifiers, [this](const Modifier& mod) { Traverse(mod, *this); }, " ", "", " ");
}

/**
 * @brief 辅助打印代码块。
 * @param pnode : Block节点
 */
inline void Ast2SourceVisitor::PrintBlock(const Ptr<Block> pnode)
{
    if (!pnode) {
        return;
    }
    PRT().PWI([this, pnode] { Traverse(*pnode, *this); }, " {", "}");
}

/**
 * @brief 尝试作为构造函数打印。
 * @param decl 声明的引用: FuncDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool Ast2SourceVisitor::TryPrintConstructor(const FuncDecl& node)
{
    if (!node.TestAttr(Attribute::CONSTRUCTOR)) {
        return false;
    }
    Logger::Get().Debug("Ast2SourceVisitor::TryPrintConstructor", "For FuncDecl is constructor");
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
bool Ast2SourceVisitor::TryPrintGetter(const FuncDecl& node)
{
    if (!node.isGetter) {
        return false;
    }
    Logger::Get().Debug("Ast2SourceVisitor::TryPrintGetter", "For FuncDecl is getter");
    PRT().PVal("get()");
    PrintBlock(node.funcBody->body);
    return true;
}
/**
 * @brief 尝试作为setter打印。
 * @param decl 枚举声明的引用: VarDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool Ast2SourceVisitor::TryPrintSetter(const FuncDecl& node)
{
    if (!node.isSetter) {
        return false;
    }
    Logger::Get().Debug("Ast2SourceVisitor::TryPrintSetter", "For FuncDecl is setter");
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
bool Ast2SourceVisitor::TryPrintEnumConstructor(const VarDecl& node)
{
    if (!node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    Logger::Get().Debug("Ast2SourceVisitor::TryPrintEnumConstructor", "For VarDecl as EnumConstructor");
    PRT().PVal(node.identifier.Val());
    return true;
}

/**
 * @brief 尝试作为Enum构造器打印。
 * @param decl 枚举声明的引用: FuncDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool Ast2SourceVisitor::TryPrintEnumConstructor(const FuncDecl& node)
{
    if (!node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    Logger::Get().Debug("Ast2SourceVisitor::TryPrintEnumConstructor", "For FuncDecl as EnumConstructor");
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
inline void Ast2SourceVisitor::PrintInheritedTypes(const std::vector<OwnedPtr<Type>>& types)
{
    PRT().PVec<Type>(types, [this](const Type& ty) { Traverse(ty, *this); }, " & ", " <: ");
}

/**
 * @brief 辅助打印可继承类型头部。
 * @param node 声明引用 (可继承类型: Class, Struct, Interface, Enum)
 */
void Ast2SourceVisitor::PrintInheritableDeclHeader(const InheritableDecl& node, const std::string& keyword)
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
inline void Ast2SourceVisitor::PrintDecls(const std::vector<OwnedPtr<Decl>>& decls)
{
    PRT().PVec<Decl>(decls, [this](const Decl& decl) {
        Traverse(decl, *this);
        PRT().PNL(2);
    });
}

/**
 * @brief 辅助打印可继承类型定义体。
 */
void Ast2SourceVisitor::PrintInheritableDeclBody(const std::vector<OwnedPtr<Decl>>& members)
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
void Ast2SourceVisitor::TryPrintGenericParams(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For GenericParams");
    PRT().PVec<GenericParamDecl>(
        generic->typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, *this); }, ", ", "<", ">");
}

/**
 * @brief 辅助打印泛型约束。
 * @param generic 泛型节点指针。
 */
void Ast2SourceVisitor::TryPrintGenericConstraints(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For GenericConstraints");
    PRT().PVec<GenericConstraint>(
        generic->genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, *this); }, ", ", " where ");
}

/**
 * @brief 尝试打印解糖的RefExpr节点。
 */
bool Ast2SourceVisitor::TryPrintDesugaredRef(const RefExpr& ref)
{
    if (!OpenDesugar() || !OpenSema()) {
        return false;
    }
    auto target = ref.ref.target;
    if (auto it = desugaredVarId.find(target); it != desugaredVarId.end()) {
        PRT().PVal(it->second);
        return true;
    }
    return false;
}

/**
 * @brief 尝试打印泛型实例参数。
 * @param ref 引用表达式: RefExpr or MemberAccess
 * @param isPattern 是否是在 pattern 中 (enum pattern 不允许打印泛型参数)
 */
void Ast2SourceVisitor::PrintInstArgs(const Cangjie::AST::NameReferenceExpr& ref, bool isPattern)
{
    if (!ref.typeArguments.empty()) {
        PRT().PVec<Type>(ref.typeArguments, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "<", ">");
    } else if (OpenSema() && !isPattern) {
        PRT().PVec<Ty>(ref.instTys, [this](const Ty& ty) { PrintTy(ty); }, ", ", "<", ">");
    }
}

/**
 * @brief 辅助打印 变量的类型标注。
 */
inline void Ast2SourceVisitor::PrintVarType(const VarDeclAbstract& node)
{
    if (!TryPrintType(node.type.get()) && OpenSema()) {
        TryPrintTy(node.ty);
    }
}

/**
 * @brief 辅助打印 Type 节点
 * @param type 类型节点指针。
 */
bool Ast2SourceVisitor::TryPrintType(const Ptr<Type> type)
{
    if (!type) {
        return false;
    }
    if (type->astKind != AstKind::TYPE) {
        // 合法的 Type 语法节点
        PRT().PVal(": ");
        Traverse(*type, *this);
        return true;
    }
    if (OpenSema()) {
        return TryPrintTy(type->ty);
    }
    return false;
}

/**
 * @brief 辅助打印 Ty 标注。
 */
inline bool Ast2SourceVisitor::TryPrintTy(const Ptr<Ty> ty)
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
void Ast2SourceVisitor::PrintTy(const Ty& ty)
{
    using TyKind = Cangjie::AST::TypeKind;
    // If the format is incorrect, need to adjust it.
    switch (ty.kind) {
        case TyKind::TYPE_CSTRING:
            PRT().PVal("CString");
            return;
        case TyKind::TYPE_POINTER:
            PRT().PVal("CPointer");
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "<", ">");
            return;
        case TyKind::TYPE_CLASS:
        case TyKind::TYPE_INTERFACE:
        case TyKind::TYPE_STRUCT:
        case TyKind::TYPE_ENUM:
            PRT().PVal(ty.name);
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "<", ">");
            return;
        case TyKind::TYPE_TUPLE:
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "(", ")");
            return;
        case TyKind::TYPE_GENERICS:
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
bool Ast2SourceVisitor::TryRecoverCallExpr(const CallExpr& node)
{
    if (OpenDesugar() && OpenSema()) {
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
        return static_cast<RefExpr*>(node.baseFunc.get().get())->ref.identifier.Val();
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
bool Ast2SourceVisitor::TryPrintInitCall(const CallExpr& node)
{
    if (!IsInitCall(node)) {
        return false;
    }
    AH_CHECK_NULL(node.ty);
    // 构造函数调用 init() -> A(), TODO: 应该缩小下范围， 构造函数内的init不需要替换
    // 特殊场景处理: init() -> ??A 是解糖表达式
    PrintTy(*TryGetBaseTy(node.ty));
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")", true);
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
bool Ast2SourceVisitor::TryRecoverOverloadCallExpr(const CallExpr& node)
{
    if (!IsOverloadCall(node)) {
        return false;
    }
    auto fn = node.resolvedFunction;
    auto op = fn->op;
    Logger::Get().Debug("Ast2SourceVisitor::TryRecoverOverloadCallExpr", "For Overload operator: ", Tk2Str(op));
    AH_ASSERT(IsOverloadCall(node));
    auto ma = static_cast<MemberAccess*>(node.baseFunc.get().get());
    auto base = ma->baseExpr.get();
    // 可以重载的操作符有
    if (op == TokenKind::NOT) {
        // 一元 !
        TryPrintNode(base, "!");
    } else if (op == TokenKind::LSQUARE) {
        // []
        Traverse(*base, *this);
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
        Traverse(*base, *this);
        PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")");
    } else {
        // 二元： + - * / % ** << >> < <= > >= == != & ^ |
        Traverse(*base, *this);
        AH_ASSERT(node.args.size() == 1);
        TryPrintNode(node.args[0].get(), " " + Tk2Str(op) + " ");
    }
    // 打印个注释在这里
    PRT().PVal(" /* Desugared ");
    TryPrintNode(ma);
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")", true);
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
bool Ast2SourceVisitor::TryRecoverPropCallExpr(const CallExpr& node)
{
    if (!IsPropCall(node)) {
        return false;
    }
    Logger::Get().Debug("Ast2SourceVisitor::TryRecoverPropCallExpr", "For Property CallExpr");
    // TODO: 测试特殊的 prop call, 比如静态属性调用
    auto fn = node.resolvedFunction;
    auto propDecl = fn->propDecl;
    AH_CHECK_NULL(propDecl);
    bool isMem = node.baseFunc->astKind == AstKind::MEMBER_ACCESS;
    if (isMem) {
        auto ma = static_cast<MemberAccess*>(node.baseFunc.get().get());
        TryPrintNode(ma->baseExpr.get(), "", ".");
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
bool Ast2SourceVisitor::TryPrintDesugaredForInExpr(const ForInExpr& node)
{
    if (!OpenSema()) {
        return false;
    }
    using Cangjie::AST::ForInKind;
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
void Ast2SourceVisitor::PrintDesugaredForInRange(const ForInExpr& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::PrintDesugaredForInRange", "For ForInExpr with Range");
    // ASSERT
    AH_CHECK_NULL(node.pattern);
    AH_ASSERT(node.pattern->astKind == AstKind::VAR_PATTERN);
    Ptr<VarPattern> varPat = static_cast<VarPattern*>(node.pattern.get().get());
    Ptr<Expr> inExpr = node.inExpression.get();
    AH_CHECK_NULL(inExpr);
    AH_ASSERT(inExpr->astKind == AstKind::BLOCK);
    Ptr<Block> block = static_cast<Block*>(inExpr.get());
    AH_ASSERT(block->body.size() == 4);

    TryPrintNode(block->body[0].get(), "", "", true); // var $iter-i = startExpr
    TryPrintNode(block->body[1].get(), "", "", true); // var $stop-compiler = stopExpr
    TryPrintNode(block->body[3].get(), "while (", ") {", true);
    PRT().Indent();
    TryPrintNode(varPat->varDecl.get(), "", "", true); // let i = $iter-i;
    Traverse(*node.body, *this);                       // foo(i);
    TryPrintNode(block->body[2].get(), "", "", true);  // $iter-i += stepExpr;
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
void Ast2SourceVisitor::PrintDesugaredForInIterator(const ForInExpr& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::PrintDesugaredForInIterator", "For ForInExpr with Iterator");
    AH_CHECK_NULL(node.desugarExpr);
    AH_ASSERT(node.desugarExpr->astKind == AstKind::BLOCK);
    Ptr<Block> block = static_cast<Block*>(node.desugarExpr.get().get());
    AH_ASSERT(block->body.size() == 2);
    Traverse(*block, *this);
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
void Ast2SourceVisitor::PrintDesugaredForInString(const ForInExpr& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::PrintDesugaredForInString", "For ForInExpr with String");
    AH_CHECK_NULL(node.pattern);
    AH_ASSERT(node.pattern->astKind == AstKind::VAR_PATTERN);
    Ptr<VarPattern> varPat = static_cast<VarPattern*>(node.pattern.get().get());
    Ptr<Expr> inExpr = node.inExpression.get();
    AH_CHECK_NULL(inExpr);
    AH_ASSERT(inExpr->astKind == AstKind::BLOCK);
    Ptr<Block> block = static_cast<Block*>(inExpr.get());
    AH_ASSERT(block->body.size() == 3);

    TryPrintNode(block->body[0].get(), "", "", true); // var $iter-compiler = 0
    TryPrintNode(block->body[1].get(), "", "", true); // let tmp1 = "hello"
    TryPrintNode(block->body[2].get(), "", "", true); // let tmp2 = tmp1.$sizeget()

    auto loopVar = desugaredVarId.at(static_cast<VarDecl*>(block->body[0].get().get()));
    auto stopVar = static_cast<VarDecl*>(block->body[2].get().get());
    PRT().PVals("while (", loopVar, " < ", Id(stopVar->identifier), ") {").PNL();
    PRT().Indent();
    TryPrintNode(varPat->varDecl.get(), "", "", true); // let i = $iter-i;
    Traverse(*node.body, *this);                       // foo(i);
    PRT().PVals(loopVar, " = ", loopVar, " + 1").PNL();
    PRT().Unindent();
    PRT().PVal("}");
}

inline bool Ast2SourceVisitor::OpenDesugar() const
{
    return flags & DESUGAR_FLAG;
}

inline bool Ast2SourceVisitor::OpenSema() const
{
    return flags & SEMA_FLAG;
}

bool Ast2SourceVisitor::IsFocused(const Decl& decl) const
{
    if (focusDecls.empty()) {
        return true;
    }
    return focusDecls.find(decl.astKind) != focusDecls.end();
}

Printer& Ast2SourceVisitor::PRT()
{
    return prt;
}

/// Ast2SourceVisitorBuilder 实现方法
Ast2SourceVisitorBuilder& Ast2SourceVisitorBuilder::Output(const std::string& out)
{
    this->out = out;
    return *this;
}

Ast2SourceVisitorBuilder& Ast2SourceVisitorBuilder::Suffix(const std::string& suffix)
{
    this->suffix = suffix;
    return *this;
}

Ast2SourceVisitorBuilder& Ast2SourceVisitorBuilder::Indent(int indent)
{
    this->indent = indent;
    return *this;
}

Ast2SourceVisitorBuilder& Ast2SourceVisitorBuilder::EnableDusgar()
{
    this->flags |= DESUGAR_FLAG;
    return *this;
}

Ast2SourceVisitorBuilder& Ast2SourceVisitorBuilder::EnableSeam()
{
    this->flags |= SEMA_FLAG;
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

Ast2SourceVisitorBuilder& Ast2SourceVisitorBuilder::Focus(const std::vector<std::string>& kinds)
{
    for (auto& kind : kinds) {
        focusDecls.insert(key2DeclKind.at(kind));
    }
    return *this;
}

Ast2SourceVisitor Ast2SourceVisitorBuilder::Build()
{
    return Ast2SourceVisitor(out, suffix, indent, flags, std::move(focusDecls));
}
