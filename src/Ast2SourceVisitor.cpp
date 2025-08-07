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
using Cangjie::AST::Attribute;
using Cangjie::AST::CallKind;
using Cangjie::AST::Expr;
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

inline std::string Id(const Cangjie::Identifier& id)
{
    std::string res = id.Val();
    if (res.find("$") == 0) {
        res = res.substr(1);
    } else if (res == "v-compiler") {
        res = "tmp";
    }
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

bool IsCallInit(const CallExpr& node)
{
    if (node.callKind == CallKind::CALL_STRUCT_CREATION || node.callKind == CallKind::CALL_OBJECT_CREATION ||
        // 处理编译器生成的错误类型节点
        node.callKind == CallKind::CALL_INVALID) {
        return TryGetCallRef(node) == "init";
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

Ast2SourceVisitor::Ast2SourceVisitor(const std::string& out, int indent) : out(out), prt(ofs, indent)
{
    CreateDirIfNotExists(out);
}

void Ast2SourceVisitor::Visit(const File& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For File");
    std::string fp = out + "/" + GetFileNameWithoutSuffix(node.fileName) + SUFFIX;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    PRT().PVec<Decl>(node.decls, [this](const Decl& decl) {
        Traverse(decl, *this);
        PRT().PNL(2);
    });
    ofs.close();
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
    PRT().PPtr<Block>(node.body,
        [this](const Block& block) { PRT().PValNL(" {").PWI([this, &block] { Traverse(block, *this); }).PVal("}"); });
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
    if (node.desugarDecl) {
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

void Ast2SourceVisitor::Visit(const VarDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarDecl: ", node.identifier.Val());
    VisitDecl(node);
    std::string pre = "let";
    if (node.isConst) {
        pre = "const";
    } else if (node.isVar) {
        pre = "var";
    }
    PRT().PVals(pre, " ", Id(node.identifier));
    VisitType(node.type.get(), node.ty);
    PRT().PPtr<Expr>(node.initializer, [this](const Expr& expr) { Traverse(expr, *this); }, " = ");
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
    PRT().PPtr<Type>(node.componentType, [this](const Type& tp) { Traverse(tp, *this); }, "?");
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
    PRT().PPtr<Type>(node.type, [this](const Type& tp) { Traverse(tp, *this); }, "(", ")");
}

void Ast2SourceVisitor::Visit(const ConstantType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ConstantType");
    PRT().PPtr<Expr>(node.constantExpr, [this](const Expr& expr) { Traverse(expr, *this); }, "$");
}

void Ast2SourceVisitor::Visit(const FuncType& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncType");
    PRT().PVec<Type>(node.paramTypes, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "(", ")", true);
    PRT().PPtr<Type>(node.retType, [this](const Type& tp) { Traverse(tp, *this); }, " -> ");
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
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarPattern");
    PRT().PVal(Id(node.varDecl->identifier));
}

void Ast2SourceVisitor::Visit(const VarOrEnumPattern& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For VarOrEnumPattern");
    VisitNode(node.pattern);
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
}

// Expr
void Ast2SourceVisitor::Visit(const Block& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Block: ", node.body.size());
    PRT().PVec<AstNode>(node.body, [this](const AstNode& node) {
        Traverse(node, *this);
        PRT().PNL();
    });
}

void Ast2SourceVisitor::Visit(const RefExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For RefExpr: ", node.ref.identifier.Val());
    PRT().PVal(Id(node.ref.identifier));
    PRT().PVec<Type>(node.typeArguments, [this](const Type& tp) { Traverse(tp, *this); }, ", ", "<", ">");
}

void Ast2SourceVisitor::Visit(const BinaryExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For BinaryExpr");
    VisitNode(node.leftExpr);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void Ast2SourceVisitor::Visit(const FuncArg& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncArg");
    VisitNode(node.expr);
}

void Ast2SourceVisitor::Visit(const CallExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For CallExpr");
    if (IsCallInit(node)) {
        AH_CHECK_NULL(node.ty);
        // 构造函数调用 init() -> A(), TODO: 应该缩小下范围， 构造函数内的init不需要替换
        VisitTy(*node.ty);
    } else {
        VisitNode(node.baseFunc);
    }
    PRT().PVec<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")", true);
}

void Ast2SourceVisitor::Visit(const ReturnExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ReturnExpr");
    // return in init, skip
    auto body = node.refFuncBody;
    if (body && body->funcDecl && body->funcDecl->TestAttr(Attribute::CONSTRUCTOR)) {
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
    PRT().PValNL(" =>").PWI([this, &body] { VisitNode(body.body); }).PVal("}");
}

void Ast2SourceVisitor::Visit(const MatchCase& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchCase");
    PRT().PVal("case ");
    PRT().PVec<Pattern>(node.patterns, [this](const Pattern& pat) { Traverse(pat, *this); }, " | ");
    PRT().PPtr<Expr>(node.patternGuard, [this](const Expr& expr) { Traverse(expr, *this); }, " where ");
    PRT().PValNL(" =>").PWI([this, &node] { VisitNode(node.exprOrDecls); });
}

void Ast2SourceVisitor::Visit(const MatchCaseOther& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchCaseOther");
    PRT().PPtr<Expr>(node.matchExpr, [this](const Expr& expr) { Traverse(expr, *this); }, "case ", " =>");
    PRT().PWI([this, &node] { VisitNode(node.exprOrDecls); });
}

void Ast2SourceVisitor::Visit(const MatchExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MatchExpr");
    PRT().PVal("match ");
    // match with selector
    PRT().PPtr<Expr>(node.selector, [this](const Expr& expr) { Traverse(expr, *this); }, "(", ")");
    PRT().PValNL(" {").Indent();
    PRT().PVec<MatchCase>(node.matchCases, [this](const MatchCase& mc) { Traverse(mc, *this); });
    PRT().PVec<MatchCaseOther>(node.matchCaseOthers, [this](const MatchCaseOther& mco) {
        PRT().PNL();
        Traverse(mco, *this);
    });
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
    VisitNode(node.leftValue);
    PRT().PVals(" ", Tk2Str(node.op), " ");
    VisitNode(node.rightExpr);
}

void Ast2SourceVisitor::Visit(const ThrowExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ThrowExpr");
    PRT().PVal("throw ");
    VisitNode(node.expr);
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
        PRT().PVal(": ");
        // For desugared node (only Type)
        if (type->astKind == AstKind::TYPE && type->ty) {
            VisitTy(*type->ty);
        } else {
            Traverse(*type, *this);
        }
    } else if (ty && ty->kind != Cangjie::AST::TypeKind::TYPE_INITIAL) {
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

Printer& Ast2SourceVisitor::PRT()
{
    return prt;
}
