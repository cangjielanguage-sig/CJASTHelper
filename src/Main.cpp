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
        auto args = ParseArgs(argc, argv);
        auto env =
            ParseEnv(envp, {"CANGJIE_PATH", "CANGJIE_HOME", "LIBRARY_PATH", "LD_LIBRARY_PATH", "PATH", "SDKROOT"});
        AstHelper ah(args, env);
        ah.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
    }
    return 0;
}
