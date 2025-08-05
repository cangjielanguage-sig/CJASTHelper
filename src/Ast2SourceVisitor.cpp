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

std::string Id(const Cangjie::Identifier& id)
{
    return id;
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

Ast2SourceVisitor::Ast2SourceVisitor(const std::string& out, int indent) : out(out), p(ofs, indent)
{
    CreateDirIfNotExists(out);
}

VisitResult Ast2SourceVisitor::Before(const File& node)
{
    Logger::Get(Logger::Mode::STD).Debug("Ast2SourceVisitor::Before", "For File: ", node.fileName);
    std::string fp = out + "/" + GetFileNameWithoutSuffix(node.fileName) + SUFFIX;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    return VisitResult::Cont();
}

void Ast2SourceVisitor::After(const File& node, const VisitResult& res)
{
    ofs.close();
}

void Ast2SourceVisitor::Visit(const FuncDecl& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncDecl: ", node.identifier.Val());
    VisitNodes(node.annotations);
    VisitNode(node.annotationsArray);
    for (auto& mod : node.modifiers) {
        AstVisitor::Visit(mod, res);
    }
    VisitNode(node.generic);
    AH_CHECK_NULL(node.funcBody);
    GetPrinter().PSVals(" ", "func", Id(node.identifier));
}

void Ast2SourceVisitor::Visit(const FuncBody& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncBody");
    VisitNode(node.paramLists[0]);
    VisitNode(node.generic);
    VisitNode(node.retType);
    if (node.body) {
        GetPrinter().PVal(" {").PNL();
        GetPrinter().Indent();
        VisitNode(node.body);
        GetPrinter().Unindent();
        GetPrinter().PVal("}").PNL();
    }
}

void Ast2SourceVisitor::Visit(const FuncParamList& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParamList");
    GetPrinter().Printc<FuncParam>(
        node.params, [this](const FuncParam& param) { traverseAst(param, *this); }, ",", "(", ")", true);
    // Do not traverse children
    res.status = false;
}

void Ast2SourceVisitor::Visit(const FuncParam& node, VisitResult& res)
{
    Logger::Get().Debug("Ast2SourceVisitor::Visit", "For FuncParam");
}

Printer& Ast2SourceVisitor::GetPrinter()
{
    return p;
}
