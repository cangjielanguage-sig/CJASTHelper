/**
 * @file
 *
 * This file implements the MutAstVisitor.
 */

#include "visitor/MutAstVisitor.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

// MutTraverse 实现方法
ValuedResult MutTraverse(AstNode& node, MutAstVisitorBase& visitor)
{
    ValuedResult res = visitor.BeforeVisit(node);
    if (!res.status) {
        return res;
    }
    visitor.Visit(node, res);
    if (res.status) {
        visitor.AfterVisit(node, res);
    }
    return res;
}

void MutAstVisitor::RegisterHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after, MergeFunc merge)
{
    handlers[kind] = {before, visit, after, merge};
}

ValuedResult MutAstVisitor::BeforeVisit(AstNode& node)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<0>(it->second)) {
        return std::get<0>(it->second)(node);
    } else {
        return DefaultBefore(node);
    }
}

void MutAstVisitor::Visit(AstNode& node, ValuedResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        std::get<1>(it->second)(node, res);
    } else {
        DefaultVisit(node, res);
    }
}

void MutAstVisitor::AfterVisit(AstNode& node, const ValuedResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<2>(it->second)) {
        std::get<2>(it->second)(node, res);
    } else {
        DefaultAfter(node, res);
    }
}

void MutAstVisitor::MergeResult(AstNode& node, ValuedResult& res, std::vector<ValuedResult>& childrenRes)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<3>(it->second)) {
        std::get<3>(it->second)(node, res, childrenRes);
    } else {
        DefaultMergeResult(node, res, childrenRes);
    }
}

ValuedResult MutAstVisitor::DefaultBefore(AstNode& node)
{
    return ValuedResult::InitResult();
}

void MutAstVisitor::DefaultVisit(AstNode& node, ValuedResult& res)
{
    auto children = AstNodeHelper::GetChildren(node);
    Logger::Get().Debug("MutAstVisitor::DefaultVisit", AstKind2Str(node.astKind), " : ", children.size());
    std::vector<ValuedResult> childrenRes;
    // 遍历子节点
    for (auto& child : children) {
        childrenRes.push_back(MutTraverse(*child, *this));
    }
    MergeResult(node, res, childrenRes);
}

void MutAstVisitor::DefaultAfter(AstNode& node, const ValuedResult& res)
{
}

void MutAstVisitor::DefaultMergeResult(AstNode& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes)
{
}
