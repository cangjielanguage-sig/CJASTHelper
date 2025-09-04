/**
 * @file
 *
 * This file implements the ConstAstVisitor.
 */

#include "visitor/ConstAstVisitor.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

void ConstAstVisitor::RegisterHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after)
{
    handlers[kind] = {before, visit, after};
}

VisitResult ConstAstVisitor::BeforeVisit(const AstNode& node)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<0>(it->second)) {
        return std::get<0>(it->second)(node);
    } else {
        return DefaultBefore(node);
    }
}

void ConstAstVisitor::Visit(const AstNode& node, VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, res);
    } else {
        return DefaultVisit(node, res);
    }
}

void ConstAstVisitor::AfterVisit(const AstNode& node, const VisitResult& res)
{
    // Logger::Get().Debug("ConstAstVisitor::AfterVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        return std::get<2>(it->second)(node, res);
    } else {
        return DefaultAfter(node, res);
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
