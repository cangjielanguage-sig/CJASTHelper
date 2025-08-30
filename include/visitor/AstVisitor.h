/**
 * @file
 *
 * This file declares the generic AstVisitor.
 */
#pragma once

#include "AstVisitorBase.h"
#include <functional>
#include <map>
#include <tuple>

class AstVisitor : public AstVisitorBase {
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
     * @brief 构造函数，初始化 AstVisitor 对象。
     */
    AstVisitor();
    /**
     * @brief 虚析构函数，确保派生类能正确析构
     */
    virtual ~AstVisitor() = default;
    /**
     * @brief 注册处理程序以处理特定类型的 AST 节点。
     *
     * @param kind AST 节点的种类。
     * @param before 访问节点之前的回调函数。
     * @param visit 访问节点时的回调函数，默认为 nullptr。
     * @param after 访问节点之后的回调函数，默认为 nullptr。
     */
    void registerHandler(AstKind kind, BeforeFunc before, VisitFunc visit = nullptr, AfterFunc after = nullptr);
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

protected:
    // 宏定义
#define GEN_BEFORE_AFTER_VISIT(N)                                                                                      \
    virtual VisitResult Before(const N& node)                                                                          \
    {                                                                                                                  \
        return VisitResult::Cont();                                                                                    \
    }                                                                                                                  \
    virtual void After(const N& node, const VisitResult& res)                                                          \
    {                                                                                                                  \
    }                                                                                                                  \
    virtual void Visit(const N& node, VisitResult& res);

    // 使用宏生成代码
#define AST_INFO(KIND, STR, DEF) GEN_BEFORE_AFTER_VISIT(DEF)
#include "AstInfo.inc"
#undef AST_INFO

    /**
     * @brief 处理声明节点。
     *
     * @param node 声明节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    void VisitDecl(const Decl& node, VisitResult& res);

    /**
     * @brief 遍历单个节点。
     *
     * @tparam Ptr 指针类型模板。
     * @tparam T 节点的具体类型。
     * @param pnode 要遍历的节点指针。
     */
    template <template <typename> class Ptr, typename T> inline void VisitNode(const Ptr<T>& pnode)
    {
        if (pnode) {
            Traverse(*pnode, *this);
        }
    }

    /**
     * @brief 遍历一组节点。
     *
     * @tparam Ptr 指针类型模板。
     * @tparam T 节点的具体类型。
     * @param nodes 要遍历的节点指针数组。
     */
    template <template <typename> class Ptr, typename T> inline void VisitNodes(const std::vector<Ptr<T>>& nodes)
    {
        for (auto& node : nodes) {
            Traverse(*node, *this);
        }
    }

protected:
    /**
     * @brief 存储每个 AST 节点种类对应的处理程序。
     */
    std::map<AstKind, std::tuple<BeforeFunc, VisitFunc, AfterFunc>> handlers;
};
