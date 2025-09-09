/**
 * @file
 *
 * This file declares the CheckDesugarPass of CounterAstVisitor.
 */

#pragma once

#include "pass/Pass.h"
#include "visitor/MutAstVisitor.h"

/**
 * CheckDesugarPass
 * Check the count of desugar nodes.
 */
class CheckDesugarPass : public Pass {
public:
    CheckDesugarPass(PassConfig config = PassConfig());
    ~CheckDesugarPass() override = default;

    void Run(AstNode& node) override;

private:
    void Visit(OptionType& node, ValuedResult& res);

private:
    CounterAstVisitor visitor;
};
