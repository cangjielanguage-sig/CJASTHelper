#pragma once

#include "wrapper/WrapperAst.h"

/**
 * @class PassConfig
 * @brief 配置 `ToSourcePass` 的参数。
 */
class PassConfig {
public:
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
    PassManager() = default;
    /**
     * 注册一个分析pass
     */
    void RegisterPass(std::string name, std::unique_ptr<Pass> pass);

    void Run(AstNode& node, const std::vector<std::string>& passes)
    {
        for (auto& pass : passes) {
            passMap[pass]->Run(node);
        }
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Pass>> passMap; /**< 注册的分析pass: name -> Pass */
};
