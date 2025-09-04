/**
 * @file
 *
 * This file implements the ConstAstVisitor.
 */

#include "visitor/ConstAstVisitor.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

void ConstAstVisitor::RegisterBeforeHandler(AstKind kind, BeforeFunc before)
{
    beforeHandlers.emplace(kind, before);
}
void ConstAstVisitor::RegisterVisitHandler(AstKind kind, VisitFunc visit)
{
    visitHandlers.emplace(kind, visit);
}
void ConstAstVisitor::RegisterAfterHandler(AstKind kind, AfterFunc after)
{
    afterHandlers.emplace(kind, after);
}
VisitResult ConstAstVisitor::BeforeVisit(const AstNode& node)
{
    auto it = beforeHandlers.find(node.astKind);
    if (it != beforeHandlers.end()) {
        return it->second(node);
    } else {
        return DefaultBefore(node);
    }
}

void ConstAstVisitor::Visit(const AstNode& node, VisitResult& res)
{
    auto it = visitHandlers.find(node.astKind);
    if (it != visitHandlers.end()) {
        it->second(node, res);
    } else {
        DefaultVisit(node, res);
    }
}

void ConstAstVisitor::AfterVisit(const AstNode& node, const VisitResult& res)
{
    auto it = afterHandlers.find(node.astKind);
    if (it != afterHandlers.end()) {
        it->second(node, res);
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
