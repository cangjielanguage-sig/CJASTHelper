#include "AstHelper.h"
#include "Printer.h"
#include <iostream>
#include <vector>

namespace {
void ShowHelperInfo()
{
    std::cout << "Welcome Using Cangjie AST Helper!" << std::endl;
}

void PrintArgs(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env)
{
    Printer p(std::cout, 4);

    p.printc<std::string>(args, [](const std::string& v) { return "\"" + v + "\""; }, ", ", "[", "]", true).pnl();

    p.printc<std::pair<const std::string, std::string>>(
         env,
         [&p](const std::pair<const std::string, std::string>& kv) {
             p.indent();
             p.pval(kv.first).pval(": ").pval(kv.second).pnl();
             p.unindent();
         },
         "", "{\n", "}", true)
        .pnl();
}
} // namespace

int main(int argc, const char* const* argv, const char* const* envp)
{
    try {
        ShowHelperInfo();
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
