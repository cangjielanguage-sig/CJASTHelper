/**
 * @file
 *
 * This file implements the ReplaceDesugarPass.
 */

#include "pass/ReplaceDesugarPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

ReplaceDesugarPass::ReplaceDesugarPass(PassConfig config) : config(config)
{
    handlers.Reg<VisitFunc>(
        AstKind::OPTION_TYPE, [this](AstNode& node, ValuedResult& res) { Visit(Cast<OptionType&>(node), res); });
}

void ReplaceDesugarPass::Visit(OptionType& node, ValuedResult& res)
{
    Logger::Get().Debug("ReplaceDesugarPass::Visit", "OptionType");
    res.Set<OwnedNodeValue>(std::move(node.desugarType));
}
