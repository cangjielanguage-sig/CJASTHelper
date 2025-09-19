/**
 * @file
 *
 * This file declares the Basic Pass.
 */

#pragma once

#include "wrapper/AstNodeHelper.h"

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
