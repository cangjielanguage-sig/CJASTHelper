/**
 * @file
 *
 * This file declares the generic MutAstVisitor.
 */

#pragma once

#include "MutAstVisitorBase.h"
#include <functional>
#include <map>
#include <tuple>

class MutAstVisitor : public MutAstVisitorBase {
public:
    /**
     * @brief 使用 std::function 定义 BeforeVisit 的回调函数类型。
     */
    using BeforeFunc = std::function<VisitResult(AstNode&)>;
    /**
     * @brief 使用 std::function 定义 Visit 的回调函数类型。
     */
    using VisitFunc = std::function<void(AstNode&, VisitResult&)>;
    /**
     * @brief 使用 std::function 定义 AfterVisit 的回调函数类型。
     */
    using AfterFunc = std::function<void(AstNode&, const VisitResult&)>;

    /**
     * @brief 使用 std::function 定义 MergeResult 的回调函数类型。
     */
    using MergeFunc = std::function<void(AstNode&, VisitResult&, const std::vector<VisitResult>&)>;

public:
    /**
     * @brief 构造函数，初始化 MutAstVisitor 对象。
     */
    MutAstVisitor() = default;
    /**
     * @brief 虚析构函数，确保派生类能正确析构
     */
    virtual ~MutAstVisitor() = default;
    /**
     * @brief 注册处理程序以处理特定类型的 AST 节点。
     *
     * @param kind AST 节点的种类。
     * @param before 访问节点之前的回调函数。
     * @param visit 访问节点时的回调函数，默认为 nullptr。
     * @param after 访问节点之后的回调函数，默认为 nullptr。
     * @param merge MergeResult回调函数， 默认为 nullptr。
     */
    void RegisterHandler(AstKind kind, BeforeFunc before, VisitFunc visit = nullptr, AfterFunc after = nullptr,
        MergeFunc merge = nullptr);
    /**
     * @brief 在访问节点之前调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    VisitResult BeforeVisit(AstNode& node) override;
    /**
     * @brief 访问节点时调用的方法。
     *
     * @param node 要访问的 AST 节点。
     */
    void Visit(AstNode& node, VisitResult& res) override;
    /**
     * @brief 在访问节点之后调用的方法。
     *
     * @param node 已访问的 AST 节点。
     */
    void AfterVisit(AstNode& node, const VisitResult& res) override;

protected:
    /**
     * @brief 合并子节点遍历结果。
     *
     * @param node 父节点。
     * @param res 父节点遍历结果初始值。
     * @param childrenRes 子节点的遍历结果对象。
     */
    virtual void MergeResult(AstNode& node, VisitResult& res, const std::vector<VisitResult>& childrenRes);

    /**
     * @brief 默认的 BeforeVisit 方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    virtual VisitResult DefaultBefore(AstNode& node);
    /**
     * @brief 默认的 Visit 方法。
     *
     * @param node 要访问的 AST 节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    virtual void DefaultVisit(AstNode& node, VisitResult& res);
    /**
     * @brief 默认的 AfterVisit 方法。
     *
     * @param node 已访问的 AST 节点。
     * @param res 遍历结果对象，包含访问结果的状态信息。
     */
    virtual void DefaultAfter(AstNode& node, const VisitResult& res);

    /**
     * @brief 合并子节点的遍历结果。
     *
     * @param node 父节点。
     * @param base 父节点的遍历结果对象，用于存储合并后的结果。
     * @param childrenRes 子节点的遍历结果列表。
     * @return 合并后的遍历结果。
     */
    virtual void DefaultMergeResult(AstNode& node, VisitResult& base, const std::vector<VisitResult>& childrenRes);

protected:
    /**
     * @brief 存储每个 AST 节点种类对应的处理程序。
     */
    std::unordered_map<AstKind, std::tuple<BeforeFunc, VisitFunc, AfterFunc, MergeFunc>> handlers;
};
