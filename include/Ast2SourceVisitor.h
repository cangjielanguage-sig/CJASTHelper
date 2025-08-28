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
 * @class Ast2SourceConfig
 * @brief 配置 `Ast2SourceVisitor` 的参数。
 */
class Ast2SourceConfig {
public:
    Ast2SourceConfig();

    /**
     * @brief 检查是否启用解糖功能。
     *     开启解糖打印解糖后的代码， 否则打印源代码 (还原解糖前的代码)。
     * @return 如果启用返回true，否则返回false。
     */
    bool Desugar() const;
    /**
     * @brief 检查是否启用语义分析功能。
     * @return 如果启用返回true，否则返回false。
     */
    bool Sema() const;
    /**
     * @brief 检查是否关注特定的声明类型。
     * @param decl 声明。
     * @return 如果关注返回true，否则返回false。
     */
    bool Focus(const Decl& decl) const;

public:
    /**
     * @typedef Flag
     * @brief 定义标志类型，用于控制功能开关。
     */
    using Flag = unsigned char;

    /** @brief 解糖标志 */
    static constexpr Flag DESUGAR_FLAG = 0x1;
    /** @brief 语义分析标志 */
    static constexpr Flag SEMA_FLAG = 0x2;

    int indent;                             /**< 输出缩进大小 */
    std::string out;                        /**< 输出文件路径 */
    std::string suffix;                     /**< 输出文件后缀 */
    Flag flags;                             /**< 功能开关标志（解糖、语义等） */
    std::unordered_set<AstKind> focusDecls; /**< 关注的顶层声明类型 */
    // 可配置属性: Attribute::C, Attribute::INTRINSIC, ...
    std::unordered_set<std::string> focusAttrs;        /**< 关注的注解对应的属性列表 */
    std::unordered_set<std::string> ignoreAnnotations; /**< 忽略的注解对应的属性列表 */
    std::unordered_set<std::string> ignoreDecls;       /**< 忽略的顶层声明列表 */
};

/**
 * @class Ast2SourceVisitor
 * @brief 继承自 `AstVisitor`，用于将AST转换为源代码。
 */
class Ast2SourceVisitor : public AstVisitor {
public:
    ~Ast2SourceVisitor() override = default;

protected:
#define GEN_BEFORE_OVERRIDE(N) VisitResult Before(const N& node) override
#define GEN_VISIT_OVERRIDE(N) void Visit(const N& node, VisitResult&) override

    // 递归展开需要重写的解糖节点
    EXPAND4(GEN_BEFORE_OVERRIDE, MainDecl, AssignExpr, UnaryExpr, BinaryExpr);
    EXPAND2(GEN_BEFORE_OVERRIDE, RefExpr, SubscriptExpr);

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

private:
    friend class Ast2SourceVisitorBuilder;
    /**
     * @brief 构造函数，初始化输出文件、缩进和标志。
     * @param config Ast2SourceConfig对象，包含输出文件、缩进和标志信息。
     */
    Ast2SourceVisitor(Ast2SourceConfig config);

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
    void PrintInheritedTypes(const std::vector<OwnedPtr<Cangjie::AST::Type>>& types);
    /**
     * @brief 辅助打印可继承类型头部。
     */
    void PrintInheritableDeclHeader(const Cangjie::AST::InheritableDecl& node, const std::string& keyword);

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
    void PrintInstArgs(const Cangjie::AST::NameReferenceExpr& ref, bool isPattern = false);
    /**
     * @brief 辅助打印 变量的类型标注。
     */
    void PrintVarType(const Cangjie::AST::VarDeclAbstract& node);
    /**
     * @brief 辅助打印 Type 节点。
     */
    bool TryPrintType(const Ptr<Cangjie::AST::Type> type);
    /**
     * @brief 辅助打印 Ty 标注。
     */
    bool TryPrintTy(const Ptr<Cangjie::AST::Ty> ty);
    /**
     * @brief 辅助打印 Ty 语义信息。
     */
    void PrintTy(const Cangjie::AST::Ty& ty);
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

private:
    Ast2SourceConfig config;                                         /**< 配置对象 */
    std::fstream ofs;                                                /**< 输出文件流 */
    Printer prt;                                                     /**< 打印器实例 */
    std::unordered_map<Ptr<const Decl>, std::string> desugaredVarId; /**< 解糖变量名字表 */
};

/**
 * @class Ast2SourceVisitorBuilder
 * @brief 构建 `Ast2SourceVisitor` 的辅助类。
 */
class Ast2SourceVisitorBuilder {
public:
    Ast2SourceVisitorBuilder() = default;
    /**
     * @brief 设置输出文件路径。
     * @param out 输出文件路径。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    Ast2SourceVisitorBuilder& Output(const std::string& out);
    /**
     * @brief 设置输出文件后缀。
     * @param suffix 输出文件后缀名。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    Ast2SourceVisitorBuilder& Suffix(const std::string& suffix);
    /**
     * @brief 设置输出缩进大小。
     * @param indent 缩进大小。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    Ast2SourceVisitorBuilder& Indent(int indent);
    /**
     * @brief 启用解糖功能。
     */
    Ast2SourceVisitorBuilder& EnableDesugar();
    /**
     * @brief 启用语义分析功能。
     */
    Ast2SourceVisitorBuilder& EnableSema();
    /**
     * @brief 设置关注的顶层声明类型。
     * @param kinds 关注的声明类型名称列表。
     */
    Ast2SourceVisitorBuilder& Focus(const std::vector<std::string>& kinds);
    /**
     * @brief 设置关注的注解属性。
     * @param attrs 关注的注解属性名称列表。
     */
    Ast2SourceVisitorBuilder& FocusAttrs(const std::vector<std::string>& attrs);
    /**
     * @brief 设置忽略的顶层声明。
     * @param decls 忽略的声明标识符列表。
     */
    Ast2SourceVisitorBuilder& IgnoreDecls(const std::vector<std::string>& decls);
    /**
     * @brief 设置忽略的注解。
     * @param annos 忽略的注解名称列表。
     */
    Ast2SourceVisitorBuilder& IgnoreAnnotations(const std::vector<std::string>& annos);
    /**
     * @brief 构建 `Ast2SourceVisitor` 实例。
     * @return 返回构建好的 `Ast2SourceVisitor` 实例。
     */
    Ast2SourceVisitor Build();

private:
    Ast2SourceConfig config;
};

#endif // AST_2_SOURCE_VISITOR_H