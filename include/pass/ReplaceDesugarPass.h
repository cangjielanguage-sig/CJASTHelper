/**
 * @file
 *
 * This file declares the ReplaceDesugarPass of MutAstVisitor.
 */

#pragma once

#include "pass/PassConfig.h"
#include "visitor/MutAstVisitor.h"

/**
 * ReplaceDesugarPass
 *
 * Replace desugared AST nodes with their original AST nodes.
 * For example, replace `?Int64` with `Option<Int64>` if desugar option is true.
 */
class ReplaceDesugarPass : public MutAstVisitor {
public:
    ReplaceDesugarPass(PassConfig config = PassConfig());
    ~ReplaceDesugarPass() override = default;

public:
    void Visit(OptionType& node, ValuedResult& res);

    void Merge(FuncDecl& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes);
    void Merge(VarDecl& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes);

private:
    PassConfig config;
};
