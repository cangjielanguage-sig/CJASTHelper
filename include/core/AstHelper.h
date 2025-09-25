/**
 * @file
 *
 * This file declares the AstHelper.
 */
#pragma once

#include "core/ArgHelper.h"
#include "core/pass/Pass.h"
#include "utils/Printer.h"
#include "wrapper/CangjieFrontendHelper.h"
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
    AstHelper(Options&& options);

    /**
     * @brief 默认析构函数
     */
    ~AstHelper() = default;

    /**
     * @brief 根据当前配置执行相应的阶段
     */
    void Run();

    /**
     * @brief 打印当前配置
     */
    void DisplayOptions();

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
    std::unique_ptr<PassConfig> MakePassConfig();

    /**
     * 注册stage回调
     */
    void RegisterStages();

private:
    Options options;                                                 /**< 用户选项 */
    CangjieFrontendHelper cjfeHelper;                                /**< Cangjie 前端辅助类 */
    PassManager passManager;                                         /**< 分析 pass 管理器 */
    std::unordered_map<SourceStage, std::function<bool()>> stageMap; /**< 注册的 stage 回调函数 */
    std::vector<Ptr<Package>> pkgs;                                  /**< 分析结果包列表 */
};
