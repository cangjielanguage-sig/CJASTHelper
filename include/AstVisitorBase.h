/**
 * @file
 *
 * This file declares the generic AstVisitor.
 */
#ifndef AST_VISITOR_BASE_H
#define AST_VISITOR_BASE_H

#include "cangjie/AST/Node.h"

using AstNode = Cangjie::AST::Node;

// 定义遍历结果结构体
struct VisitResult {
    /**
     * @brief 构造函数，初始化状态。
     *
     * @param status 遍历状态，true 表示继续遍历，false 表示停止遍历。
     */
    VisitResult(bool status);
    /**
     * @brief 获取一个表示继续遍历的结果。
     *
     * @return 表示继续遍历的 VisitResult 对象。
     */
    static VisitResult Cont();
    /**
     * @brief 获取一个表示跳过当前节点的结果。
     *
     * @return 表示跳过当前节点的 VisitResult 对象。
     */
    static VisitResult Skip();
    bool status; // 遍历状态，true 表示继续遍历，false 表示停止遍历
};

// 抽象访问器接口
class AstVisitorBase {
public:
    /**
     * @brief 在访问节点之前调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    virtual VisitResult BeforeVisit(const AstNode& node) = 0;
    /**
     * @brief 访问节点时调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    virtual void Visit(const AstNode& node, VisitResult& res) = 0;
    /**
     * @brief 在访问节点之后调用的方法。
     *
     * @param node 已访问的 AST 节点。
     * @param res 遍历结果对象，包含访问结果的状态信息。
     */
    virtual void AfterVisit(const AstNode& node, const VisitResult& res) = 0;
    /**
     * @brief 虚析构函数，确保派生类正确销毁。
     */
    virtual ~AstVisitorBase() = default;
};

/**
 * @brief 通用遍历接口，递归地遍历 AST 节点并应用访问器。
 *
 * @param node 要遍历的 AST 节点。
 * @param visitor 访问器对象，实现了 AstVisitorBase 接口。
 * @return 遍历结果，指示整个遍历过程的状态。
 */
VisitResult Traverse(const AstNode& node, AstVisitorBase& visitor);

#endif // AST_VISITOR_BASE_H
