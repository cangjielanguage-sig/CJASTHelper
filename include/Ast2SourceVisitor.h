/**
 * @file
 *
 * This file declares the Ast2SourceVisitor.
 */
#ifndef AST_2_SOURCE_VISITOR_H
#define AST_2_SOURCE_VISITOR_H
#include "AstVisitor.h"
#include "Macro.h"
#include "Printer.h"
#include <fstream>

/**
 * @class Ast2SourceException
 * @brief 自定义异常类，用于处理 `Ast2SourceVisitor` 中的异常。
 */
class Ast2SourceException : public std::exception {
private:
    std::string message; /**< 异常消息 */

public:
    /**
     * @brief 构造函数，初始化异常消息。
     * @param msg 异常消息字符串。
     */
    explicit Ast2SourceException(const std::string& msg) noexcept;

    /**
     * @brief 获取异常消息。
     * @return 异常消息的C字符串。
     */
    const char* what() const noexcept override;
};

/**
 * @typedef Flag
 * @brief 定义标志类型，用于控制功能开关。
 */
using Flag = unsigned char;

/** @brief 解糖标志 */
constexpr Flag DESUGAR_FLAG = 0x1;
/** @brief 语义分析标志 */
constexpr Flag SEMA_FLAG = 0x2;

/**
 * @class Ast2SourceVisitor
 * @brief 继承自 `AstVisitor`，用于将AST转换为源代码。
 */
class Ast2SourceVisitor : public AstVisitor {
public:
    /**
     * @brief 构造函数，初始化输出文件、缩进和标志。
     * @param out 输出文件路径。
     * @param indent 缩进大小，默认为2。
     * @param flags 功能开关标志，默认为0。
     */
    Ast2SourceVisitor(const std::string& out, int indent = 2, Flag flags = 0);

    /**
     * @brief 启用解糖功能。
     */
    inline void EnableDusgar()
    {
        this->flags |= DESUGAR_FLAG;
    }

    /**
     * @brief 启用语义分析功能。
     */
    inline void EnableSeam()
    {
        this->flags |= SEMA_FLAG;
    }

protected:
#define GEN_VISIT_OVERRIDE(N) void Visit(const N& node, VisitResult&) override

    // 递归展开需要重写的节点
    EXPAND2(GEN_VISIT_OVERRIDE, Annotation, Modifier);
    EXPAND1(GEN_VISIT_OVERRIDE, File);
    EXPAND3(GEN_VISIT_OVERRIDE, PackageSpec, ImportSpec, ImportContent);
    // Decl
    EXPAND4(GEN_VISIT_OVERRIDE, FuncDecl, FuncBody, FuncParamList, FuncParam);
    EXPAND4(GEN_VISIT_OVERRIDE, MainDecl, VarDecl, ClassDecl, ClassBody);
    // Type
    EXPAND4(GEN_VISIT_OVERRIDE, PrimitiveType, RefType, OptionType, TupleType);
    EXPAND4(GEN_VISIT_OVERRIDE, QualifiedType, ThisType, VArrayType, ParenType);
    EXPAND2(GEN_VISIT_OVERRIDE, ConstantType, FuncType);
    // Pattern
    EXPAND4(GEN_VISIT_OVERRIDE, WildcardPattern, ConstPattern, EnumPattern, VarPattern);
    EXPAND3(GEN_VISIT_OVERRIDE, TypePattern, VarOrEnumPattern, TuplePattern);
    // Expr
    EXPAND4(GEN_VISIT_OVERRIDE, Block, FuncArg, MatchCase, MatchCaseOther);
    EXPAND3(GEN_VISIT_OVERRIDE, RefExpr, MemberAccess, CallExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, LitConstExpr, ArrayLit, ReturnExpr, LambdaExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, MatchExpr, IsExpr, AsExpr, ThrowExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, AssignExpr, UnaryExpr, IncOrDecExpr, BinaryExpr);
    EXPAND1(GEN_VISIT_OVERRIDE, SubscriptExpr);
    // Generic
    EXPAND3(GEN_VISIT_OVERRIDE, Generic, GenericParamDecl, GenericConstraint);

private:
    /**
     * @brief 获取 `Printer` 实例。
     * @return `Printer` 的引用。
     */
    Printer& PRT();

    using Ty = Cangjie::AST::Ty;     /**< 类型别名，表示AST中的Ty节点。 */
    using Type = Cangjie::AST::Type; /**< 类型别名，表示AST中的Type节点。 */

    /**
     * @brief 访问类型节点。
     * @param type 类型节点指针。
     * @param ty 可选的Ty节点指针。
     */
    void VisitType(const Ptr<Type> type, const Ptr<Ty> ty = nullptr);

    /**
     * @brief 访问Ty节点。
     * @param ty Ty节点的引用。
     */
    void VisitTy(const Ty& ty);

    /**
     * @brief 访问声明节点。
     * @param node 声明节点的引用。
     */
    void VisitDecl(const Decl& node);

    /**
     * @brief 访问泛型参数。
     * @param generic 泛型节点指针。
     */
    void VisitGenericParams(Ptr<Generic> generic);

    /**
     * @brief 访问泛型约束。
     * @param generic 泛型节点指针。
     */
    void VisitGenericConstraints(Ptr<Generic> generic);

    /**
     * @brief 打印重载调用表达式。
     * @param node 调用表达式节点的引用。
     */
    void PrintOverloadCallExpr(const CallExpr& node);

    /**
     * @brief 打印节点。
     * @param pnode 节点指针。
     * @param pre 前缀字符串。
     * @param suf 后缀字符串。
     */
    void PrintNode(const Ptr<AstNode>& pnode, const std::string& pre = "", const std::string& suf = "");

    /**
     * @brief 检查是否启用解糖功能。
     * @return 如果启用返回true，否则返回false。
     */
    inline bool OpenDesugar() const
    {
        return flags & DESUGAR_FLAG;
    }

    /**
     * @brief 检查是否启用语义分析功能。
     * @return 如果启用返回true，否则返回false。
     */
    inline bool OpenSema() const
    {
        return flags & SEMA_FLAG;
    }

private:
    std::string out;  /**< 输出文件路径 */
    std::fstream ofs; /**< 输出文件流 */
    Printer prt;      /**< 打印器实例 */
    Flag flags;       /**< 功能开关标志（解糖、语义等） */
};

#endif // AST_2_SOURCE_VISITOR_H