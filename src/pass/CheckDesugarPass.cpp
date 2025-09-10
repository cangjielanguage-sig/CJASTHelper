/**
 * @file
 *
 * This file implements the CheckDesugarPass.
 */

#include "pass/CheckDesugarPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

CheckDesugarPass::CheckDesugarPass(const PassConfig& config) : Pass(config)
{
    visitor.RegVisit(
        AstKind::OPTION_TYPE, [this](AstNode& node, ValuedResult& res) { Visit(Cast<OptionType&>(node), res); });

    visitor.RegVisit(
        AstKind::TRAIL_CLOSURE_EXPR, [this](AstNode& node, ValuedResult& res) { Visit(Cast<Expr&>(node), res); });
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
