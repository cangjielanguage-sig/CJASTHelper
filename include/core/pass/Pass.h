/**
 * @file
 *
 * This file declares the Basic Pass.
 */

#pragma once

#include "utils/Logger.h"
#include "utils/types/TypeAlias.h"
#include "wrapper/TypeAlias.h"

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
 * @class PassConfig
 * @brief 配置 `Pass` 的参数。
 */
class PassConfig {
public:
    PassConfig() = default;
    virtual ~PassConfig() = default;

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
     * @brief 启用解糖功能。
     */
    PassConfig& EnableDesugar();
    /**
     * @brief 启用语义分析功能。
     */
    PassConfig& EnableSema();

protected:
    Flag flags; /**< 功能开关标志（解糖、语义等） */
};

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
    ToSourcePassConfig& Output(ConStr& out);
    /**
     * @brief 设置输出文件后缀。
     * @param suffix 输出文件后缀名。
     * @return 返回当前构建器实例的引用，支持链式调用。
     */
    ToSourcePassConfig& Suffix(ConStr& suffix);

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
    ToSourcePassConfig& FocusAnnotationAttrs(ConStrVec& attrs);
    /**
     * @brief 设置忽略的顶层声明。
     * @param decls 忽略的声明标识符列表。
     */
    ToSourcePassConfig& IgnoreDecls(ConStrSet& decls);
    /**
     * @brief 设置忽略的注解。
     * @param annos 忽略的注解名称列表。
     */
    ToSourcePassConfig& IgnoreAnnotations(ConStrSet& annos);
    /**
     * @brief 设置关注的顶层声明类型。
     * @param kinds 关注的声明类型名称列表。
     */
    ToSourcePassConfig& Focus(ConStrSet& kinds);
    /**
     * @brief 设置关注的修饰符属性。
     * @param attrs 关注的修饰符属性名称列表。
     * @param kinds 关注的修饰符属性所在的声明类型名称列表（白名单）。
     */
    ToSourcePassConfig& FocusModifierAttrs(ConStrVec& attrs, ConStrVec& kinds);

    int indent; /**< 输出缩进大小 */
    Str out;    /**< 输出文件路径 */
    Str suffix; /**< 输出文件后缀 */
    // 可配置属性: Attribute::C, Attribute::INTRINSIC, ...
    StrSet focusAnnotationAttrs; /**< 关注的注解对应的属性列表 */
    // 可配置属性: Attribute::PUBLIC, ...
    StrSet focusModifierAttrs;                    /**< 关注的修饰符对应的属性列表 */
    UnorderedSet<AstKind> focusModifierWhiteList; /**< 关注的语义后修饰符的节点白名单 */
    StrSet ignoreAnnotations;                     /**< 忽略的注解对应的属性列表 */
    StrSet ignoreDecls;                           /**< 忽略的顶层声明列表 */
    UnorderedSet<AstKind> focusDecls;             /**< 关注的顶层声明类型 */
};

/**
 * @brief 抽象的Pass类，所有Pass的基类。
 */
class Pass {
public:
    Pass(const PassConfig& config) : config(config)
    {
    }

    virtual ~Pass() = default;
    virtual void Run(AstNode& node) = 0;

protected:
    const PassConfig& config;
};

using PassBuilder = Function<UniquePtr<Pass>(const PassConfig&)>;

struct PassInfo {
    Str group;      /**< group 名称 */
    StrVec names;   /**< pass names 名称 */
    Str lib;        /**< lib 名称 */
    Str desc;       /**< pass 组描述 */
    Str version;    /**< pass 版本 */
    StrVec depends; /**< 依赖的 pass 组 */
};

class PassManager {
public:
    PassManager(UniquePtr<PassConfig> config) : config(std::move(config))
    {
    }

    /**
     * 初始化 PassManager
     *
     * @param path pass配置文件路径
     */
    static void Init(ConStr& path);

    /**
     * 执行 passes 中的所有 pass
     *
     * @param node 待处理的 AST 树
     * @param passes 待执行的 passes
     */
    void Run(AstNode& node, ConStrVec& passes);
    /**
     * 注册 pass builder
     *
     * @param name pass 名称
     * @param builder pass builder
     */
    static void RegPassBuilder(ConStr& name, const PassBuilder& builder);

    /**
     * Register a pass builder
     */
    class Register {
    public:
        Register(ConStr& name, const PassBuilder& builder)
        {
            PassManager::RegPassBuilder(name, builder);
        }
    };

private:
    bool LoadPass(ConStr& lib);

private:
    Pass* TryGetPass(ConStr& name);

private:
    UniquePtr<PassConfig> config;                 /**< 配置信息 */
    StrMap<UniquePtr<Pass>> passMap;              /**< 注册的分析 pass: name -> Pass */
    static inline StrMap<PassBuilder> builderMap; /**< pass name -> Builder */
    static inline StrMap<Str> groupMap;           /**< pass name -> group name */
    static inline StrMap<PassInfo> passInfoMap;   /**< 注册的pass信息: group name -> PassInfo  */
};

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define REG_PASS(name, builder)                                                                                        \
    [[maybe_unused]] inline static PassManager::Register CONCAT(_reg_, __LINE__)(name, builder)
