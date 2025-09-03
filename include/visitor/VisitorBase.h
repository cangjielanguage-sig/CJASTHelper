/**
 * @file
 *
 * This file declares the basic struct of AstVisitor.
 */
#pragma once

#include "wrapper/WrapperAst.h"

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

// 抽象访问器接口 (const 版本)
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

using DefaultValue = int;
using OwnedNodeValue = OwnedPtr<AstNode>;
using ValueType = std::variant<DefaultValue, OwnedNodeValue>;
// ResultType 类型定义, 可扩展。
template <typename T>
concept ResultType = std::same_as<T, DefaultValue> || std::same_as<T, OwnedNodeValue>;

// 定义带值遍历结果结构体
struct ValuedResult : VisitResult {
    template <ResultType T> ValuedResult(T&& v, bool status = true) : VisitResult(status), data(std::forward<T>(v))
    {
    }

    static ValuedResult InitResult()
    {
        return ValuedResult(0);
    }

    static ValuedResult SkipResult()
    {
        return ValuedResult(0, false);
    }

    template <ResultType T> inline T* TryGet()
    {
        return std::get_if<T>(&data);
    }

    template <ResultType T> inline void Set(T&& v)
    {
        this->data = std::forward<T>(v);
    }

    ValueType data;
};

// 抽象访问器接口 （mut 版本）
class MutAstVisitorBase {
public:
    MutAstVisitorBase() = default;
    /**
     * @brief 在访问节点之前调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @return 遍历结果，指示是否继续遍历或跳过节点。
     */
    virtual ValuedResult BeforeVisit(AstNode& node) = 0;
    /**
     * @brief 访问节点时调用的方法。
     *
     * @param node 要访问的 AST 节点。
     * @param res 遍历结果对象，用于传递状态信息。
     */
    virtual void Visit(AstNode& node, ValuedResult& res) = 0;
    /**
     * @brief 在访问节点之后调用的方法。
     *
     * @param node 已访问的 AST 节点。
     * @param res 遍历结果对象，包含访问结果的状态信息。
     */
    virtual void AfterVisit(AstNode& node, const ValuedResult& res) = 0;

    /**
     * @brief 虚析构函数，确保派生类正确销毁。
     */
    virtual ~MutAstVisitorBase() = default;
};

/**
 * @brief 通用遍历接口，递归地遍历 AST 节点并应用访问器。
 *
 * @param node 要遍历的 AST 节点 (拥有所有权的节点, 支持修改)。
 * @param visitor 访问器对象，实现了 MutAstVisitorBase 接口。
 * @return 遍历结果，指示整个遍历过程的状态。
 */
ValuedResult MutTraverse(AstNode& node, MutAstVisitorBase& visitor);
