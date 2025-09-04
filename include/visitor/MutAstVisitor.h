/**
 * @file
 *
 * This file declares the generic MutAstVisitor.
 */

#pragma once

#include "VisitorBase.h"
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
     * @brief 注册处理程序以处理特定类型的 AST 节点。
     *
     * @param kind AST 节点的种类。
     * @param before 访问节点之前的回调函数。
     * @param visit 访问节点时的回调函数。
     * @param after 访问节点之后的回调函数。
     * @param merge MergeResult回调函数。
     */
    void RegisterBeforeHandler(AstKind kind, BeforeFunc before);
    void RegisterVisitHandler(AstKind kind, VisitFunc visit);
    void RegisterAfterHandler(AstKind kind, AfterFunc after);
    void RegisterMergeHandler(AstKind kind, MergeFunc merge);
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
    std::unordered_map<AstKind, BeforeFunc> beforeHandlers;
    std::unordered_map<AstKind, VisitFunc> visitHandlers;
    std::unordered_map<AstKind, AfterFunc> afterHandlers;
    std::unordered_map<AstKind, MergeFunc> mergeHandlers;
};
