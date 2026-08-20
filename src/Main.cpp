/**
 * @file
 *
 * This file implements the main entry of the AstHelper.
 */

#include "core/AstHelper.h"
#include "utils/TaskExecutor.h"
#include "wrapper/JsonDiagCollector.h"
#include <atomic>
#include <iostream>

/**
 * @brief 并行执行所有包的检查
 * @return 所有包是否全部通过(任一包失败返回 false)
 */
bool RunParallel(Vec<Options>& options)
{
    std::atomic<bool> allOk{true};
    {
        // 内层作用域: TaskExecutor 析构(join 所有任务)必须先于读取 allOk,
        // 否则 return 取到的是任务尚未完成时的初值
        TaskExecutor exector(Options::Parallels());
        for (auto& option : options) {
            exector.Post([&option, &allOk]() {
                AstHelper ah(std::move(option));
                if (!ah.Run()) {
                    allOk.store(false);
                }
            });
        }
    }
    return allOk.load();
}

bool RunSerial(Vec<Options>& options)
{
    bool succeed = true;
    for (auto option : options) {
        AstHelper ah(std::move(option));
        succeed = ah.Run() && succeed;
    }
    return succeed;
}

/**
 * @brief 程序主入口函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @param envp 环境变量数组
 * @return 程序退出状态码
 */
int main(int argc, const char* const* argv, const char* const* envp)
{
    try {
        auto argHelper = ArgHelper();
        auto options = argHelper.ParseArgs(argc, argv, envp);
        if (options.empty() || options[0].stage == SourceStage::DEFAULT) {
            if (!options.empty() && !options[0].valid) {
                // 参数解析失败: 打印帮助信息后以错误码退出
                argHelper.ShowHelperInfo();
                return 1;
            }
            argHelper.ShowHelperInfo();
            return 0;
        }
        bool succeed = true;
        if (Options::Parallels() > 1) {
            succeed = RunParallel(options);
        } else {
            succeed = RunSerial(options);
        }
        // 所有包(串行或并行)检查结束后统一输出一份聚合的 JSON 诊断文档
        // (多包 check-syntax 模式; TaskExecutor 析构时已 join 所有任务, 此处聚合必然完整)
        JsonDiagCollector::FlushIfJsonMode();
        return succeed ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
