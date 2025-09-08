/**
 * @file
 *
 * This file implements the main entry of the AstHelper.
 */

#include "AstHelper.h"
#include <iostream>

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
        AstHelper ah(options);
        if (options.stage == SourceStage::DEFAULT) {
            argHelper.ShowHelperInfo();
            return 0;
        }
        ah.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
