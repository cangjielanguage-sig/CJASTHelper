/**
 * @file
 *
 * This file declares the ToCangjiePass.
 */
#pragma once

#include "ToSourcePass.h"

/**
 * @class ToCangjiePass
 */
class ToCangjiePass : public ToSourcePass {
public:
    /**
     * @brief 构造函数，初始化输出文件、缩进和标志。
     * @param config 配置对象，包含输出文件、缩进和标志信息。
     */
    ToCangjiePass(const ToSourcePassConfig& config);
    ~ToCangjiePass() override = default;

protected:
#define GEN_BEFORE_OVERRIDE(N) VisitResult Before(const N& node)
#define GEN_VISIT_OVERRIDE(N) void Visit(const N& node, VisitResult&)

    // 递归展开需要重写的解糖节点
    EXPAND4(GEN_BEFORE_OVERRIDE, MainDecl, AssignExpr, UnaryExpr, BinaryExpr);
    EXPAND4(GEN_BEFORE_OVERRIDE, CallExpr, RefExpr, SubscriptExpr, OptionType);
    EXPAND1(GEN_BEFORE_OVERRIDE, MacroDecl);

    // 递归展开需要重写的节点
    EXPAND2(GEN_VISIT_OVERRIDE, Annotation, Modifier);
    EXPAND1(GEN_VISIT_OVERRIDE, File);
    EXPAND3(GEN_VISIT_OVERRIDE, PackageSpec, ImportSpec, ImportContent);
    // Decl
    EXPAND4(GEN_VISIT_OVERRIDE, VarDecl, VarWithPatternDecl, PropDecl, FuncParam);
    EXPAND4(GEN_VISIT_OVERRIDE, FuncParamList, FuncBody, FuncDecl, MainDecl);
    EXPAND4(GEN_VISIT_OVERRIDE, PrimaryCtorDecl, ClassDecl, InterfaceDecl, StructDecl);
    EXPAND4(GEN_VISIT_OVERRIDE, EnumDecl, ExtendDecl, TypeAliasDecl, MacroDecl);
    EXPAND2(GEN_VISIT_OVERRIDE, MacroExpandDecl, BuiltInDecl);
    // Type
    EXPAND4(GEN_VISIT_OVERRIDE, PrimitiveType, RefType, OptionType, TupleType);
    EXPAND4(GEN_VISIT_OVERRIDE, QualifiedType, ThisType, VArrayType, ParenType);
    EXPAND2(GEN_VISIT_OVERRIDE, ConstantType, FuncType);
    // Pattern
    EXPAND4(GEN_VISIT_OVERRIDE, WildcardPattern, ConstPattern, EnumPattern, VarPattern);
    EXPAND4(GEN_VISIT_OVERRIDE, TypePattern, VarOrEnumPattern, TuplePattern, ExceptTypePattern);
    // Expr
    EXPAND4(GEN_VISIT_OVERRIDE, Block, FuncArg, MatchCase, MatchCaseOther);
    EXPAND3(GEN_VISIT_OVERRIDE, RefExpr, MemberAccess, CallExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, LitConstExpr, ArrayLit, ReturnExpr, LambdaExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, MatchExpr, IsExpr, AsExpr, ThrowExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, AssignExpr, UnaryExpr, IncOrDecExpr, BinaryExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, SubscriptExpr, JumpExpr, RangeExpr, LetPatternDestructor);
    EXPAND4(GEN_VISIT_OVERRIDE, IfExpr, DoWhileExpr, WhileExpr, ForInExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, TupleLit, TypeConvExpr, ParenExpr, TryExpr);
    EXPAND3(GEN_VISIT_OVERRIDE, QuoteExpr, TokenPart, MacroExpandExpr);
    EXPAND4(GEN_VISIT_OVERRIDE, WildcardExpr, ArrayExpr, PointerExpr, PrimitiveTypeExpr);
    EXPAND3(GEN_VISIT_OVERRIDE, TrailingClosureExpr, SpawnExpr, SynchronizedExpr);
    EXPAND2(GEN_VISIT_OVERRIDE, InterpolationExpr, StrInterpolationExpr);
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
    template <template <typename> class Ptr, typename T> inline void VisitNodes(ConVec<Ptr<T>>& nodes)
    {
        for (auto& node : nodes) {
            Traverse(*node, visitor);
        }
    }

private:
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
     * @brief 辅助打印MacroInvocation。
     */
    void PrintMacroInvocation(const MacroInvocation& node, const std::string& id);
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
    void PrintInheritedTypes(ConVec<OwnedPtr<Type>>& types);
    /**
     * @brief 辅助打印可继承类型头部。
     */
    void PrintInheritableDeclHeader(const InheritableDecl& node, const std::string& keyword);

    /**
     * @brief 辅助打印一组声明。
     */
    void PrintDecls(ConVec<OwnedPtr<Decl>>& decls);

    /**
     * @brief 辅助打印可继承类型定义体。
     */
    void PrintInheritableDeclBody(ConVec<OwnedPtr<Decl>>& members);

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
    void PrintType(const Type& type);
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
};
