/**
 * @file
 *
 * This file declares the generic MutAstVisitor.
 */

#pragma once

#include "wrapper/WrapperAst.h"
#include <functional>
#include <map>
#include <tuple>

class AstNodeHelper {
public:
    /**
     * @brief 获取节点的子节点列表。
     *
     * @param node 要获取子节点的节点。
     * @return 子节点列表。
     */
    static std::vector<Ptr<AstNode>> GetChildren(const AstNode& node);
};
