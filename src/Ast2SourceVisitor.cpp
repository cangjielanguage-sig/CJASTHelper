/**
 * @file
 *
 * This file implementation of Ast2SourceVisitor.
 */
#include "Ast2SourceVisitor.h"
#include "Logger.h"
#include <filesystem>

namespace fs = std::filesystem;

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
    return id;
}

inline std::string Tk2Str(Cangjie::TokenKind tk)
{
    return Cangjie::TOKENS[static_cast<int>(tk)];
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

VisitResult Ast2SourceVisitor::Before(const File& node)
{
    Logger::Get(Logger::Mode::STD)
        .Debug("Ast2SourceVisitor::Before", "For File: ", node.fileName, ", decls: ", node.decls.size());
    std::string fp = out + "/" + GetFileNameWithoutSuffix(node.fileName) + SUFFIX;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    return VisitResult::Cont();
}

void Ast2SourceVisitor::After(const File& node, const VisitResult&)
{
    ofs.close();
}

void Ast2SourceVisitor::Visit(const Annotation& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Annotation");
    PRT().PVals("@", Id(node.identifier)).PNL();
}

void Ast2SourceVisitor::Visit(const Modifier& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Modifier");
    PRT().PVal(Tk2Str(node.modifier));
}

void Ast2SourceVisitor::Visit(const FuncDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncDecl: ", node.identifier.Val());
    VisitDecl(node);
    PRT().PSVals(" ", "func", Id(node.identifier));
    AH_CHECK_NULL(node.funcBody);
    Visit(*node.funcBody, res);
}

void Ast2SourceVisitor::Visit(const MainDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For MainDecl");
    if (node.desugarDecl) {
        Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Desugared Decl of MainDecl");
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
    // let x: ty
    PRT().PVals(pre, " ", Id(node.identifier));
    VisitType(node.type.get(), node.ty);
    if (node.initializer) {
        PRT().PVal(" = ");
        Traverse(*node.initializer, *this);
    }
}

void Ast2SourceVisitor::Visit(const FuncBody& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncBody");
    VisitNode(node.paramLists[0]);
    VisitNode(node.generic);
    // To add sema type (optional)
    VisitType(node.retType);
    if (node.body) {
        PRT().PVal(" {").PNL();
        PRT().Indent();
        VisitNode(node.body);
        PRT().Unindent();
        PRT().PVal("}").PNL();
    }
}

void Ast2SourceVisitor::Visit(const FuncParamList& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParamList: ", node.params.size());
    PRT().Printc<FuncParam>(
        node.params, [this, &res](const FuncParam& param) { Visit(param, res); }, ", ", "(", ")", true);
}

void Ast2SourceVisitor::Visit(const FuncParam& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParam");
    PRT().PVal(Id(node.identifier));
    VisitType(node.type.get(), node.ty);
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
    PRT().PVal(Id(node.ref.identifier));
    PRT().Printc<AstNode>(node.typeArguments, [this](const AstNode& node) { Traverse(node, *this); }, ", ", "<", ">");
}

// Expr
void Ast2SourceVisitor::Visit(const Block& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For Block: ", node.body.size());
    PRT().Printc<AstNode>(node.body, [this](const AstNode& node) {
        Traverse(node, *this);
        PRT().PNL();
    });
}

void Ast2SourceVisitor::Visit(const RefExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For RefExpr");
    PRT().PVal(Id(node.ref.identifier));
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
    VisitNode(node.baseFunc);
    PRT().Printc<FuncArg>(node.args, [this](const FuncArg& arg) { Traverse(arg, *this); }, ", ", "(", ")", true);
}

void Ast2SourceVisitor::Visit(const ReturnExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For ReturnExpr");
    PRT().PVal("return");
    if (node.expr) {
        PRT().PVal(" ");
        VisitNode(node.expr);
    }
}

void Ast2SourceVisitor::Visit(const LitConstExpr& node, VisitResult&)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For LitConstExpr");
    PRT().PVal(node.stringValue);
}

// Seam Type
void Ast2SourceVisitor::VisitType(const Ptr<Type> type, const Ptr<Ty> ty)
{
    if (type) {
        Logger::Get().Debug("Ast2SourceVisitor::VisitType", "For Type");
        PRT().PVal(": ");
        Traverse(*type, *this);
    } else if (ty && ty->kind != Cangjie::AST::TypeKind::TYPE_INITIAL) {
        PRT().PVal(": ");
        VisitTy(*ty);
    }
}

void Ast2SourceVisitor::VisitTy(const Ty& ty)
{
    Logger::Get().Debug("Ast2SourceVisitor::VisitTy", "For Ty");
    // If the format is incorrect, need to adjust it.
    PRT().PVal(ty.String());
}

void Ast2SourceVisitor::VisitDecl(const Decl& node)
{
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    PRT().Printc<Modifier>(node.modifiers, [this](const Modifier& mod) { Traverse(mod, *this); }, " ", "", " ");
    VisitNode(node.generic);
}

Printer& Ast2SourceVisitor::PRT()
{
    return prt;
}
