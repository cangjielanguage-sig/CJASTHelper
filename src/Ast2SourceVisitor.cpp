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
using Cangjie::TokenKind;
using Cangjie::AST::Attribute;
using Cangjie::AST::CallKind;
using Cangjie::AST::Expr;
using Cangjie::AST::ImportKind;
using Cangjie::AST::Pattern;

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

std::string GetFileNameWithoutSuffix(const std::string& fname)
{
    size_t pos = fname.find_last_of('.');
    if (pos != std::string::npos) {
        return fname.substr(0, pos);
    } else {
        return fname;
    }
}

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

inline bool IsIterVar(const std::string& id)
{
    static std::vector<std::string> keys{"$iter-", "$stop-compiler", "iter-compiler"};
    for (auto& key : keys) {
        if (id.find(key) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline void UpdateIterVar(std::string& id)
{
    static int counter = 0; // 全局id
    if (IsIterVar(id)) {
        id += std::to_string(counter++);
    }
}

inline std::string Id(const Cangjie::Identifier& id)
{

    std::string res = id.Val();
    UpdateIterVar(res);
    replaceAll(res, "$", "");
    replaceAll(res, "-", "_");
    return res;
}

inline std::string Tk2Str(Cangjie::TokenKind tk)
{
    return Cangjie::TOKENS[static_cast<int>(tk)];
}

inline Ptr<Cangjie::AST::Ty> TryGetRetTy(const FuncBody& funcBody)
{
    if (!funcBody.ty) {
        return nullptr;
    }
    if (!funcBody.ty->IsFunc()) {
        return nullptr;
    }
    using FuncTy = Cangjie::AST::FuncTy;
    return static_cast<FuncTy*>(funcBody.ty.get())->retTy;
}

inline std::string TryGetCallRef(const CallExpr& node)
{
    if (node.baseFunc->astKind == AstKind::REF_EXPR) {
        return static_cast<RefExpr*>(node.baseFunc.get().get())->ref.identifier.Val();
    }
    return "";
}

bool IsInitCall(const CallExpr& node)
{
    if (node.callKind == CallKind::CALL_STRUCT_CREATION || node.callKind == CallKind::CALL_OBJECT_CREATION ||
        // 处理编译器生成的错误类型节点
        node.callKind == CallKind::CALL_INVALID) {
        return TryGetCallRef(node) == "init";
    }
    return false;
}

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

inline bool IsImportStdCore(const ImportContent& ic)
{
    // std.core.*
    auto& paths = ic.prefixPaths;
    return ic.kind == ImportKind::IMPORT_ALL && paths.size() == 2 && paths[0] == "std" && paths[1] == "core";
}

inline bool NeedHidden(const ImportSpec& node)
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

const std::string SUFFIX = "_source.cj";
} // namespace

Ast2SourceException::Ast2SourceException(const std::string& msg) noexcept : message(msg)
{
}

const char* Ast2SourceException::what() const noexcept
{
    return message.c_str();
}

Ast2SourceVisitor::Ast2SourceVisitor(const std::string& out, int indent, Flag flags)
    : out(out), prt(ofs, indent), flags(flags)
{
    CreateDirIfNotExists(out);
}

void Ast2SourceVisitor::Visit(const File& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For File imports: ", node.imports.size());
    std::string fp = out + "/" + GetFileNameWithoutSuffix(node.fileName) + SUFFIX;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    // package declaration
    PrintNode(node.package.get());
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
    PrintNode(node.modifier.get(), "", " ");
    PRT().PVal("package ");
    PRT().PVec<std::string>(node.prefixPaths, [this](const std::string& pre) { PRT().PVal(pre); }, ".", "", ".");
    PRT().PVal(Id(node.packageName));
    PRT().PNL(2);
}

void Ast2SourceVisitor::Visit(const ImportSpec& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ImportSpec");
    if (NeedHidden(node)) {
        // Import multi-packages is desugared as serveral import single-packages, import std.core.* is implicit import!
        return;
    }
    VisitNodes(node.annotations);
    PrintNode(node.modifier.get(), "", " ");
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
void Ast2SourceVisitor::Visit(const FuncDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncDecl: ", node.identifier.Val());
    VisitDecl(node);
    if (!node.TestAttr(Attribute::CONSTRUCTOR)) {
        PRT().PVal("func ");
    }
    PRT().PVal(Id(node.identifier));
    AH_CHECK_NULL(node.funcBody);
    Visit(*node.funcBody, res);
}

void Ast2SourceVisitor::Visit(const FuncBody& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncBody");
    VisitGenericParams(node.generic.get());
    VisitNode(node.paramLists[0]);
    Ptr<Ty> retTy = TryGetRetTy(node);
    if (!node.funcDecl || (node.funcDecl && !node.funcDecl->TestAttr(Attribute::CONSTRUCTOR))) {
        VisitType(node.retType, retTy);
    }
    VisitGenericConstraints(node.generic.get());
    PRT().PPtr<Block>(
        node.body, [this](const Block& block) { PRT().PWI([this, &block] { Traverse(block, *this); }, " {", "}"); });
}

void Ast2SourceVisitor::Visit(const FuncParamList& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParamList: ", node.params.size());
    PRT().PVec<FuncParam>(
        node.params, [this](const FuncParam& param) { Traverse(param, *this); }, ", ", "(", ")", true);
}

void Ast2SourceVisitor::Visit(const FuncParam& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParam");
    PRT().PVal(Id(node.identifier));
    VisitType(node.type.get(), node.ty);
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
        VisitDecl(node);
        PRT().PVal("main");
        AH_CHECK_NULL(node.funcBody);
        Visit(*node.funcBody, res);
    }
}

void Ast2SourceVisitor::Visit(const VarDecl& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarDecl: ", node.identifier.Val());
    VisitDecl(node);
    std::string pre = "let";
    if (node.isConst) {
        pre = "const";
    } else if (node.isVar) {
        pre = "var";
    }
    auto id = Id(node.identifier);
    // Desugared variable id
    if (IsIterVar(node.identifier.Val())) {
        desugaredVarId.emplace(&node, id);
    }
    PRT().PVals(pre, " ", id);
    VisitType(node.type.get(), node.ty);
    PrintNode(node.initializer.get(), " = ");
}

void Ast2SourceVisitor::Visit(const ClassDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ClassDecl: ", node.identifier.Val());
    VisitDecl(node);
    PRT().PVals("class ", Id(node.identifier));
    VisitGenericParams(node.generic);
    PRT().PVec<Type>(node.inheritedTypes, [this](const Type& ty) { Traverse(ty, *this); }, " & ", " <: ");
    VisitGenericConstraints(node.generic.get());
    VisitNode(node.body);
}

void Ast2SourceVisitor::Visit(const ClassBody& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ClassBody");
    PRT().PValNL(" {").Indent();
    PRT().PVec<Decl>(node.decls, [this](const Decl& decl) {
        Traverse(decl, *this);
        PRT().PNL(2);
    });
    PRT().Unindent();
    PRT().PVal("}");
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
    if (node.ref.identifier.Val() == "" && node.ty) {
        VisitTy(*node.ty);
    } else {
        PRT().PVal(Id(node.ref.identifier));
        PRT().PVec<AstNode>(node.typeArguments, [this](const AstNode& node) { Traverse(node, *this); }, ", ", "<", ">");
    }
}

void Ast2SourceVisitor::Visit(const OptionType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For OptionType");
    PrintNode(node.componentType.get(), "?");
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
    PrintNode(node.type.get(), "(", ")");
}

void Ast2SourceVisitor::Visit(const ConstantType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ConstantType");
    PrintNode(node.constantExpr.get(), "$");
}

void Ast2SourceVisitor::Visit(const FuncType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncType");
    PRT().PVec<Type>(node.paramTypes, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "(", ")", true);
    PrintNode(node.retType.get(), " -> ");
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
    VisitNode(node.constructor);
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
    VisitType(node.type.get());
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
    if (node.body.size() > 0) {
        Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Block body ", static_cast<int>(node.body[0]->astKind));
    }
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
    if (OpenDesugar() && OpenSema()) {
        auto target = node.ref.target;
        if (auto it = desugaredVarId.find(target); it != desugaredVarId.end()) {
            PRT().PVal(it->second);
            return;
        }
    }
    PRT().PVal(Id(node.ref.identifier));
    PRT().PVec<Type>(node.typeArguments, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "<", ">");
}

void Ast2SourceVisitor::Visit(const FuncArg& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncArg");
    VisitNode(node.expr);
}

void Ast2SourceVisitor::Visit(const CallExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For CallExpr");
    if (OpenDesugar() && OpenSema()) {
        // CallExpr 可能是解糖的 操作重载调用 要恢复操作符调用源码
        // 比如 a.[](i) -> a[i], a.+(b) -> a + b
        if (IsOverloadCall(node)) {
            PrintOverloadCallExpr(node);
            return;
        }
    }
    if (IsInitCall(node)) {
        AH_CHECK_NULL(node.ty);
        // 构造函数调用 init() -> A(), TODO: 应该缩小下范围， 构造函数内的init不需要替换
        VisitTy(*node.ty);
    } else {
        VisitNode(node.baseFunc);
    }
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

void Ast2SourceVisitor::Visit(const MemberAccess& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MemberAccess");
    VisitNode(node.baseExpr);
    PRT().PVals(".", Id(node.field));
    PRT().PVec<Type>(node.typeArguments, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "<", ">");
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
    PrintNode(node.patternGuard.get(), " where ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void Ast2SourceVisitor::Visit(const MatchCaseOther& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchCaseOther");
    PrintNode(node.matchExpr.get(), "case ");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); }, " =>");
}

void Ast2SourceVisitor::Visit(const MatchExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchExpr");
    PRT().PVal("match ");
    // match with selector
    PrintNode(node.selector.get(), "(", ")");
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
    PrintNode(node.expr.get(), "", Tk2Str(node.op));
}

void Ast2SourceVisitor::Visit(const UnaryExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For UnaryExpr");
    PrintNode(node.expr.get(), Tk2Str(node.op));
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
    PrintNode(node.startExpr.get());
    PRT().PVal("..");
    PrintNode(node.stopExpr.get());
    PrintNode(node.stepExpr.get(), " : ");
}

void Ast2SourceVisitor::Visit(const LetPatternDestructor& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For LetPatternDestructor");
    PRT().PVal("let ");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, *this); }, " | ");
    PrintNode(node.initializer, " <- ");
}

void Ast2SourceVisitor::Visit(const IfExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For IfExpr");
    PRT().PVal("if ");
    PrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.thenBody); }, " {", "}");
    PrintNode(node.elseBody.get(), " else ");
}

void Ast2SourceVisitor::Visit(const WhileExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For WhileExpr");
    PRT().PVal("while ");
    PrintNode(node.condExpr.get(), "(", ")");
    PRT().PWI([this, &node] { VisitNode(node.body); }, " {", "}");
}

void Ast2SourceVisitor::Visit(const DoWhileExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For DoWhileExpr");
    PRT().PWI([this, &node] { VisitNode(node.body); }, "do {", "} while ");
    PrintNode(node.condExpr.get(), "(", ")");
    PRT().PNL();
}

void Ast2SourceVisitor::Visit(const ForInExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ForInExpr");
    if (OpenDesugar() && OpenSema()) {
        using ForInKind = Cangjie::AST::ForInKind;
        if (node.forInKind == ForInKind::FORIN_RANGE) {
            PrintDesugaredForInRange(node);
        } else if (node.forInKind == ForInKind::FORIN_STRING) {
            PrintDesugaredForInString(node);
        } else {
            // Default: ForInKind::FORIN_ITER
            PrintDesugaredForInIterator(node);
            Logger::Get().Warn("Ast2SourceVisitor::Visit", "Unknown ForInKind: ", static_cast<int>(node.forInKind));
        }
    } else {
        PrintNode(node.pattern.get(), "for (", " in ");
        PrintNode(node.inExpression.get());
        PrintNode(node.patternGuard.get());
        PRT().PWI([this, &node] { VisitNode(node.body); }, ") {", "}");
    }
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

void Ast2SourceVisitor::VisitGenericParams(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For GenericParams");
    PRT().PVec<GenericParamDecl>(
        generic->typeParameters, [this](const GenericParamDecl& gpd) { Traverse(gpd, *this); }, ", ", "<", ">");
}

void Ast2SourceVisitor::VisitGenericConstraints(Ptr<Generic> generic)
{
    if (!generic) {
        return;
    }
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For GenericConstraints");
    PRT().PVec<GenericConstraint>(
        generic->genericConstraints, [this](const GenericConstraint& gc) { Traverse(gc, *this); }, ", ", " where ");
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

// Seam Type
void Ast2SourceVisitor::VisitType(const Ptr<Type> type, const Ptr<Ty> ty)
{
    if (type) {
        Logger::Get().Debug("Ast2SourceVisitor::VisitType", "For Type");
        // For desugared node (only Type)
        if (OpenSema() && OpenDesugar() && type->astKind == AstKind::TYPE && type->ty) {
            VisitTy(*type->ty);
        } else {
            PRT().PVal(": ");
            Traverse(*type, *this);
        }
    } else if (OpenSema() && ty) {
        PRT().PVal(": ");
        VisitTy(*ty);
    }
}

void Ast2SourceVisitor::VisitTy(const Ty& ty)
{
    Logger::Get().Debug("Ast2SourceVisitor::VisitTy", "For Ty");
    using TyKind = Cangjie::AST::TypeKind;
    // If the format is incorrect, need to adjust it.
    switch (ty.kind) {
        case TyKind::TYPE_CSTRING:
            PRT().PVal("CString");
            return;
        case TyKind::TYPE_POINTER:
            PRT().PVal("CPointer");
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { VisitTy(argTy); }, ", ", "<", ">");
            return;
        case TyKind::TYPE_CLASS:
        case TyKind::TYPE_INTERFACE:
        case TyKind::TYPE_STRUCT:
        case TyKind::TYPE_ENUM:
            PRT().PVal(ty.name);
            PRT().PVec<Ty>(ty.typeArgs, [this](const Ty& argTy) { VisitTy(argTy); }, ", ", "<", ">");
            return;
        default:
            break;
    };
    PRT().PVal(ty.String());
}

void Ast2SourceVisitor::VisitDecl(const Decl& node)
{
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    PRT().PVec<Modifier>(node.modifiers, [this](const Modifier& mod) { Traverse(mod, *this); }, " ", "", " ");
}

void Ast2SourceVisitor::PrintOverloadCallExpr(const CallExpr& node)
{
    auto fn = node.resolvedFunction;
    auto op = fn->op;
    Logger::Get().Debug("Ast2SourceVisitor::PrintOverloadCallExpr", "For Overload operator: ", Tk2Str(op));
    AH_ASSERT(IsOverloadCall(node));
    auto ma = static_cast<MemberAccess*>(node.baseFunc.get().get());
    auto base = ma->baseExpr.get();
    // 可以重载的操作符有
    if (op == TokenKind::NOT) {
        // 一元 !
        PrintNode(base, "!");
    } else if (op == TokenKind::LSQUARE) {
        // []
        Traverse(*base, *this);
        AH_ASSERT(node.args.size() == 1 || node.args.size() == 2);
        if (node.args.size() == 1) {
            // x[i]
            PrintNode(node.args[0].get(), "[", "]");
        } else {
            // x[i] = v
            PrintNode(node.args[0].get(), "[", "]");
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
        PrintNode(node.args[0].get(), " " + Tk2Str(op) + " ");
    }
    // 打印个注释在这里
    PRT().PVal(" /* Desugared ");
    PrintNode(ma);
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")", true);
    PRT().PVal(" */ ");
}

inline void Ast2SourceVisitor::PrintNode(
    const Ptr<AstNode> pnode, const std::string& pre, const std::string& suf, bool withNL)
{
    if (pnode) {
        PRT().PVal(pre);
        Traverse(*pnode, *this);
        PRT().PVal(suf);
        if (withNL) {
            PRT().PNL();
        }
    }
}

/**
 * for (x in start..stop:step)
 * ============================
 * var $iter-i = startExpr
 * var $stop-compiler = stopExpr
 * while ($iter-i < $stop-compiler) {
 *     let i = $iter-i
 *     foo(i)
 *     $iter-i += stepExpr
 * }
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

    PrintNode(block->body[0].get(), "", "", true); // var $iter-i = startExpr
    PrintNode(block->body[1].get(), "", "", true); // var $stop-compiler = stopExpr
    PrintNode(block->body[3].get(), "while (", ") {", true);
    PRT().Indent();
    PrintNode(varPat->varDecl.get(), "", "", true); // let i = $iter-i;
    Traverse(*node.body, *this);                    // foo(i);
    PrintNode(block->body[2].get(), "", "", true);  // $iter-i += stepExpr;
    PRT().Unindent();
    PRT().PVal("}");
}

/**
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
 */
void Ast2SourceVisitor::PrintDesugaredForInIterator(const ForInExpr& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::PrintDesugaredForInIterator", "For ForInExpr with Iterator");
    AH_CHECK_NULL(node.desugarExpr);
    AH_ASSERT(node.desugarExpr->astKind == AstKind::BLOCK);
    Ptr<Block> block = static_cast<Block*>(node.desugarExpr.get().get());
    AH_ASSERT(block->body.size() == 2);
    // TODO: 确认迭代变量的唯一性
    Traverse(*block, *this);
}

void Ast2SourceVisitor::PrintDesugaredForInString(const ForInExpr& node)
{
}

Printer& Ast2SourceVisitor::PRT()
{
    return prt;
}

bool Ast2SourceVisitor::IsFocused(const Decl& decl) const
{
    if (focusDecls.empty()) {
        return true;
    }
    return focusDecls.find(decl.astKind) != focusDecls.end();
}
