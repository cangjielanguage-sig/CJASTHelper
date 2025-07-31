/**
 * @file
 *
 * This file declares the generic AstVisitor.
 */
#ifndef AST_VISITOR_BASE_H
#define AST_VISITOR_BASE_H

#include "cangjie/AST/Node.h"

using AstNode = Cangjie::AST::Node;

// VisitResult
struct VisitResult {
    bool status;
    VisitResult(bool status);
    static VisitResult Cont();
    static VisitResult Skip();
};

// 抽象访问器接口
class AstVisitorBase {
public:
    virtual VisitResult BeforeVisit(const AstNode& node) = 0;
    virtual void VisitChildren(const AstNode& node, VisitResult& visitResult) = 0;
    virtual void AfterVisit(const AstNode& node, const VisitResult& visitResult) = 0;
    virtual ~AstVisitorBase() = default;
};

// 通用遍历接口
VisitResult traverseAst(const AstNode& node, AstVisitorBase& visitor);

#endif // AST_VISITOR_BASE_H
