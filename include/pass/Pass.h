/**
 * @file
 *
 * This file declares the Basic Pass.
 */

#pragma once

#include "wrapper/WrapperAst.h"

/**
 * @class PassConfig
 * @brief 配置 `Pass` 的参数。
 *
 * TODO: 拆分 PassConfig
 */
struct PassConfig {
    PassConfig();

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

    /**
     * @brief 设置输出文件路径。
     * @param out 输出文件路径。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    PassConfig& Output(const std::string& out);
    /**
     * @brief 设置输出文件后缀。
     * @param suffix 输出文件后缀名。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    PassConfig& Suffix(const std::string& suffix);
    /**
     * @brief 设置输出缩进大小。
     * @param indent 缩进大小。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    PassConfig& Indent(int indent);
    /**
     * @brief 启用解糖功能。
     */
    PassConfig& EnableDesugar();
    /**
     * @brief 启用语义分析功能。
     */
    PassConfig& EnableSema();
    /**
     * @brief 设置关注的顶层声明类型。
     * @param kinds 关注的声明类型名称列表。
     */
    PassConfig& Focus(const std::unordered_set<std::string>& kinds);
    /**
     * @brief 设置关注的注解属性。
     * @param attrs 关注的注解属性名称列表。
     */
    PassConfig& FocusAnnotationAttrs(const std::vector<std::string>& attrs);
    /**
     * @brief 设置关注的修饰符属性。
     * @param attrs 关注的修饰符属性名称列表。
     * @param kinds 关注的修饰符属性所在的声明类型名称列表（白名单）。
     */
    PassConfig& FocusModifierAttrs(const std::vector<std::string>& attrs, const std::vector<std::string>& kinds);
    /**
     * @brief 设置忽略的顶层声明。
     * @param decls 忽略的声明标识符列表。
     */
    PassConfig& IgnoreDecls(const std::unordered_set<std::string>& decls);
    /**
     * @brief 设置忽略的注解。
     * @param annos 忽略的注解名称列表。
     */
    PassConfig& IgnoreAnnotations(const std::unordered_set<std::string>& annos);

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
    std::unordered_set<std::string> focusAnnotationAttrs; /**< 关注的注解对应的属性列表 */
    // 可配置属性: Attribute::PUBLIC, ...
    std::unordered_set<std::string> focusModifierAttrs; /**< 关注的修饰符对应的属性列表 */
    std::unordered_set<AstKind> focusModifierWhiteList; /**< 关注的语义后修饰符的节点白名单 */
    std::unordered_set<std::string> ignoreAnnotations;  /**< 忽略的注解对应的属性列表 */
    std::unordered_set<std::string> ignoreDecls;        /**< 忽略的顶层声明列表 */
};

class Pass {
public:
    Pass(const PassConfig& config) : config(config)
    {
    }

    virtual ~Pass() = default;
    virtual void Run(AstNode& node) = 0;

protected:
    PassConfig config;
};

class PassManager {
public:
    PassManager(std::unique_ptr<PassConfig> config) : config(std::move(config))
    {
    }

    /**
     * 执行 passes 中的所有 pass
     *
     * @param node 待处理的 AST 树
     * @param passes 待执行的 passes
     */
    void Run(AstNode& node, const std::vector<std::string>& passes);

    using PassBuilder = std::function<std::unique_ptr<Pass>(const PassConfig&)>;
    /**
     * 注册 pass builder
     *
     * @param name pass 名称
     * @param builder pass builder
     */
    static void RegPassBuilder(const std::string& name, const PassBuilder& builder);

    /**
     * Register a pass builder
     */
    class Register {
    public:
        Register(const std::string& name, const PassBuilder& builder)
        {
            PassManager::RegPassBuilder(name, builder);
        }
    };

private:
    Pass* TryGetPass(const std::string& name);

private:
    std::unique_ptr<PassConfig> config;
    std::unordered_map<std::string, std::unique_ptr<Pass>> passMap; /**< 注册的分析pass: name -> Pass */

    static inline std::unordered_map<std::string, PassBuilder> passBuilderMap;
};

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define REG_PASS(name, builder)                                                                                        \
    [[maybe_unused]] inline static PassManager::Register CONCAT(_reg_, __LINE__)(name, builder)
