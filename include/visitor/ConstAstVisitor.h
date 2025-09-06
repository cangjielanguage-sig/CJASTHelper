/**
 * @file
 *
 * This file declares the generic AstVisitor.
 */
#pragma once

#include "VisitorBase.h"
#include "utils/CallbackManger.h"
#include <functional>
#include <map>
#include <tuple>

class ConstAstVisitor : public AstVisitorBase {
public:
    /**
     * @brief 使用 std::function 定义 BeforeVisit 的回调函数类型。
     */
    using BeforeFunc = std::function<VisitResult(const AstNode&)>;
    /**
     * @brief 使用 std::function 定义 Visit 的回调函数类型。
     */
    using VisitFunc = std::function<void(const AstNode&, VisitResult&)>;
    /**
     * @brief 使用 std::function 定义 AfterVisit 的回调函数类型。
     */
    using AfterFunc = std::function<void(const AstNode&, const VisitResult&)>;

public:
    /**
     * @brief 构造函数，初始化 ConstAstVisitor 对象。
     */
    ConstAstVisitor() = default;
    /**
     * @brief 虚析构函数，确保派生类能正确析构
     */
    virtual ~ConstAstVisitor() = default;

    /**
     * @brief 在访问节点之前调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    VisitResult BeforeVisit(const AstNode& node) override;
    /**
     * @brief 访问节点时调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    void Visit(const AstNode& node, VisitResult& res) override;
    /**
     * @brief 在访问节点之后调用的方法。
     *
     * @param node 已访问的 AST 节点。
     * @param res 遍历结果对象，包含访问结果的状态信息。
     */
    void AfterVisit(const AstNode& node, const VisitResult& res) override;

    /**
     * @brief 默认的 BeforeVisit 方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    virtual VisitResult DefaultBefore(const AstNode& node);
    /**
     * @brief 默认的 Visit 方法。
     *
     * @param node 要访问的 AST 节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    virtual void DefaultVisit(const AstNode& node, VisitResult& res);
    /**
     * @brief 默认的 AfterVisit 方法。
     *
     * @param node 已访问的 AST 节点。
     * @param res 遍历结果对象，包含访问结果的状态信息。
     */
    virtual void DefaultAfter(const AstNode& node, const VisitResult& res);

    void RegBefore(AstKind kind, BeforeFunc&& before);
    void RegVisit(AstKind kind, VisitFunc&& visit);
    void RegAfter(AstKind kind, AfterFunc&& after);

protected:
    /**
     * @brief 存储每个 AST 节点种类对应的处理程序。
     */
    CallbackManager<AstKind, std::tuple<BeforeFunc, VisitFunc, AfterFunc>> handlers;
};
