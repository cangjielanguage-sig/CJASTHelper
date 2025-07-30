#include "AstHelper.h"
#include "Printer.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
void ShowHelperInfo()
{
    std::cout << "Welcome Using Cangjie AST Helper!" << std::endl;
}

std::vector<std::string> ParseArgs(int argc, const char** argv)
{
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }
        args.emplace_back(argv[i]);
    }
    return args;
}

// Filter some key vars
std::unordered_map<std::string, std::string> ParseEnv(const char** envp, const std::unordered_set<std::string>& focus)
{
    std::unordered_map<std::string, std::string> env;
    if (!envp) {
        return env;
    }
    int i = 0;
    constexpr char ASSIGN = '=';
    while (true) {
        if (!envp[i]) {
            break;
        }
        std::string kv(envp[i]);
        if (auto pos = kv.find(ASSIGN); pos != std::string::npos) {
            std::string key = kv.substr(0, pos);
            if (focus.find(key) != focus.end()) {
                env.emplace(key, kv.substr(pos + 1));
            }
        }
        i++;
    }
    return env;
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

int main(int argc, const char** argv, const char** envp)
{
    ShowHelperInfo();
    auto args = ParseArgs(argc, argv);
    auto env = ParseEnv(envp, {"CANGJIE_PATH", "CANGJIE_HOME", "LIBRARY_PATH", "LD_LIBRARY_PATH", "PATH", "SDKROOT"});

    PrintArgs(args, env);
    AstHelper ah(args, env);
    ah.run();
    return 0;
}
