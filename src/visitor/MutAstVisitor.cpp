/**
 * @file
 *
 * This file implementation of MutAstVisitor.
 */

#include "visitor/MutAstVisitor.h"
#include "cangjie/Utils/CastingTemplate.h"
#include "utils/Logger.h"
#include "utils/Macro.h"
#include <tuple>

// AstKind 2 String
static std::unordered_map<AstKind, std::string> kindsInfo{
#define AST_INFO(KIND, STR, DEF) {AstKind::KIND, STR},
#include "visitor/AstInfo.inc"
#undef AST_INFO
};

MutAstVisitor::MutAstVisitor()
{
    // 定义注册代码片段
}

void MutAstVisitor::RegisterHandler(AstKind kind, BeforeFunc before, VisitFunc visit, AfterFunc after, MergeFunc merge)
{
    handlers[kind] = {before, visit, after, merge};
    Logger::Get().Debug("MutAstVisitor:registerHandler", "For ", kindsInfo[kind]);
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

VisitResult MutAstVisitor::Visit(AstNode& node, VisitResult& res)
{
    auto it = handlers.find(node.astKind);
    if (it != handlers.end() && std::get<1>(it->second)) {
        return std::get<1>(it->second)(node, res);
    } else {
        return DefaultVisit(node, res);
    }
}

VisitResult MutAstVisitor::AfterVisit(AstNode& node, VisitResult& res)
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
    auto defaultRes = VisitResult::Cont();
    std::vector<VisitResult> childrenRes;
    // 遍历子节点
    for (auto& child : GetChildren(node)) {
        childrenRes.push_back(BeforeVisit(*child));
    }
    MergeResult(node, defaultRes, childrenRes);
    return defaultRes;
}

VisitResult MutAstVisitor::DefaultVisit(AstNode& node, VisitResult& res)
{
    std::vector<VisitResult> childrenRes;
    // 遍历子节点
    for (auto& child : GetChildren(node)) {
        childrenRes.push_back(BeforeVisit(*child));
    }
    MergeResult(node, res, childrenRes);
    return res;
}

VisitResult MutAstVisitor::DefaultAfter(AstNode& node, VisitResult& res)
{
    std::vector<VisitResult> childrenRes;
    // 遍历子节点
    for (auto& child : GetChildren(node)) {
        childrenRes.push_back(BeforeVisit(*child));
    }
    MergeResult(node, res, childrenRes);
    return res;
}

void MutAstVisitor::DefaultMergeResult(AstNode& node, VisitResult& base, const std::vector<VisitResult>& childrenRes)
{
}
