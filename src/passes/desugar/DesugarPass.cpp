/**
 * @file
 *
 * This file implements the CheckDesugarPass.
 */

#include "DesugarPass.h"
#include "utils/Cast.h"
#include "utils/Logger.h"

REG_PASS("replace-desugar", ([](const PassConfig& config) { return UniquePtr<Pass>(new ReplaceDesugarPass{config}); }));
REG_PASS("recover-desugar", ([](const PassConfig& config) { return UniquePtr<Pass>(new RecoverDesugarPass{config}); }));
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
    LOGD();
    auto res = MutTraverse(node, visitor);
    LOGD("res: ", *res.TryGet<DefaultValue>());
}

void CheckDesugarPass::Visit(OptionType& node, ValuedResult& res)
{
    LOGD("OptionType");
    if (node.desugarType) {
        res.Set<DefaultValue>(1);
    }
}

void CheckDesugarPass::Visit(Expr& node, ValuedResult& res)
{
    LOGD("Expr");
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
    LOGD();
    MutTraverse(node, visitor);
}

void ReplaceDesugarPass::Visit(OptionType& node, ValuedResult& res)
{
    LOGD("OptionType");
    if (node.desugarType) {
        res.Set<OwnedNodeValue>(std::move(node.desugarType));
    }
}

void ReplaceDesugarPass::Visit(Expr& node, ValuedResult& res)
{
    LOGD("Expr");
    if (node.desugarExpr) {
        res.Set<OwnedNodeValue>(std::move(node.desugarExpr));
    }
}

/// RecoverDesugarPass

RecoverDesugarPass::RecoverDesugarPass(const PassConfig& config) : DesugarPass(config)
{
    RegVisitor(visitor);
    handlers.Reg<RecoverFunc>(
        AstKind::TRAIL_CLOSURE_EXPR, [this](AstNode& node) { Recover(Cast<TrailingClosureExpr&>(node)); });
}

void RecoverDesugarPass::Run(AstNode& node)
{
    LOGD();
    MutTraverse(node, visitor);
}

void RecoverDesugarPass::Visit(OptionType& node, ValuedResult& res)
{
    LOGD("OptionType");
    if (node.desugarType) {
        node.desugarType.reset();
    }
}

void RecoverDesugarPass::Visit(Expr& node, ValuedResult& res)
{
    LOGD("Expr");
    if (!node.desugarExpr) {
        return;
    }
    if (auto fn = handlers.TryGet<RecoverFunc>(node.astKind)) {
        fn->get()(node);
    }
}

void RecoverDesugarPass::Recover(TrailingClosureExpr& node)
{
    LOGD("TrailingClosureExpr");
}
