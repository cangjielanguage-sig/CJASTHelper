/**
 * @file
 *
 * This file implements the ConstAstVisitor.
 */

#include "visitor/ConstAstVisitor.h"

VisitResult ConstAstVisitor::BeforeVisit(const AstNode& node)
{
    if (auto fn = handlers.TryGet<BeforeFunc>(node.astKind)) {
        return fn->get()(node);
    } else {
        return DefaultBefore(node);
    }
}

void ConstAstVisitor::Visit(const AstNode& node, VisitResult& res)
{
    if (auto fn = handlers.TryGet<VisitFunc>(node.astKind)) {
        fn->get()(node, res);
    } else {
        DefaultVisit(node, res);
    }
}

void ConstAstVisitor::AfterVisit(const AstNode& node, const VisitResult& res)
{
    if (auto fn = handlers.TryGet<AfterFunc>(node.astKind)) {
        fn->get()(node, res);
    } else {
        DefaultAfter(node, res);
    }
}

VisitResult ConstAstVisitor::DefaultBefore(const AstNode& node)
{
    return VisitResult::Cont();
}

void ConstAstVisitor::DefaultVisit(const AstNode& node, VisitResult& res)
{
    // 遍历子节点
    for (auto& child : AstNodeHelper::GetChildren(node)) {
        Traverse(*child, *this);
    }
}

void ConstAstVisitor::DefaultAfter(const AstNode& node, const VisitResult& res)
{
}

void ConstAstVisitor::RegBefore(AstKind kind, BeforeFunc&& before)
{
    handlers.Reg<BeforeFunc>(kind, std::forward<BeforeFunc>(before));
}
void ConstAstVisitor::RegVisit(AstKind kind, VisitFunc&& visit)
{
    handlers.Reg<VisitFunc>(kind, std::forward<VisitFunc>(visit));
}
void ConstAstVisitor::RegAfter(AstKind kind, AfterFunc&& after)
{
    handlers.Reg<AfterFunc>(kind, std::forward<AfterFunc>(after));
}
