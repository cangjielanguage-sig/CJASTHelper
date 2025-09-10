/**
 * @file
 *
 * This file declares the Passes related to sugar.
 */

#pragma once

#include "pass/Pass.h"
#include "visitor/MutAstVisitor.h"

/**
 * DesugarPass
 * Check the count of desugar nodes.
 */
class DesugarPass : public Pass {
public:
    DesugarPass(const PassConfig& config);
    ~DesugarPass() override = default;

protected:
    void RegVisitor(MutAstVisitor& visitor);
    virtual void Visit(OptionType& node, ValuedResult& res) = 0;
    virtual void Visit(Expr& node, ValuedResult& res) = 0;
};

/**
 * CheckDesugarPass
 * Check the count of desugar nodes.
 */
class CheckDesugarPass : public DesugarPass {
public:
    CheckDesugarPass(const PassConfig& config);
    ~CheckDesugarPass() override = default;

    void Run(AstNode& node) override;

protected:
    void Visit(OptionType& node, ValuedResult& res) override;
    void Visit(Expr& node, ValuedResult& res) override;

private:
    CounterAstVisitor visitor;
};

/**
 * ReplaceDesugarPass
 *
 * Replace desugared AST nodes with their original AST nodes.
 * For example, replace `?Int64` with `Option<Int64>` if desugar option is true.
 */
class ReplaceDesugarPass : public DesugarPass {
public:
    ReplaceDesugarPass(const PassConfig& config);
    ~ReplaceDesugarPass() override = default;

    void Run(AstNode& node) override;

protected:
    void Visit(OptionType& node, ValuedResult& res) override;
    void Visit(Expr& node, ValuedResult& res) override;

private:
    ReplaceAstVisitor visitor;
};
