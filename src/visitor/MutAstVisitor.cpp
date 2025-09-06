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

void MutAstVisitor::RegBefore(AstKind kind, BeforeFunc&& before)
{
    handlers.Reg<BeforeFunc>(kind, std::forward<BeforeFunc>(before));
}
void MutAstVisitor::RegVisit(AstKind kind, VisitFunc&& visit)
{
    handlers.Reg<VisitFunc>(kind, std::forward<VisitFunc>(visit));
}
void MutAstVisitor::RegAfter(AstKind kind, AfterFunc&& after)
{
    handlers.Reg<AfterFunc>(kind, std::forward<AfterFunc>(after));
}
void MutAstVisitor::RegMerge(AstKind kind, MergeFunc&& merge)
{
    handlers.Reg<MergeFunc>(kind, std::forward<MergeFunc>(merge));
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

void ReplaceAstVisitor::DefaultMergeResult(AstNode& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes)
{
    std::vector<OwnedPtr<AstNode>> children;
    for (auto& child : childrenRes) {
        if (auto val = child.TryGet<OwnedNodeValue>()) {
            children.push_back(std::move(*val));
        }
    }
    AstNodeHelper::ReplaceChildren(node, children);
}
