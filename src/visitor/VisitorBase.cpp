/**
 * @file
 *
 * This file implementation of AstVisitorBase.
 */

#include "utils/Logger.h"
#include "visitor/AstVisitorBase.h"
#include "visitor/MutAstVisitorBase.h"

VisitResult::VisitResult(bool cont) : status(cont)
{
}

VisitResult VisitResult::Cont()
{
    return {true};
}

VisitResult VisitResult::Skip()
{
    return {false};
}

VisitResult Traverse(const AstNode& node, AstVisitorBase& visitor)
{
    auto res = visitor.BeforeVisit(node);
    if (!res.status) {
        return res;
    }
    visitor.Visit(node, res);
    if (res.status) {
        visitor.AfterVisit(node, res);
    }
    return res;
}

VisitResult MutTraverse(AstNode& node, MutAstVisitorBase& visitor)
{
    VisitResult res = visitor.BeforeVisit(node);
    if (!res.status) {
        return res;
    }
    res = visitor.Visit(node, res);
    if (res.status) {
        res = visitor.AfterVisit(node, res);
    }
    return res;
}

namespace {
template <typename T>
inline void CollectChildren(const std::vector<OwnedPtr<T>>& nodes, std::vector<Ptr<AstNode>>& children)
{
    for (auto& node : nodes) {
        children.push_back(node);
    }
}

template <typename T> inline void CollectChildren(const OwnedPtr<T>& node, std::vector<Ptr<AstNode>>& children)
{
    if (node) {
        children.push_back(node);
    }
}

std::unordered_map<AstKind, std::function<void(AstNode&, std::vector<Ptr<AstNode>>&)>> getChildrenMap = {
    {AstKind::PACKAGE,
        [](AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(static_cast<const Package&>(node).files, children);
        }},
    {AstKind::PACKAGE_SPEC,
        [](AstNode& node, std::vector<Ptr<AstNode>>& children) {
            CollectChildren(static_cast<const PackageSpec&>(node).modifier, children);
        }},
};

} // namespace
std::vector<Ptr<AstNode>> AstNodeVisitor::GetChildren(AstNode& node)
{
    std::vector<Ptr<AstNode>> result;
    if (auto fn = getChildrenMap.find(node.astKind); fn != getChildrenMap.end()) {
        fn->second(node, result);
    } else {
        Logger::Get().Warn("AstNodeVisitor::GetChildren", "unregistered kind ", static_cast<int>(node.astKind));
    }
    return result;
}
