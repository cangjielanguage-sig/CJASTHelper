/**
 * @file
 *
 * This file implements the ToCangjiePass.
 */
#include "ToCangjiePass.h"

REG_PASS("to-cangjie", ([](const PassConfig& config) {
    return UniquePtr<Pass>(new ToCangjiePass{Cast<const ToSourcePassConfig&>(config)});
}));

/// ToCangjiePass 实现函数
namespace {
// 私有辅助函数
void replaceAll(Str& str, ConStr& from, ConStr& to)
{
    if (from.empty()) {
        return;
    }
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != Str::npos) {
        str.replace(start_pos, from.length(), to);
        // 这里+to.length()是为了防止to中有重叠部分，例如从"aa"替换成"a"
        start_pos += to.length();
    }
}
/**
 * 判断是否是 自动生成的临时变量名 (可能重复， 比如迭代器变量)
 */
inline bool MaybeRepated(ConStr& id)
{
    static StrVec keys{"$iter-", "$stop-compiler", "iter-compiler"};
    for (auto& key : keys) {
        if (id.find(key) != Str::npos) {
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
inline void NormalizedId(Str& id)
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
inline Str Id(const Identifier& id)
{
    Str res = id.Val();
    NormalizedId(res);
    return res;
}
} // namespace

void ToCangjiePass::Visit(const File& node, VisitResult&)
{
    DEBUG("For File imports: ", node.imports.size());
    Str fp = Config().out + "/" + FileName(node.fileName) + Config().suffix;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        ERROR("ToCangjiePass try to open file: " + fp + " failed!");
        throw std::logic_error("ToCangjiePass try to open file: " + fp + " failed!");
    }
    // package declaration
    TryPrintNode(node.package.get());
    // import statements
    PRT().PVec<ImportSpec>(
        node.imports, [this](const ImportSpec& imp) { Traverse(imp, visitor); }, "", "", "\n");
    // toplevel decls
    PRT().PVec<Decl>(node.decls, [this](const Decl& decl) {
        if (!Config().Focus(decl)) {
            return;
        }
        Traverse(decl, visitor);
        PRT().PNL(2);
    });
    PRT().Flush();
    ofs.close();
}

void ToCangjiePass::Visit(const PackageSpec& node, VisitResult&)
{
    DEBUG("For PackageSpec: ", node.packageName.Val());
    TryPrintNode(node.modifier.get(), "", " ");
    PRT().PVal("package ");
    PRT().PVec<Str>(
        node.prefixPaths, [this](ConStr& pre) { PRT().PVal(pre); }, ".", "", ".");
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

void ToCangjiePass::Visit(const ImportSpec& node, VisitResult&)
{
    DEBUG("For ImportSpec");
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

void ToCangjiePass::Visit(const ImportContent& node, VisitResult&)
{
    DEBUG("For ImportContent");
    PRT().PVec<Str>(
        node.prefixPaths, [this](ConStr& pre) { PRT().PVal(pre); }, ".", "", ".");
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

void ToCangjiePass::Visit(const Annotation& node, VisitResult&)
{
    DEBUG("For Annotation");
    PRT().PVals("@", Id(node.identifier));
    PRT().PVec<FuncArg>(
        node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "[", "]");
    PRT().PNL();
}

void ToCangjiePass::Visit(const Modifier& node, VisitResult&)
{
    DEBUG("For Modifier");
    PRT().PVal(Tk2Str(node.modifier));
}

// Decls
namespace {
inline Str GetVarKeyword(const VarDeclAbstract& node)
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

void ToCangjiePass::Visit(const VarDecl& node, VisitResult&)
{
    DEBUG("For VarDecl: ", node.identifier.Val());
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

void ToCangjiePass::Visit(const VarWithPatternDecl& node, VisitResult&)
{
    DEBUG("For VarWithPatternDecl: ", node.identifier.Val());
    PrintDecl(node);
    TryPrintNode(node.irrefutablePattern.get(), GetVarKeyword(node) + " ");
    PrintVarType(node);
    TryPrintNode(node.initializer.get(), " = ");
}

void ToCangjiePass::Visit(const PropDecl& node, VisitResult&)
{
    DEBUG("For PropDecl: ", node.identifier.Val());
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

void ToCangjiePass::Visit(const FuncParam& node, VisitResult&)
{
    DEBUG("For FuncParam");
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

void ToCangjiePass::Visit(const FuncParamList& node, VisitResult&)
{
    DEBUG("For FuncParamList: ", node.params.size());
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

void ToCangjiePass::Visit(const FuncBody& node, VisitResult&)
{
    DEBUG("For FuncBody");
    TryPrintGenericParams(node.generic.get());
    VisitNode(node.paramLists[0]);
    if (!TryPrintType(node.retType) && Config().Sema()) {
        TryPrintTy(TryGetRetTy(node.ty));
    }
    TryPrintGenericConstraints(node.generic);
    PrintBlock(node.body);
}

void ToCangjiePass::Visit(const FuncDecl& node, VisitResult&)
{
    DEBUG("For FuncDecl: ", node.identifier.Val());
    PrintDecl(node);
    if (TryPrintEnumConstructor(node) || TryPrintConstructor(node) || TryPrintGetter(node) || TryPrintSetter(node)) {
        return;
    }
    // General func.
    PRT().PVals("func ", Id(node.identifier));
    TryPrintNode(node.funcBody);
}

VisitResult ToCangjiePass::Before(const MainDecl& node)
{
    if (!node.desugarDecl) {
        return VisitResult::Cont();
    }
    DEBUG("For MainDecl");
    // 存在解糖节点
    if (Config().Desugar() || node.desugarDecl) {
        DEBUG("For Desugared Decl of MainDecl");
        if (node.TestAttr(Attribute::UNSAFE)) {
            PRT().PVal("unsafe ");
        }
        VisitNode(node.desugarDecl);
    } else {
        // 还原原节点
        DEBUG("For Recover Desugared Decl of MainDecl");
        PrintDecl(node);
        PRT().PVal("main");
        TryPrintNode(node.desugarDecl->funcBody);
    }
    return VisitResult::Skip();
}

void ToCangjiePass::Visit(const MainDecl& node, VisitResult& res)
{
    DEBUG("For MainDecl");
    PrintDecl(node);
    PRT().PVal("main");
    AH_CHECK_NULL(node.funcBody);
    Visit(*node.funcBody, res);
}

void ToCangjiePass::Visit(const PrimaryCtorDecl& node, VisitResult&)
{
    DEBUG("For PrimaryCtorDecl: ", node.identifier.Val());
    if (Config().Sema()) {
        return; // PrimaryCtorDecl is desugared in Sema
    }
    PrintDecl(node);
    PRT().PVal(Id(node.identifier));
    AH_CHECK_NULL(node.funcBody);
    TryPrintNode(node.funcBody.get());
}

void ToCangjiePass::Visit(const ClassDecl& node, VisitResult& res)
{
    DEBUG("For ClassDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "class");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void ToCangjiePass::Visit(const InterfaceDecl& node, VisitResult& res)
{
    DEBUG("For InterfaceDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "interface");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void ToCangjiePass::Visit(const StructDecl& node, VisitResult& res)
{
    DEBUG("For StructDecl: ", node.identifier.Val());
    PrintInheritableDeclHeader(node, "struct");
    AH_CHECK_NULL(node.body);
    PrintInheritableDeclBody(node.body->decls);
}

void ToCangjiePass::Visit(const EnumDecl& node, VisitResult&)
{
    DEBUG("For EnumDecl: ", node.identifier.Val());
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

void ToCangjiePass::Visit(const ExtendDecl& node, VisitResult&)
{
    DEBUG("For ExtendDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("extend");
    TryPrintGenericParams(node.generic.get());
    TryPrintNode(node.extendedType.get(), " ");
    PrintInheritedTypes(node.inheritedTypes);
    TryPrintGenericConstraints(node.generic.get());
    PrintInheritableDeclBody(node.members);
}

void ToCangjiePass::Visit(const TypeAliasDecl& node, VisitResult&)
{
    DEBUG("For TypeAliasDecl: ", node.identifier.Val());
    PrintDecl(node);
    PRT().PVal("type ");
    PRT().PVal(Id(node.identifier));
    TryPrintNode(node.type.get(), " = ");
}

// Type
void ToCangjiePass::Visit(const PrimitiveType& node, VisitResult&)
{
    DEBUG("For PrimitiveType");
    PRT().PVal(node.str);
}

void ToCangjiePass::Visit(const RefType& node, VisitResult& res)
{
    DEBUG("For RefType");
    if (node.ref.identifier.Val() == "" && Config().Sema() && node.ty) {
        PrintTy(*node.ty);
    } else {
        PRT().PVal(Id(node.ref.identifier));
        PRT().PVec<AstNode>(
            node.typeArguments, [this](const AstNode& node) { Traverse(node, visitor); }, ", ", "<", ">");
    }
}

VisitResult ToCangjiePass::Before(const OptionType& node)
{
    if (!Config().Desugar() || !node.desugarType) {
        // desugar is false && node.desugarType is not nullptr is okay, because node.componentType is not nullptr.
        return VisitResult::Cont();
    }
    DEBUG("For OptionType: desugar: ", node.desugarType != nullptr);
    TryPrintNode(node.desugarType);
    return VisitResult::Skip();
}

void ToCangjiePass::Visit(const OptionType& node, VisitResult&)
{
    DEBUG("For OptionType");
    Str preQuest(node.questNum, '?');
    TryPrintNode(node.componentType.get(), preQuest);
}

void ToCangjiePass::Visit(const TupleType& node, VisitResult&)
{
    DEBUG("For TupleType");
    PRT().PVec<Type>(
        node.fieldTypes, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "(", ")");
}

void ToCangjiePass::Visit(const QualifiedType& node, VisitResult&)
{
    DEBUG("For QualifiedType");
    VisitNode(node.baseType);
    PRT().PVals(".", Id(node.field));
    PRT().PVec<Type>(
        node.typeArguments, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "<", ">");
}

void ToCangjiePass::Visit(const ThisType& node, VisitResult&)
{
    DEBUG("For ThisType");
    PRT().PVal("This");
}

void ToCangjiePass::Visit(const VArrayType& node, VisitResult&)
{
    DEBUG("For VArrayType");
    PRT().PVal("VArray<");
    VisitNode(node.typeArgument);
    PRT().PVal(", ");
    VisitNode(node.constantType);
    PRT().PVal(">");
}

void ToCangjiePass::Visit(const ParenType& node, VisitResult&)
{
    DEBUG("For ParenType");
    TryPrintNode(node.type.get(), "(", ")");
}

void ToCangjiePass::Visit(const ConstantType& node, VisitResult&)
{
    DEBUG("For ConstantType");
    TryPrintNode(node.constantExpr.get(), "$");
}

void ToCangjiePass::Visit(const FuncType& node, VisitResult&)
{
    DEBUG("For FuncType");
    PRT().PVec<Type>(
        node.paramTypes, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "(", ")", true);
    TryPrintNode(node.retType.get(), " -> ");
}

// Pattern
void ToCangjiePass::Visit(const WildcardPattern& node, VisitResult&)
{
    DEBUG("For WildcardPattern");
    PRT().PVal("_");
}

void ToCangjiePass::Visit(const ConstPattern& node, VisitResult&)
{
    DEBUG("For ConstPattern");
    VisitNode(node.literal);
}

void ToCangjiePass::Visit(const EnumPattern& node, VisitResult&)
{
    DEBUG("For EnumPattern");

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
    PRT().PVec<Pattern>(
        node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, ", ", "(", ")");
}

void ToCangjiePass::Visit(const VarPattern& node, VisitResult&)
{
    DEBUG("For VarPattern", node.varDecl->identifier.Val());
    PRT().PVal(Id(node.varDecl->identifier));
}

void ToCangjiePass::Visit(const VarOrEnumPattern& node, VisitResult&)
{
    DEBUG("For VarOrEnumPattern: ", node.identifier.Val());
    PRT().PVal(Id(node.identifier));
    // pattern 是 解糖后才有的？
    // VisitNode(node.pattern);
}

void ToCangjiePass::Visit(const TypePattern& node, VisitResult&)
{
    DEBUG("For TypePattern");
    VisitNode(node.pattern);
    TryPrintType(node.type.get());
}

void ToCangjiePass::Visit(const TuplePattern& node, VisitResult&)
{
    DEBUG("For TuplePattern");
    PRT().PVec<Pattern>(
        node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, ", ", "(", ")");
}

// Expr
void ToCangjiePass::Visit(const Block& node, VisitResult&)
{
    DEBUG("For Block: ", node.body.size());
    PRT().PVec<AstNode>(node.body, [this](const AstNode& node) {
        auto res = Traverse(node, visitor);
        PRT().PNL();
    });
}

VisitResult ToCangjiePass::Before(const RefExpr& node)
{
    if (!Config().Sema()) {
        return VisitResult::Cont();
    }
    auto target = node.ref.target;
    if (auto it = desugaredVarId.find(target); it != desugaredVarId.end()) {
        DEBUG("For RefExpr: ", node.ref.identifier.Val());
        PRT().PVal(it->second);
        return VisitResult::Skip();
    }
    return VisitResult::Cont();
}

void ToCangjiePass::Visit(const RefExpr& node, VisitResult&)
{
    DEBUG("For RefExpr: ", node.ref.identifier.Val());
    PRT().PVal(Id(node.ref.identifier));
    PrintInstArgs(node);
}

void ToCangjiePass::Visit(const FuncArg& node, VisitResult&)
{
    DEBUG("For FuncArg");
    VisitNode(node.expr);
}

void ToCangjiePass::Visit(const CallExpr& node, VisitResult&)
{
    DEBUG("For CallExpr");
    if (TryRecoverCallExpr(node)) {
        return;
    }
    // General func call
    VisitNode(node.baseFunc);
    PRT().PVec<FuncArg>(
        node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")", true);
}

void ToCangjiePass::Visit(const ReturnExpr& node, VisitResult& res)
{
    DEBUG("For ReturnExpr");
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

void ToCangjiePass::Visit(const LitConstExpr& node, VisitResult&)
{
    DEBUG("For LitConstExpr");
    PRT().PVal(node.ToString());
}

void ToCangjiePass::Visit(const ArrayLit& node, VisitResult&)
{
    DEBUG("For ArrayLit");
    PRT().PVec<AstNode>(
        node.children, [this](const AstNode& expr) { Traverse(expr, visitor); }, ", ", "[", "]", true);
}

void ToCangjiePass::Visit(const TupleLit& node, VisitResult&)
{
    DEBUG("For TupleLit");
    PRT().PVec<AstNode>(
        node.children, [this](const AstNode& expr) { Traverse(expr, visitor); }, ", ", "(", ")", true);
}

void ToCangjiePass::Visit(const TypeConvExpr& node, VisitResult&)
{
    DEBUG("For TypeConvExpr");
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

void ToCangjiePass::Visit(const MemberAccess& node, VisitResult&)
{
    DEBUG("For MemberAccess");
    VisitNode(node.baseExpr);
    PRT().PVals(".", Id(node.field));
    if (!Config().Sema() || !IsRefEnum(*node.baseExpr)) {
        PrintInstArgs(node, node.isPattern);
    }
}

void ToCangjiePass::Visit(const LambdaExpr& node, VisitResult&)
{
    DEBUG("For LambdaExpr");
    auto& body = *node.funcBody;
    PRT().PVal("{");
    AH_ASSERT(body.paramLists.size() == 1);
    auto& params = body.paramLists[0]->params;
    PRT().PVec<FuncParam>(
        params, [this](const FuncParam& param) { Traverse(param, visitor); }, ", ", " ");
    PRT().PWI([this, &body] { VisitNode(body.body); }, " =>", "}");
}

void ToCangjiePass::Visit(const MatchCase& node, VisitResult&)
{
    DEBUG("For MatchCase");
    PRT().PVal("case ");
    PRT().PVec<Pattern>(
        node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, " | ");
    TryPrintNode(node.patternGuard.get(), " where ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void ToCangjiePass::Visit(const MatchCaseOther& node, VisitResult&)
{
    DEBUG("For MatchCaseOther");
    TryPrintNode(node.matchExpr.get(), "case ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void ToCangjiePass::Visit(const MatchExpr& node, VisitResult&)
{
    DEBUG("For MatchExpr");
    PRT().PVal("match ");
    // match with selector
    TryPrintNode(node.selector.get(), "(", ")");
    PRT().PValNL(" {").Indent();
    PRT().PVec<MatchCase>(node.matchCases, [this](const MatchCase& mc) { Traverse(mc, visitor); });
    PRT().PVec<MatchCaseOther>(node.matchCaseOthers, [this](const MatchCaseOther& mco) { Traverse(mco, visitor); });
    PRT().Unindent();
    PRT().PVal("}");
}

void ToCangjiePass::Visit(const IsExpr& node, VisitResult&)
{
    DEBUG("For IsExpr");
    VisitNode(node.leftExpr);
    PRT().PVal(" is ");
    VisitNode(node.isType);
}

void ToCangjiePass::Visit(const AsExpr& node, VisitResult&)
{
    DEBUG("For AsExpr");
    VisitNode(node.leftExpr);
    PRT().PVal(" as ");
    VisitNode(node.asType);
}

VisitResult ToCangjiePass::Before(const AssignExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    DEBUG("For AssignExpr");
    if (Config().Desugar() || node.desugarExpr) {
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

void ToCangjiePass::Visit(const AssignExpr& node, VisitResult&)
{
    DEBUG("For AssignExpr");
    VisitNode(node.leftValue);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void ToCangjiePass::Visit(const IncOrDecExpr& node, VisitResult&)
{
    DEBUG("For IncOrDecExpr");
    TryPrintNode(node.expr.get(), "", Tk2Str(node.op));
}

VisitResult ToCangjiePass::Before(const UnaryExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    DEBUG("For UnaryExpr");
    if (Config().Desugar() || node.desugarExpr) {
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

void ToCangjiePass::Visit(const UnaryExpr& node, VisitResult&)
{
    DEBUG("For UnaryExpr");
    TryPrintNode(node.expr.get(), Tk2Str(node.op));
}

VisitResult ToCangjiePass::Before(const BinaryExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    DEBUG("For BinaryExpr");
    if (Config().Desugar() || node.desugarExpr) {
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
void ToCangjiePass::Visit(const BinaryExpr& node, VisitResult&)
{
    DEBUG("For BinaryExpr");
    VisitNode(node.leftExpr);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void ToCangjiePass::Visit(const ThrowExpr& node, VisitResult&)
{
    DEBUG("For ThrowExpr");
    PRT().PVal("throw ");
    VisitNode(node.expr);
}

VisitResult ToCangjiePass::Before(const SubscriptExpr& node)
{
    if (!node.desugarExpr) {
        return VisitResult::Cont();
    }
    DEBUG("For SubscriptExpr");
    if (Config().Desugar() || node.desugarExpr) {
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

void ToCangjiePass::Visit(const SubscriptExpr& node, VisitResult&)
{
    DEBUG("For SubscriptExpr");
    VisitNode(node.baseExpr);
    PRT().PVec<Expr>(
        node.indexExprs, [this](const Expr& expr) { Traverse(expr, visitor); }, ", ", "[", "]");
}

void ToCangjiePass::Visit(const JumpExpr& node, VisitResult&)
{
    DEBUG("For JumpExpr");
    if (node.isBreak) {
        PRT().PVal("break");
    } else {
        PRT().PVal("continue");
    }
}

void ToCangjiePass::Visit(const RangeExpr& node, VisitResult&)
{
    DEBUG("For RangeExpr");
    TryPrintNode(node.startExpr.get());
    PRT().PVal("..");
    TryPrintNode(node.stopExpr.get());
    TryPrintNode(node.stepExpr.get(), " : ");
}

void ToCangjiePass::Visit(const LetPatternDestructor& node, VisitResult&)
{
    DEBUG("For LetPatternDestructor");
    PRT().PVal("let ");
    PRT().PVec<Pattern>(
        node.patterns, [this](const Pattern& pat) { Traverse(pat, visitor); }, " | ");
    TryPrintNode(node.initializer, " <- ");
}

void ToCangjiePass::Visit(const IfExpr& node, VisitResult&)
{
    DEBUG("For IfExpr");
    PRT().PVal("if ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.thenBody); }, " {", "}");
    TryPrintNode(node.elseBody.get(), " else ");
}

void ToCangjiePass::Visit(const WhileExpr& node, VisitResult&)
{
    DEBUG("For WhileExpr");
    PRT().PVal("while ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.body); }, " {", "}");
}

void ToCangjiePass::Visit(const DoWhileExpr& node, VisitResult&)
{
    DEBUG("For DoWhileExpr");
    PRT().PWI([this, &node] { VisitNode(node.body); }, "do {", "} while ");
    TryPrintNode(node.condExpr.get(), "(", ")");
    PRT().PNL();
}

void ToCangjiePass::Visit(const ForInExpr& node, VisitResult&)
{
    DEBUG("For ForInExpr");
    if (TryPrintDesugaredForInExpr(node)) {
        return;
    }
    TryPrintNode(node.pattern.get(), "for (", " in ");
    TryPrintNode(node.inExpression.get());
    TryPrintNode(node.patternGuard.get());
    PRT().PWI([this, &node] { VisitNode(node.body); }, ") {", "}");
}

// Generic
void ToCangjiePass::Visit(const Generic& node, VisitResult&)
{
    DEBUG("For Generic");
    PRT().PVec<GenericParamDecl>(
        node.typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, visitor); }, ", ", "<", ">");
    PRT().PVec<GenericConstraint>(
        node.genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, visitor); }, ", ", " where ");
}

void ToCangjiePass::Visit(const GenericParamDecl& node, VisitResult&)
{
    DEBUG("For GenericParamDecl");
    PRT().PVal(Id(node.identifier));
}

void ToCangjiePass::Visit(const GenericConstraint& node, VisitResult&)
{
    DEBUG("For GenericConstraint");
    VisitNode(node.type);
    PRT().PVal(" <: ");
    PRT().PVec<Type>(
        node.upperBounds, [this](const Type& tp) { Traverse(tp, visitor); }, " & ");
}

/// 私有实现函数

ToCangjiePass::ToCangjiePass(const ToSourcePassConfig& config) : ToSourcePass(config)
{
    RegisterHandlers();
}

void ToCangjiePass::RegisterHandlers()
{
    static StrMap<AstKind> name2kind{
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
inline void ToCangjiePass::TryPrintNode(const Ptr<AstNode> pnode, ConStr& pre, ConStr& suf, bool withNL)
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
void ToCangjiePass::PrintDecl(const Decl& node)
{
    PrintAnnotations(node);
    PrintModifiers(node);
}

namespace {
// 关注的注解属性映射表
StrMap<Attribute> focusAttrsMap = {{"C", Attribute::C}, {"public", Attribute::PUBLIC},
    {"protected", Attribute::PROTECTED}, {"private", Attribute::PRIVATE}, {"internal", Attribute::INTERNAL}};
} // namespace

/**
 * @brief 辅助打印注解列表。
 *  注解打印规则:
 * 1. 用户代码注解列表： node.annotations
 * 2. 配置忽略打印的列表： Config().ignoreAnnotations
 * 3. 语义后置的注解列表： Config().focusAnnotationAttrs
 * 规则描述:
 * (node.annotations + Config().focusAnnotationAttrs) - Config().ignoreAnnotations
 */
void ToCangjiePass::PrintAnnotations(const Decl& node)
{
    StrSet annotations;
    for (auto& anno : node.annotations) {
        auto& annoName = anno->identifier.Val();
        if (!Config().ignoreAnnotations.count(annoName)) {
            Traverse(*anno, visitor);
            annotations.insert(annoName);
        }
    }
    if (!Config().Sema()) {
        return;
    }
    // 补充打印语义后的缺少的注解
    PRT().PVec<Str>(Config().focusAnnotationAttrs, [&node, &annotations, this](ConStr& anno) {
        // node 有关注的属性 没有打印过 也没有忽略
        if (node.TestAttr(focusAttrsMap.at(anno)) && !annotations.count(anno) &&
            !Config().ignoreAnnotations.count(anno)) {
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
void ToCangjiePass::PrintModifiers(const Decl& node)
{
    StrSet modifiers;
    PRT().PVec<Modifier>(
        node.modifiers,
        [this, &modifiers](const Modifier& mod) {
            modifiers.insert(Tk2Str(mod.modifier));
            Traverse(mod, visitor);
        },
        " ", "", " ");

    if (!Config().Sema() || !Config().focusModifierWhiteList.count(node.astKind) || !NeedAddMoidifier(node)) {
        return;
    }
    // 补充打印语义后的缺少的修饰符
    PRT().PVec<Str>(Config().focusModifierAttrs, [&node, &modifiers, this](ConStr& mod) {
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
inline void ToCangjiePass::PrintBlock(const Ptr<Block> pnode)
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
bool ToCangjiePass::TryPrintConstructor(const FuncDecl& node)
{
    if (!node.TestAttr(Attribute::CONSTRUCTOR)) {
        return false;
    }
    DEBUG("For FuncDecl is constructor");
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
bool ToCangjiePass::TryPrintGetter(const FuncDecl& node)
{
    if (!node.isGetter) {
        return false;
    }
    DEBUG("For FuncDecl is getter");
    PRT().PVal("get()");
    PrintBlock(node.funcBody->body);
    return true;
}
/**
 * @brief 尝试作为setter打印。
 * @param decl 枚举声明的引用: VarDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToCangjiePass::TryPrintSetter(const FuncDecl& node)
{
    if (!node.isSetter) {
        return false;
    }
    DEBUG("For FuncDecl is setter");
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
bool ToCangjiePass::TryPrintEnumConstructor(const VarDecl& node)
{
    if (!node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    DEBUG("For VarDecl as EnumConstructor");
    PRT().PVal(node.identifier.Val());
    return true;
}

/**
 * @brief 尝试作为Enum构造器打印。
 * @param decl 枚举声明的引用: FuncDecl。
 * @return 如果打印成功返回true，否则返回false。
 */
bool ToCangjiePass::TryPrintEnumConstructor(const FuncDecl& node)
{
    if (!node.TestAttr(Attribute::ENUM_CONSTRUCTOR)) {
        return false;
    }
    DEBUG("For FuncDecl as EnumConstructor");
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
inline void ToCangjiePass::PrintInheritedTypes(ConVec<OwnedPtr<Type>>& types)
{
    PRT().PVec<Type>(
        types, [this](const Type& ty) { Traverse(ty, visitor); }, " & ", " <: ");
}

/**
 * @brief 辅助打印可继承类型头部。
 * @param node 声明引用 (可继承类型: Class, Struct, Interface, Enum)
 */
void ToCangjiePass::PrintInheritableDeclHeader(const InheritableDecl& node, ConStr& keyword)
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
inline void ToCangjiePass::PrintDecls(ConVec<OwnedPtr<Decl>>& decls)
{
    PRT().PVec<Decl>(decls, [this](const Decl& decl) {
        Traverse(decl, visitor);
        PRT().PNL(2);
    });
}

/**
 * @brief 辅助打印可继承类型定义体。
 */
void ToCangjiePass::PrintInheritableDeclBody(ConVec<OwnedPtr<Decl>>& members)
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
void ToCangjiePass::TryPrintGenericParams(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    DEBUG("For GenericParams");
    PRT().PVec<GenericParamDecl>(
        generic->typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, visitor); }, ", ", "<", ">");
}

/**
 * @brief 辅助打印泛型约束。
 * @param generic 泛型节点指针。
 */
void ToCangjiePass::TryPrintGenericConstraints(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    DEBUG("For GenericConstraints");
    PRT().PVec<GenericConstraint>(
        generic->genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, visitor); }, ", ", " where ");
}

/**
 * @brief 尝试打印泛型实例参数。
 * @param ref 引用表达式: RefExpr or MemberAccess
 * @param isPattern 是否是在 pattern 中 (enum pattern 不允许打印泛型参数)
 */
void ToCangjiePass::PrintInstArgs(const NameReferenceExpr& ref, bool isPattern)
{
    if (!ref.typeArguments.empty()) {
        PRT().PVec<Type>(
            ref.typeArguments, [this](const Type& tp) { Traverse(tp, visitor); }, ", ", "<", ">");
    } else if (Config().Sema() && !isPattern) {
        PRT().PVec<Ty>(
            ref.instTys, [this](const Ty& ty) { PrintTy(ty); }, ", ", "<", ">");
    }
}

/**
 * @brief 辅助打印 变量的类型标注。
 */
inline void ToCangjiePass::PrintVarType(const VarDeclAbstract& node)
{
    if (!TryPrintType(node.type.get()) && Config().Sema()) {
        TryPrintTy(node.ty);
    }
}

/**
 * @brief 辅助打印 Type 节点
 * @param type 类型节点指针。
 */
bool ToCangjiePass::TryPrintType(const Ptr<Type> type)
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
    if (Config().Sema()) {
        return TryPrintTy(type->ty);
    }
    return false;
}

/**
 * @brief 辅助打印 Ty 标注。
 */
inline bool ToCangjiePass::TryPrintTy(const Ptr<Ty> ty)
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
void ToCangjiePass::PrintTy(const Ty& ty)
{

    // If the format is incorrect, need to adjust it.
    switch (ty.kind) {
        case TypeKind::TYPE_CSTRING:
            PRT().PVal("CString");
            return;
        case TypeKind::TYPE_POINTER:
            PRT().PVal("CPointer");
            PRT().PVec<Ty>(
                ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "<", ">");
            return;
        case TypeKind::TYPE_CLASS:
        case TypeKind::TYPE_INTERFACE:
        case TypeKind::TYPE_STRUCT:
        case TypeKind::TYPE_ENUM:
            PRT().PVal(ty.name);
            PRT().PVec<Ty>(
                ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "<", ">");
            return;
        case TypeKind::TYPE_TUPLE:
            PRT().PVec<Ty>(
                ty.typeArgs, [this](const Ty& argTy) { PrintTy(argTy); }, ", ", "(", ")");
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
bool ToCangjiePass::TryRecoverCallExpr(const CallExpr& node)
{
    if (Config().Sema()) {
        return TryPrintInitCall(node) || TryRecoverOverloadCallExpr(node) || TryRecoverPropCallExpr(node);
    }
    return false;
}

namespace {
/**
 * Try get the base func identifier.
 */
inline Str TryGetCallRef(const CallExpr& node)
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
bool ToCangjiePass::TryPrintInitCall(const CallExpr& node)
{
    if (!IsInitCall(node)) {
        return false;
    }
    AH_CHECK_NULL(node.ty);
    // 构造函数调用 init() -> A(), TODO: 应该缩小下范围， 构造函数内的init不需要替换
    // 特殊场景处理: init() -> ??A 是解糖表达式
    PrintTy(*TryGetBaseTy(node.ty));
    PRT().PVec<FuncArg>(
        node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")", true);
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
bool ToCangjiePass::TryRecoverOverloadCallExpr(const CallExpr& node)
{
    if (!IsOverloadCall(node)) {
        return false;
    }
    auto fn = node.resolvedFunction;
    auto op = fn->op;
    DEBUG("For Overload operator: ", Tk2Str(op));
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
        PRT().PVec<FuncArg>(
            node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")");
    } else {
        // 二元： + - * / % ** << >> < <= > >= == != & ^ |
        Traverse(*base, visitor);
        AH_ASSERT(node.args.size() == 1);
        TryPrintNode(node.args[0].get(), " " + Tk2Str(op) + " ");
    }
    // 打印个注释在这里
    PRT().PVal(" /* Desugared ");
    Traverse(ma, visitor);
    PRT().PVec<FuncArg>(
        node.args, [this](const FuncArg& arg) { Traverse(arg, visitor); }, ", ", "(", ")", true);
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
bool ToCangjiePass::TryRecoverPropCallExpr(const CallExpr& node)
{
    if (!IsPropCall(node)) {
        return false;
    }
    DEBUG("For Property CallExpr");
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
bool ToCangjiePass::TryPrintDesugaredForInExpr(const ForInExpr& node)
{
    // TODO: 适配 默认 desugar false
    if (!Config().Sema() || !Config().Desugar()) {
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
void ToCangjiePass::PrintDesugaredForInRange(const ForInExpr& node)
{
    DEBUG("For ForInExpr with Range");
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
void ToCangjiePass::PrintDesugaredForInIterator(const ForInExpr& node)
{
    DEBUG("For ForInExpr with Iterator");
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
void ToCangjiePass::PrintDesugaredForInString(const ForInExpr& node)
{
    DEBUG("For ForInExpr with String");
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
