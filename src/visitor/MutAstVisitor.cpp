/**
 * @file
 *
 * This file implements the MutAstVisitor.
 */

#include "visitor/MutAstVisitor.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

void MutAstVisitor::RegisterHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after, MergeFunc merge)
{
    handlers[kind] = {before, visit, after, merge};
}

VisitResult MutAstVisitor::BeforeVisit(AstNode& node)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<0>(it->second)) {
        return std::get<0>(it->second)(node);
    } else {
        return DefaultBefore(node);
    }
}

void MutAstVisitor::Visit(AstNode& node, VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, res);
    } else {
        return DefaultVisit(node, res);
    }
}

void MutAstVisitor::AfterVisit(AstNode& node, const VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        return std::get<2>(it->second)(node, res);
    } else {
        return DefaultAfter(node, res);
    }
}

void MutAstVisitor::MergeResult(AstNode& node, VisitResult& res, const std::vector<VisitResult>& childrenRes)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        std::get<3>(it->second)(node, res, childrenRes);
    } else {
        DefaultMergeResult(node, res, childrenRes);
    }
}

VisitResult MutAstVisitor::DefaultBefore(AstNode& node)
{
    return VisitResult::Cont();
}

void MutAstVisitor::DefaultVisit(AstNode& node, VisitResult& res)
{
    Logger::Get().Debug("MutAstVisitor::DefaultVisit", AstKind2Str(node.astKind));
    std::vector<VisitResult> childrenRes;
    // 遍历子节点
    for (auto& child : AstNodeHelper::GetChildren(node)) {
        childrenRes.push_back(MutTraverse(*child, *this));
    }
    MergeResult(node, res, childrenRes);
}

void MutAstVisitor::DefaultAfter(AstNode& node, const VisitResult& res)
{
}

void MutAstVisitor::DefaultMergeResult(AstNode& node, VisitResult& base, const std::vector<VisitResult>& childrenRes)
{
}
