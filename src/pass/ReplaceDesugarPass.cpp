/**
 * @file
 *
 * This file implements the ReplaceDesugarPass.
 */

#include "pass/ReplaceDesugarPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

ReplaceDesugarPass::ReplaceDesugarPass(const PassConfig& config) : Pass(config)
{
    visitor.RegVisit(
        AstKind::OPTION_TYPE, [this](AstNode& node, ValuedResult& res) { Visit(Cast<OptionType&>(node), res); });
    visitor.RegVisit(
        AstKind::TRAIL_CLOSURE_EXPR, [this](AstNode& node, ValuedResult& res) { Visit(Cast<Expr&>(node), res); });
}

void ReplaceDesugarPass::Run(AstNode& node)
{
    // AstNodeHelper::DumpAst(node, "input.txt");
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
