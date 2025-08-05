/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "AstHelper.h"
#include "Ast2SourceVisitor.h"
#include "Logger.h"
#include "cangjie/Sema/Desugar.h"

using namespace Cangjie;

AstHelper::AstHelper(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env)
{
    ci.frontendOptions.ReadPathsFromEnvironmentVars(env);
    ParseArgs(args);
    mci = std::make_unique<CompilerInstance>(ci, diag);
}

std::string AstHelper::GetOutputDir() const
{
    return ci.globalOptions.outputDir.value_or(".");
}

void AstHelper::Run()
{
    for (int i = 0; i <= static_cast<int>(stage); i++) {
        if (i == static_cast<int>(SourceStage::DESUGARED_PARSE) && stage > SourceStage::DESUGARED_PARSE) {
            // Skip desugared parse stage when stage including sema.
            continue;
        }
        if (!stageMap.at(static_cast<SourceStage>(i))(this)) {
            return;
        }
    }
    auto pkgs = mci->GetSourcePackages();
    Logger::Get().Debug("AstHelper::Run", "Get pkgs: ", pkgs.size());
    Ast2SourceVisitor ast2SourceVisitor(GetOutputDir());
    for (auto pkg : pkgs) {
        Logger::Get().Debug("AstHelper::Run", "Traverse ", pkg->fullPackageName, " by Ast2SourceVisitor");
        Traverse(*pkg, ast2SourceVisitor);
    }
}

void AstHelper::ParseArgs(const std::vector<std::string>& args)
{
    const std::string& DS_KEY = "--dump-source=";
    std::vector<std::string> ciArgs;
    for (auto arg : args) {
        if (arg.find(DS_KEY) != std::string::npos) {
            // parse the value
            auto val = arg.substr(DS_KEY.size());
            Logger::Get().Debug("AstHelper::ParseArgs", "Get Stage: ", val);
            if (auto it = key2Stage.find(val); it != key2Stage.end()) {
                stage = it->second;
            } else {
                Logger::Get().Warn("AstHelper::ParseArgs", "not supported stage: ", val);
            }
        } else {
            ciArgs.push_back(arg);
        }
    }
    Logger::Get().Debug("AstHelper::ParseArgs", "args: ", ciArgs.size());
    ci.ParseArgs(ciArgs);
}

bool AstHelper::Default()
{
    Logger::Get().Debug("AstHelper::Default", "input files: ", ci.globalOptions.srcFiles.size());
    Logger::Get().Debug("AstHelper::Default", "Output", GetOutputDir());
    return true;
}

bool AstHelper::Parse()
{
    Logger::Get().Debug("AstHelper::Parse");
    Logger::Get().Debug("AstHelper::Parse", "file paths: ", mci->srcFilePaths.size());

    return mci->PerformParse();
}

bool AstHelper::DesugaredParse()
{
    Logger::Get().Debug("AstHelper::DesugaredParse");
    for (auto& pkg : mci->GetPackages()) {
        PerformDesugarBeforeTypeCheck(*pkg);
    }
    return true;
}

bool AstHelper::Sema()
{
    Logger::Get().Debug("AstHelper::Sema");
    return mci->PerformSema();
}

bool AstHelper::DesugaredSema()
{
    Logger::Get().Debug("AstHelper::DesugaredSema");
    return mci->PerformDesugarAfterSema();
}

std::vector<std::string> ParseArgs(int argc, const char* const* argv)
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
std::unordered_map<std::string, std::string> ParseEnv(
    const char* const* envp, const std::unordered_set<std::string>& focus)
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