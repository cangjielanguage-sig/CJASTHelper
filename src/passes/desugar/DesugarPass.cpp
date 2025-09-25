/**
 * @file
 *
 * This file implements the CheckDesugarPass.
 */

#include "DesugarPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

// 注意：这里使用 c++20 inline static 避免在cpp文件中全局变量初始化不被执行问题
REG_PASS("replace-desugar", ([](const PassConfig& config) { return UniquePtr<Pass>(new ReplaceDesugarPass{config}); }));
REG_PASS("check-desugar", ([](const PassConfig& config) { return UniquePtr<Pass>(new CheckDesugarPass{config}); }));

/// DesugarPass

DesugarPass::DesugarPass(const PassConfig& config) : Pass(config)
{
}

void DesugarPass::RegVisitor(MutAstVisitor& visitor)
{
    visitor.RegVisit(
        AstKind::OPTION_TYPE, [this](AstNode& node, ValuedResult& res) { Visit(Cast<OptionType&>(node), res); });

    visitor.RegVisit(
        AstKind::TRAIL_CLOSURE_EXPR, [this](AstNode& node, ValuedResult& res) { Visit(Cast<Expr&>(node), res); });
}

/// CheckDesugarPass
CheckDesugarPass::CheckDesugarPass(const PassConfig& config) : DesugarPass(config)
{
    RegVisitor(visitor);
}

void CheckDesugarPass::Run(AstNode& node)
{
    DEBUG();
    auto res = MutTraverse(node, visitor);
    DEBUG("res: ", *res.TryGet<DefaultValue>());
}

void CheckDesugarPass::Visit(OptionType& node, ValuedResult& res)
{
    DEBUG("OptionType");
    if (node.desugarType) {
        res.Set<DefaultValue>(1);
    }
}

void CheckDesugarPass::Visit(Expr& node, ValuedResult& res)
{
    DEBUG("Expr");
    if (node.desugarExpr) {
        res.Set<DefaultValue>(1);
    }
}

/// ReplaceDesugarPass

ReplaceDesugarPass::ReplaceDesugarPass(const PassConfig& config) : DesugarPass(config)
{
    RegVisitor(visitor);
}

void ReplaceDesugarPass::Run(AstNode& node)
{
    DEBUG();
    MutTraverse(node, visitor);
}

void ReplaceDesugarPass::Visit(OptionType& node, ValuedResult& res)
{
    DEBUG("OptionType");
    if (node.desugarType) {
        res.Set<OwnedNodeValue>(std::move(node.desugarType));
    }
}

void ReplaceDesugarPass::Visit(Expr& node, ValuedResult& res)
{
    DEBUG("Expr");
    if (node.desugarExpr) {
        res.Set<OwnedNodeValue>(std::move(node.desugarExpr));
    }
}
