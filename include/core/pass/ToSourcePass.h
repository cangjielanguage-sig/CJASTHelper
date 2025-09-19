/**
 * @file
 *
 * This file declares the ToSourcePass.
 */
#pragma once

#include "core/pass/Pass.h"
#include "core/visitor/ConstAstVisitor.h"
#include "utils/Macro.h"
#include "utils/Printer.h"
#include <fstream>

/**
 * @class ToSourcePassConfig
 */
class ToSourcePassConfig : public PassConfig {
public:
    ToSourcePassConfig();
    ~ToSourcePassConfig() override = default;

    /**
     * @brief 检查是否关注特定的声明类型。
     * @param decl 声明。
     * @return 如果关注返回true，否则返回false。
     */
    bool Focus(const Decl& decl) const;

    /**
     * @brief 设置输出文件路径。
     * @param out 输出文件路径。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    ToSourcePassConfig& Output(const std::string& out);
    /**
     * @brief 设置输出文件后缀。
     * @param suffix 输出文件后缀名。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    ToSourcePassConfig& Suffix(const std::string& suffix);

    /**
     * @brief 设置输出缩进大小。
     * @param indent 缩进大小。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    ToSourcePassConfig& Indent(int indent);

    /**
     * @brief 设置关注的注解属性。
     * @param attrs 关注的注解属性名称列表。
     */
    ToSourcePassConfig& FocusAnnotationAttrs(const std::vector<std::string>& attrs);
    /**
     * @brief 设置忽略的顶层声明。
     * @param decls 忽略的声明标识符列表。
     */
    ToSourcePassConfig& IgnoreDecls(const std::unordered_set<std::string>& decls);
    /**
     * @brief 设置忽略的注解。
     * @param annos 忽略的注解名称列表。
     */
    ToSourcePassConfig& IgnoreAnnotations(const std::unordered_set<std::string>& annos);
    /**
     * @brief 设置关注的顶层声明类型。
     * @param kinds 关注的声明类型名称列表。
     */
    ToSourcePassConfig& Focus(const std::unordered_set<std::string>& kinds);
    /**
     * @brief 设置关注的修饰符属性。
     * @param attrs 关注的修饰符属性名称列表。
     * @param kinds 关注的修饰符属性所在的声明类型名称列表（白名单）。
     */
    ToSourcePassConfig& FocusModifierAttrs(
        const std::vector<std::string>& attrs, const std::vector<std::string>& kinds);

    int indent;         /**< 输出缩进大小 */
    std::string out;    /**< 输出文件路径 */
    std::string suffix; /**< 输出文件后缀 */
    // 可配置属性: Attribute::C, Attribute::INTRINSIC, ...
    std::unordered_set<std::string> focusAnnotationAttrs; /**< 关注的注解对应的属性列表 */
    // 可配置属性: Attribute::PUBLIC, ...
    std::unordered_set<std::string> focusModifierAttrs; /**< 关注的修饰符对应的属性列表 */
    std::unordered_set<AstKind> focusModifierWhiteList; /**< 关注的语义后修饰符的节点白名单 */
    std::unordered_set<std::string> ignoreAnnotations;  /**< 忽略的注解对应的属性列表 */
    std::unordered_set<std::string> ignoreDecls;        /**< 忽略的顶层声明列表 */
    std::unordered_set<AstKind> focusDecls;             /**< 关注的顶层声明类型 */
};

/**
 * @class ToSourcePass
 * @brief 继承自 `ConstAstVisitor`，用于将AST转换为源代码。
 */
class ToSourcePass : public Pass {
public:
    /**
     * @brief 构造函数，初始化输出文件、缩进和标志。
     * @param config Ast2SourceConfig对象，包含输出文件、缩进和标志信息。
     */
    ToSourcePass(const ToSourcePassConfig& config);
    ~ToSourcePass() override = default;

    void Run(AstNode& node) override;

protected:
#define GEN_BEFORE_OVERRIDE(N) VisitResult Before(const N& node)
#define GEN_VISIT_OVERRIDE(N) void Visit(const N& node, VisitResult&)

    // 递归展开需要重写的解糖节点
    EXPAND4(GEN_BEFORE_OVERRIDE, MainDecl, AssignExpr, UnaryExpr, BinaryExpr);
    EXPAND3(GEN_BEFORE_OVERRIDE, RefExpr, SubscriptExpr, OptionType);

    // 递归展开需要重写的节点
    EXPAND2(GEN_VISIT_OVERRIDE, Annotation, Modifier);
    EXPAND1(GEN_VISIT_OVERRIDE, File);
    EXPAND3(GEN_VISIT_OVERRIDE, PackageSpec, ImportSpec, ImportContent);
    // Decl
    EXPAND4(GEN_VISIT_OVERRIDE, VarDecl, VarWithPatternDecl, PropDecl, FuncParam);
    EXPAND4(GEN_VISIT_OVERRIDE, FuncParamList, FuncBody, FuncDecl, MainDecl);
    EXPAND4(GEN_VISIT_OVERRIDE, PrimaryCtorDecl, ClassDecl, InterfaceDecl, StructDecl);
    EXPAND3(GEN_VISIT_OVERRIDE, EnumDecl, ExtendDecl, TypeAliasDecl);
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
    EXPAND4(GEN_VISIT_OVERRIDE, SubscriptExpr, JumpExpr, RangeExpr, LetPatternDestructor);
    EXPAND4(GEN_VISIT_OVERRIDE, IfExpr, DoWhileExpr, WhileExpr, ForInExpr);
    EXPAND2(GEN_VISIT_OVERRIDE, TupleLit, TypeConvExpr);
    // Generic
    EXPAND3(GEN_VISIT_OVERRIDE, Generic, GenericParamDecl, GenericConstraint);

protected:
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
            Traverse(*pnode, visitor);
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
            Traverse(*node, visitor);
        }
    }

private:
    friend class ToSourcePassBuilder;

    void RegisterHandlers();

private:
    /// 辅助打印函数
    /**
     * @brief 辅助打印节点。
     */
    void TryPrintNode(
        const Ptr<AstNode> pnode, const std::string& pre = "", const std::string& suf = "", bool withNL = false);
    /**
     * @brief 辅助打印声明节点。
     */
    void PrintDecl(const Decl& node);
    /**
     * @brief 辅助打印注解列表。
     */
    void PrintAnnotations(const Decl& node);
    /**
     * @brief 辅助打印修饰符列表。
     */
    void PrintModifiers(const Decl& node);
    /**
     * @brief 辅助打印block。
     */
    void PrintBlock(const Ptr<Block> pnode);
    /**
     * @brief 尝试作为构造函数打印。
     */
    bool TryPrintConstructor(const FuncDecl& node);
    /**
     * @brief 尝试作为getter or setter打印。
     */
    bool TryPrintGetter(const FuncDecl& node);
    bool TryPrintSetter(const FuncDecl& node);
    /**
     * @brief 尝试作为Enum构造器打印。
     */
    bool TryPrintEnumConstructor(const VarDecl& node);
    bool TryPrintEnumConstructor(const FuncDecl& node);

    /**
     * @brief 辅助打印继承类型。
     */
    void PrintInheritedTypes(const std::vector<OwnedPtr<Type>>& types);
    /**
     * @brief 辅助打印可继承类型头部。
     */
    void PrintInheritableDeclHeader(const InheritableDecl& node, const std::string& keyword);

    /**
     * @brief 辅助打印一组声明。
     */
    void PrintDecls(const std::vector<OwnedPtr<Decl>>& decls);

    /**
     * @brief 辅助打印可继承类型定义体。
     */
    void PrintInheritableDeclBody(const std::vector<OwnedPtr<Decl>>& members);

    /**
     * @brief 辅助打印泛型参数。
     */
    void TryPrintGenericParams(Ptr<Generic> generic);
    /**
     * @brief 辅助打印泛型约束。
     */
    void TryPrintGenericConstraints(Ptr<Generic> generic);
    /**
     * @brief 尝试打印泛型实例参数。
     */
    void PrintInstArgs(const NameReferenceExpr& ref, bool isPattern = false);
    /**
     * @brief 辅助打印 变量的类型标注。
     */
    void PrintVarType(const VarDeclAbstract& node);
    /**
     * @brief 辅助打印 Type 节点。
     */
    bool TryPrintType(const Ptr<Type> type);
    /**
     * @brief 辅助打印 Ty 标注。
     */
    bool TryPrintTy(const Ptr<Ty> ty);
    /**
     * @brief 辅助打印 Ty 语义信息。
     */
    void PrintTy(const Ty& ty);
    /**
     * @brief 尝试还原解糖后的调用表达式。
     */
    bool TryRecoverCallExpr(const CallExpr& node);
    /**
     * @brief 尝试还原构造函数调用表达式。
     */
    bool TryPrintInitCall(const CallExpr& node);
    /**
     * @brief 尝试还原重载调用表达式。
     */
    bool TryRecoverOverloadCallExpr(const CallExpr& node);
    /**
     * @brief 尝试还原解糖后属性调用表达式。
     */
    bool TryRecoverPropCallExpr(const CallExpr& node);
    /**
     * @brief 尝试打印解糖后的for-in表达式。
     */
    bool TryPrintDesugaredForInExpr(const ForInExpr& node);
    /**
     * @brief 打印解糖后的 For-In 表达式（范围形式）。
     */
    void PrintDesugaredForInRange(const ForInExpr& node);
    /**
     * @brief 打印解糖后的 For-In 表达式（迭代器形式）。
     */
    void PrintDesugaredForInIterator(const ForInExpr& node);
    /**
     * @brief 打印解糖后的 For-In 表达式（字符串形式）。
     */
    void PrintDesugaredForInString(const ForInExpr& node);

    /**
     * @brief 获取 `Printer` 实例。
     * @return `Printer` 的引用。
     */
    Printer& PRT();

    const ToSourcePassConfig& Config() const;

private:
    std::fstream ofs;                                                /**< 输出文件流 */
    Printer prt;                                                     /**< 打印器实例 */
    std::unordered_map<Ptr<const Decl>, std::string> desugaredVarId; /**< 解糖变量名字表 */
    ConstAstVisitor visitor;                                         /**< 抽象语法树遍历器 */
};
