#include "AstHelper.h"
#include <iostream>

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
