/**
 * @file
 *
 * This file declares the generic MutAstVisitor.
 */

#pragma once

#include "WrapperAst.h"
#include <functional>
#include <map>
#include <tuple>

class AstNodeVisitor {
public:
    /**
     * @brief 构造函数，初始化 AstNodeVisitor 对象。
     */
    AstNodeVisitor() = default;
    /**
     * @brief 虚析构函数，确保派生类能正确析构
     */
    virtual ~AstNodeVisitor() = default;
    /**
     * @brief 获取节点的子节点列表。
     *
     * @param node 要获取子节点的节点。
     * @return 子节点列表。
     */
    std::vector<Ptr<AstNode>> GetChildren(const AstNode& node);
};
