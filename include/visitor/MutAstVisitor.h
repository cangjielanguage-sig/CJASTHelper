/**
 * @file
 *
 * This file declares the generic MutAstVisitor.
 */

#pragma once

#include "VisitorBase.h"
#include "utils/CallbackManger.h"
#include <functional>
#include <map>
#include <tuple>

class MutAstVisitor : public MutAstVisitorBase {
public:
    /**
     * @brief 使用 std::function 定义 BeforeVisit 的回调函数类型。
     */
    using BeforeFunc = std::function<ValuedResult(AstNode&)>;
    /**
     * @brief 使用 std::function 定义 Visit 的回调函数类型。
     */
    using VisitFunc = std::function<void(AstNode&, ValuedResult&)>;
    /**
     * @brief 使用 std::function 定义 AfterVisit 的回调函数类型。
     */
    using AfterFunc = std::function<void(AstNode&, const ValuedResult&)>;

    /**
     * @brief 使用 std::function 定义 MergeResult 的回调函数类型。
     */
    using MergeFunc = std::function<void(AstNode&, ValuedResult&, std::vector<ValuedResult>&)>;

public:
    /**
     * @brief 构造函数，初始化 MutAstVisitor 对象。
     */
    MutAstVisitor() = default;
    /**
     * @brief 虚析构函数，确保派生类能正确析构
     */
    ~MutAstVisitor() override = default;

    /**
     * @brief 在访问节点之前调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    ValuedResult BeforeVisit(AstNode& node) override;
    /**
     * @brief 访问节点时调用的方法。
     *
     * @param node 要访问的 AST 节点。
     */
    void Visit(AstNode& node, ValuedResult& res) override;
    /**
     * @brief 在访问节点之后调用的方法。
     *
     * @param node 已访问的 AST 节点。
     */
    void AfterVisit(AstNode& node, const ValuedResult& res) override;

    void RegBefore(AstKind kind, BeforeFunc&& before);
    void RegVisit(AstKind kind, VisitFunc&& visit);
    void RegAfter(AstKind kind, AfterFunc&& after);
    void RegMerge(AstKind kind, MergeFunc&& merge);

protected:
    /**
     * @brief 合并子节点遍历结果。
     *
     * @param node 父节点。
     * @param res 父节点遍历结果初始值。
     * @param childrenRes 子节点的遍历结果对象。
     */
    virtual void MergeResult(AstNode& node, ValuedResult& res, std::vector<ValuedResult>& childrenRes);

    /**
     * @brief 默认的 BeforeVisit 方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    virtual ValuedResult DefaultBefore(AstNode& node);
    /**
     * @brief 默认的 Visit 方法。
     *
     * @param node 要访问的 AST 节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    virtual void DefaultVisit(AstNode& node, ValuedResult& res);
    /**
     * @brief 默认的 AfterVisit 方法。
     *
     * @param node 已访问的 AST 节点。
     * @param res 遍历结果对象，包含访问结果的状态信息。
     */
    virtual void DefaultAfter(AstNode& node, const ValuedResult& res);

    /**
     * @brief 合并子节点的遍历结果。
     *
     * @param node 父节点。
     * @param base 父节点的遍历结果对象，用于存储合并后的结果。
     * @param childrenRes 子节点的遍历结果列表。
     * @return 合并后的遍历结果。
     */
    virtual void DefaultMergeResult(AstNode& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes);

protected:
    /**
     * @brief 存储每个 AST 节点种类对应的处理程序。
     */
    CallbackManager<AstKind, std::tuple<BeforeFunc, VisitFunc, AfterFunc, MergeFunc>> handlers;
};

class ReplaceAstVisitor : public MutAstVisitor {
public:
    ReplaceAstVisitor() = default;

protected:
    /**
     * @brief 合并子节点的遍历结果。
     *
     * @param node 父节点。
     * @param base 父节点的遍历结果对象，用于存储合并后的结果。
     * @param childrenRes 子节点的遍历结果列表。
     * @return 合并后的遍历结果。
     */
    void DefaultMergeResult(AstNode& node, ValuedResult& base, std::vector<ValuedResult>& childrenRes) override;
};