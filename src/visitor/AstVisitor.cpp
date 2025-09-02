/**
 * @file
 *
 * This file implements the AstVisitor.
 */

#include "visitor/AstVisitor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

void AstVisitor::RegisterHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after)
{
    handlers[kind] = {before, visit, after};
}

VisitResult AstVisitor::BeforeVisit(const AstNode& node)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<0>(it->second)) {
        return std::get<0>(it->second)(node);
    } else {
        return DefaultBefore(node);
    }
}

void AstVisitor::Visit(const AstNode& node, VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, res);
    } else {
        return DefaultVisit(node, res);
    }
}

void AstVisitor::AfterVisit(const AstNode& node, const VisitResult& res)
{
    // Logger::Get().Debug("AstVisitor::AfterVisit", "For ", static_cast<int>(node.astKind));
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        return std::get<2>(it->second)(node, res);
    } else {
        return DefaultAfter(node, res);
    }
}

VisitResult AstVisitor::DefaultBefore(const AstNode& node)
{
    return VisitResult::Cont();
}

void AstVisitor::DefaultVisit(const AstNode& node, VisitResult& res)
{
    // 遍历子节点
    for (auto& child : this->GetChildren(node)) {
        Traverse(*child, *this);
    }
}

void AstVisitor::DefaultAfter(const AstNode& node, const VisitResult& res)
{
}
