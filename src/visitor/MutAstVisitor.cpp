/**
 * @file
 *
 * This file implements the MutAstVisitor.
 */

#include "visitor/MutAstVisitor.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

void MutAstVisitor::RegisterBeforeHandler(AstKind kind, BeforeFunc before)
{
    beforeHandlers.emplace(kind, before);
}
void MutAstVisitor::RegisterVisitHandler(AstKind kind, VisitFunc visit)
{
    visitHandlers.emplace(kind, visit);
}
void MutAstVisitor::RegisterAfterHandler(AstKind kind, AfterFunc after)
{
    afterHandlers.emplace(kind, after);
}
void MutAstVisitor::RegisterMergeHandler(AstKind kind, MergeFunc merge)
{
    mergeHandlers.emplace(kind, merge);
}

ValuedResult MutAstVisitor::BeforeVisit(AstNode& node)
{
    auto it = beforeHandlers.find(node.astKind);
    if (it != beforeHandlers.end()) {
        return it->second(node);
    } else {
        return DefaultBefore(node);
    }
}

void MutAstVisitor::Visit(AstNode& node, ValuedResult& res)
{
    auto it = visitHandlers.find(node.astKind);
    if (it != visitHandlers.end()) {
        it->second(node, res);
    } else {
        DefaultVisit(node, res);
    }
}

void MutAstVisitor::AfterVisit(AstNode& node, const ValuedResult& res)
{
    auto it = afterHandlers.find(node.astKind);
    if (it != afterHandlers.end()) {
        it->second(node, res);
    } else {
        DefaultAfter(node, res);
    }
}

void MutAstVisitor::MergeResult(AstNode& node, ValuedResult& res, std::vector<ValuedResult>& childrenRes)
{
    auto it = mergeHandlers.find(node.astKind);
    if (it != mergeHandlers.end()) {
        it->second(node, res, childrenRes);
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
