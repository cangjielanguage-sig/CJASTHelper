/**
 * @file
 *
 * This file implements the MutAstVisitor.
 */

#include "visitor/MutAstVisitor.h"

ValuedResult MutAstVisitor::BeforeVisit(AstNode& node)
{
    if (auto fn = handlers.TryGet<BeforeFunc>(node.astKind)) {
        return fn->get()(node);
    } else {
        return DefaultBefore(node);
    }
}

void MutAstVisitor::Visit(AstNode& node, ValuedResult& res)
{
    if (auto fn = handlers.TryGet<VisitFunc>(node.astKind)) {
        fn->get()(node, res);
    } else {
        DefaultVisit(node, res);
    }
}

void MutAstVisitor::AfterVisit(AstNode& node, const ValuedResult& res)
{
    if (auto fn = handlers.TryGet<AfterFunc>(node.astKind)) {
        fn->get()(node, res);
    } else {
        DefaultAfter(node, res);
    }
}

void MutAstVisitor::MergeResult(AstNode& node, ValuedResult& res, std::vector<ValuedResult>& childrenRes)
{
    if (auto fn = handlers.TryGet<MergeFunc>(node.astKind)) {
        fn->get()(node, res, childrenRes);
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
