/**
 * @file
 *
 * This file implementation of Ast2SourceVisitor.
 */
#include "Ast2SourceVisitor.h"
#include "Logger.h"
#include "Macro.h"
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
    Logger::Get().Debug("Ast2SourceVisitor::Before", "For File: ", node.fileName);
    std::string fp = out + "/" + GetFileNameWithoutSuffix(node.fileName) + SUFFIX;
    ofs.open(fp, std::ios::out);
    if (!ofs.is_open()) {
        throw Ast2SourceException("Failed to open file: " + fp);
    }
    return VisitResult::Cont();
}

void Ast2SourceVisitor::After(const File& node, const VisitResult& visitResult)
{
    ofs.close();
}

VisitResult Ast2SourceVisitor::Before(const FuncDecl& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::Before", "For FuncDecl: ", node.identifier.Val());
    AH_CHECK_NULL(node.funcBody);
    GetPrinter().PSVals(" ", "func", Id(node.identifier));
    return VisitResult::Cont();
}

VisitResult Ast2SourceVisitor::Before(const FuncBody& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::Before", "For FuncBody");
    return VisitResult::Cont();
}

VisitResult Ast2SourceVisitor::Before(const FuncParamList& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::Before", "For FuncParamList");
    GetPrinter().Printc<FuncParam>(
        node.params, [this](const FuncParam& param) { traverseAst(param, *this); }, ",", "(", ")", true);
    return VisitResult::Skip();
}

VisitResult Ast2SourceVisitor::Before(const FuncParam& node)
{
    Logger::Get().Debug("Ast2SourceVisitor::Before", "For FuncParam");
    return VisitResult::Cont();
}

Printer& Ast2SourceVisitor::GetPrinter()
{
    return p;
}
