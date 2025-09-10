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
    res.Set<OwnedNodeValue>(std::move(node.desugarType));
}
