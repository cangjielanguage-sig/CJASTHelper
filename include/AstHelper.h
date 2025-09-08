/**
 * @file
 *
 * This file declares the AstHelper.
 */
#pragma once

#include "pass/Pass.h"
#include "utils/ArgHelper.h"
#include "utils/Printer.h"
#include "wrapper/WrapperCangjieFrontend.h"
#include <memory>

/**
 * 封装了对Cangjie前端工具的操作
 */
class AstHelper {
public:
    /**
     * @brief 构造AstHelper实例
     * @param args 命令行参数的向量
     * @param env 环境变量的映射
     */
    AstHelper(const Options& options);

    /**
     * @brief 默认析构函数
     */
    ~AstHelper() = default;

    /**
     * @brief 获取输出目录
     * @return 表示输出目录的字符串
     */
    std::string GetOutputDir() const;

    /**
     * @brief 根据当前配置执行相应的阶段
     */
    void Run();

protected:
    /**
     * @brief 执行解析阶段 复用前端的编译器调用，得到AST
     * @return 解析阶段执行成功返回true，否则返回false
     */
    bool DoParse();
    /**
     * @brief 执行分析阶段 调用注册的分析pass
     * @return 分析阶段执行成功返回true，否则返回false
     */
    bool DoAnalysis();

private:
    /**
     * @brief 解析提供的命令行参数
     * @param args 命令行参数的向量
     */
    void ParseArgs(const std::vector<std::string>& args);

    /**
     * 注册分析pass
     */
    void RegisterPasses();

    using StageFunc = std::function<bool()>;
    /**
     * 注册stage回调
     */
    void RegisterStage(SourceStage stage, StageFunc fn);
    void RegisterStages();

private:
    DiagnosticEngine diag;                 /**< 诊断引擎实例 */
    CompilerInvocation ci;                 /**< 编译器调用实例 */
    std::unique_ptr<CompilerInstance> mci; /**< 编译器实例的智能指针 */

    Options options;                /**< 用户选项 */
    std::vector<Ptr<Package>> pkgs; /**< 分析结果包列表 */

    PassManager passManager;
    std::unordered_map<SourceStage, std::function<bool()>> stageMap; /**< 注册的 stage 回调函数 */
};
