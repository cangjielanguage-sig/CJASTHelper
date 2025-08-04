/**
 * @file
 *
 * This file declares the Ast2SourceVisitor.
 */
#ifndef AST_2_SOURCE_VISITOR_H
#define AST_2_SOURCE_VISITOR_H
#include "AstVisitor.h"
#include "Printer.h"
#include <fstream>

// 定义自定义异常类
class Ast2SourceException : public std::exception {
private:
    std::string message;

public:
    explicit Ast2SourceException(const std::string& msg) noexcept;
    const char* what() const noexcept override;
};

class Ast2SourceVisitor : public AstVisitor {
public:
    Ast2SourceVisitor(const std::string& out, int indent = 2);

    VisitResult Before(const File& node) override;
    void After(const File& node, const VisitResult& visitResult) override;

    VisitResult Before(const FuncDecl& node) override;
    VisitResult Before(const FuncBody& node) override;
    VisitResult Before(const FuncParamList& node) override;
    VisitResult Before(const FuncParam& node) override;

private:
    Printer& GetPrinter();

private:
    std::string out;
    std::fstream ofs;
    Printer p;
};

#endif // AST_2_SOURCE_VISITOR_H