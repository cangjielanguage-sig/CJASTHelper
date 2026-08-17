/**
 * @file
 *
 * This file implements the main entry of the AstHelper.
 */

#include "core/AstHelper.h"
#include "utils/TaskExecutor.h"
#include <iostream>

void RunParallel(Vec<Options>& options)
{
    TaskExecutor exector(Options::parallels);
    for (auto& option : options) {
        exector.Post([&option]() {
            AstHelper ah(std::move(option));
            ah.Run();
        });
    }
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
        if (Options::parallels > 1) {
            RunParallel(options);
        } else {
            return RunSerial(options) ? 0 : 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
