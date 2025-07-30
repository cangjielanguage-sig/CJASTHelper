/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "AstHelper.h"
#include "cangjie/Sema/Desugar.h"

using namespace Cangjie;

AstHelper::AstHelper(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env)
    : sm(std::make_unique<SourceManager>()),
      diag(std::make_unique<DiagnosticEngine>()),
      ci(std::make_unique<CompilerInvocation>()),
      dci(std::make_unique<DefaultCompilerInstance>(*ci, *diag)),
      logger(std::cout)
{
    AH_CHECK_NULL(sm);
    AH_CHECK_NULL(diag);
    AH_CHECK_NULL(ci);
    AH_CHECK_NULL(dci);
    diag->SetSourceManager(sm.get());
    ci->frontendOptions.ReadPathsFromEnvironmentVars(env);
    ParseArgs(args);
}

std::string AstHelper::getOutputDir() const
{
    return ci->globalOptions.outputDir.value_or("");
}

void AstHelper::run()
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
    auto pkgs = dci->GetSourcePackages();
    log("Get pkgs", pkgs.size());
}

void AstHelper::ParseArgs(const std::vector<std::string>& args)
{
    const std::string& DS_KEY = "--dump-source=";
    std::vector<std::string> ciArgs;
    for (auto arg : args) {
        if (arg.find(DS_KEY) != std::string::npos) {
            // parse the value
            auto val = arg.substr(DS_KEY.size());
            log("Get Stage", val);
            if (auto it = key2Stage.find(val); it != key2Stage.end()) {
                stage = it->second;
            } else {
                log("Warning: not supported stage", val);
            }
        } else {
            ciArgs.push_back(arg);
        }
    }
    ci->ParseArgs(ciArgs);
}

bool AstHelper::Default()
{
    log("inputs", ci->globalOptions.srcFiles.size());
    log("Output", getOutputDir());
    return true;
}

bool AstHelper::Parse()
{
    log("Parse", "");
    return dci->PerformParse();
}

bool AstHelper::DesugaredParse()
{
    for (auto& pkg : dci->GetPackages()) {
        PerformDesugarBeforeTypeCheck(*pkg);
    }
    return true;
}

bool AstHelper::Sema()
{
    return dci->PerformSema();
}

bool AstHelper::DesugaredSema()
{
    return dci->PerformDesugarAfterSema();
}
