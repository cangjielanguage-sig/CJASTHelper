/**
 * @file
 *
 * This file declares the AstTreePrintPass.
 */
#pragma once

#include "cangjie/AST/Node.h"
#include "core/pass/Pass.h"
#include "core/visitor/ConstAstVisitor.h"
#include "utils/Cast.h"
#include "utils/FileHelper.h"
#include "utils/Printer.h"
#include <fstream>

/**
 * @class AstTreePrintPass
 * @brief 按 golden 结构打印 AST 树（一包一文件：`<out>/<fullPackageName>.tree`）。
 *
 * 遍历：复用 `ConstAstVisitor` —— 注册 Before/Visit/After 钩子驱动整棵树；
 * 缩进：复用仓库 `Printer` 的 `Indent()/Unindent()`（步长取配置 indent，默认 4）；
 * 词面：取 `AstInfo.inc` 第三字段（禁自造词表）；类型名取 `TypeKind.inc` 第三字段。
 *
 * 本 pass 只读遍历，不改动 AST，亦不持有旁路状态。
 */
class AstTreePrintPass : public Pass {
public:
    /**
     * @brief 构造。
     * @param config 配置（out 输出目录、indent 缩进步长）。
     */
    explicit AstTreePrintPass(const ToSourcePassConfig& config) : Pass(config), prt(ofs, Config().indent)
    {
        RegisterHandlers();
    }
    ~AstTreePrintPass() override = default;

    /** @brief 执行打印：一包一文件（多源文件按序拼接于同一文件）。 */
    void Run(AstNode& node) override;

protected:
    /** @brief 取打印器实例。 */
    Printer& PRT()
    {
        return prt;
    }

    /** @brief 取 ToSource 配置（与既有 pass 同形）。 */
    const ToSourcePassConfig& Config() const
    {
        return Cast<const ToSourcePassConfig&>(config);
    }

    /**
     * @brief Before 钩子：出行（词 ＋ 名 ＋ 留白 ＋ 定位 ＋ 修饰符）并按需增加缩进。
     * @param node 当前节点。
     * @return 恒为 Cont（透明节点亦需下探子级，只是不出行）。
     */
    VisitResult BeforeNode(const AstNode& node);

    /**
     * @brief After 钩子：按需减少缩进并刷新缓存。
     * @param node 当前节点。
     */
    void AfterNode(const AstNode& node);

    /** @brief 透明节点：不出行、不进层，仅透传子级。 */
    bool IsTransparentKind(const AstNode& node) const;

    /** @brief 是否枚举构造符上下文（golden：Constructor 行不报名，其形参不出行）。 */
    bool IsCtorContext(const AstNode& node) const;

    /** @brief 当前词面（含 golden 例外：枚举体成员 ⇒ Constructor；值数组 ⇒ VArrayExpr）。 */
    const char* WordOf(const AstNode& node) const;

    /** @brief 行余部：名 / 留白 / 定位 / 修饰符（按节点种类取）。 */
    Str Detail(const AstNode& node, bool topLevel);

    /** @brief 常规子级下探（序即源序）。 */
    void WalkChildren(const AstNode& node);

    /** @brief 类型标注自补（Decl 类型面；带指针去重）。 */
    void AppendTypeAnnotation(const AstNode& node);

    /** @brief 成员访问类型实参自补（带指针去重）。 */
    void AppendMemberTypeArgs(const AstNode& node);
    /**
     * @brief 语义类型后缀 `type=<…>`（sema 后有效；parse 阶段 ty 为初始无效 ⇒ 空 ✓）
     *
     * 目的：让**结构化 AST ＋ 语义类型**一次成型（含解糖节点），作为对齐基线 ✓
     */
    Str TySuffix(const AstNode& node);
    /**
     * @brief 语义类型渲染成**用户面名字**（`std.core.String` / `Array<UInt8>` / `(A)->B` / `T`）
     *
     * `Ty::String()` 给的是**唯一名**（`Struct-String`/`Class-Object`）⇒ 不能直接当基线 ✗
     */
    Str TyText(const Cangjie::AST::Ty* ty);

    /** @brief 导入项 content 自补（收集器未纳入）。 */
    void Visit(const AstNode& node);

    // ── 按种类 Visit（Visit 重载；注册处薄 lambda 转发至此）──

    /** @brief File：缺省包头补行 ＋ 直接子级顶层留白（子级序见 WalkFileChildren）。 */
    void Visit(const File& node, VisitResult& res);
    /** @brief FuncBody：golden 子级序 返回类型 ⇒ 形参列表 ⇒ 函数体。 */
    void Visit(const FuncBody& node, VisitResult& res);
    /** @brief LambdaExpr：体内 Block 扁平（不出 Block 行），形参列表照出。 */
    void Visit(const LambdaExpr& node, VisitResult& res);
    /** @brief TypeConvExpr：golden 序 表达式 ⇒ 类型。 */
    void Visit(const TypeConvExpr& node, VisitResult& res);
    /** @brief IsExpr：表达式 ＋ 目标类型（shiftType）两面都出。 */
    void Visit(const IsExpr& node, VisitResult& res);
    /** @brief AsExpr：表达式 ＋ 目标类型（shiftType）两面都出。 */
    void Visit(const AsExpr& node, VisitResult& res);
    /** @brief ArrayExpr：值数组只出实参（不出类型子树）。 */
    void Visit(const ArrayExpr& node, VisitResult& res);
    /** @brief VarWithPatternDecl：通配模式不出模式行，直接出初始化式。 */
    void Visit(const VarWithPatternDecl& node, VisitResult& res);
    /** @brief VarPattern：不出内层 VarDecl 行，直接出其子级。 */
    void Visit(const VarPattern& node, VisitResult& res);
    /** @brief TuplePattern：叶（golden 下不出现元素行）。 */
    void Visit(const TuplePattern& node, VisitResult& res);
    /** @brief ConstPattern：叶。 */
    void Visit(const ConstPattern& node, VisitResult& res);
    /** @brief LetPatternDestructor：不出模式子级。 */
    void Visit(const LetPatternDestructor& node, VisitResult& res);
    /** @brief ForInExpr：不出模式子级。 */
    void Visit(const ForInExpr& node, VisitResult& res);
    /** @brief ImportSpec：子级为嵌套条目（词面 ImportContent），末尾补 content 行。 */
    void Visit(const ImportSpec& node, VisitResult& res);
    /** @brief ImportContent：内容行（收集器未纳入，由 ImportSpec 处自补调用）。 */
    void Visit(const ImportContent& node, VisitResult& res);
    /** @brief EnumDecl：直接子级为构造符（ctor）语境。 */
    void Visit(const EnumDecl& node, VisitResult& res);
    /** @brief GenericConstraint：只出上界（左端类型不出）。 */
    void Visit(const GenericConstraint& node, VisitResult& res);
    void Visit(const TryExpr& node, VisitResult& res);
    /** @brief VarDecl：类型标注排在子级之后。 */
    void Visit(const VarDecl& node, VisitResult& res);
    /** @brief FuncParam：类型标注排在子级之后。 */
    void Visit(const FuncParam& node, VisitResult& res);
    /** @brief PropDecl：类型在先、访问器（get/set）在后。 */
    void Visit(const PropDecl& node, VisitResult& res);
    /** @brief VArrayType：只下探元素类型（尺寸已由词面 `$n` 表达）。 */
    void Visit(const VArrayType& node, VisitResult& res);
    /** @brief MemberAccess：末尾补类型实参。 */
    void Visit(const MemberAccess& node, VisitResult& res);

    // ── 私有助手（非钩子；不复用 Visit 前缀，避免与重载混淆）──

    /** @brief File 直接子级遍历；对花括号导入（IMPORT_MULTI）做表现层重分组（golden 嵌套形态）。 */
    void WalkFileChildren(const AstNode& fileNode);
    /** @brief IsExpr/AsExpr 共用：表达式 ＋ 目标类型两面都出。 */
    void TraverseTypeTestChildren(const AstNode& node, VisitResult& res);
    /** @brief LetPatternDestructor / ForInExpr 共用：不出模式子级。 */
    void TraversePatternExprChildren(const AstNode& node, VisitResult& res);
    /** @brief VarDecl / FuncParam 共用：类型标注排在子级之后。 */
    void TraverseTypeAnnotationLast(const AstNode& node, VisitResult& res);
    /** @brief 出导入行（可改名/改定位）：`词 + 名 + 留白 + // line:col`。 */
    void EmitImportRow(const AstNode& node, const Str& nameOverride, const Str& locOverride = "");
    /** @brief 无体函数（接口方法）的合成行：`Block // -`。 */
    void EmitEmptyBodyBlock();

private:
    /** @brief 注册钩子：通用默认（RegDefault*）＋ 按种类 Visit（薄适配转发具名 Visit*）。 */
    void RegisterHandlers();

    /** @brief 顶层判定：根或根的直接子级 ⇒ 留白 5。 */
    bool AtTop(const AstNode& node) const
    {
        return node.astKind == AstKind::FILE || parentIsFile_;
    }

private:
    std::fstream ofs;        /**< 输出文件流 */
    Printer prt;             /**< 打印器实例 */
    ConstAstVisitor visitor; /**< 抽象语法树遍历器 */

    bool inEnumBody_ = false;     /**< 是否处于枚举体直接子级（ctor 语境） */
    bool inNestedImport_ = false; /**< 是否处于 ImportSpec 子级（嵌套导入条目） */
    bool parentIsFile_ = false;   /**< 当前节点之父是否为 File（顶层留白判定，仅一层） */
};
