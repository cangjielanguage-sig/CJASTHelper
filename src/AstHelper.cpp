/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "AstHelper.h"
#include "pass/AllPasses.h"
#include "utils/Logger.h"

AstHelper::AstHelper(const Options& options) : options(options)
{
    ci.frontendOptions.ReadPathsFromEnvironmentVars(options.env);
    ci.ParseArgs(options.args);
    mci = std::make_unique<CompilerInstance>(ci, diag);
    // 注册 stage 回调函数
    RegisterStages();
    // 注册可用的 pass
    RegisterPasses();
}

std::string AstHelper::GetOutputDir() const
{
    return ci.globalOptions.outputDir.value_or(".");
}

void AstHelper::Run()
{
    DisplayOptions();
    if (!DoParse()) {
        Logger::Get().Error("AstHelper::Run", "DoParse failed.");
        return;
    }
    if (!DoAnalysis()) {
        Logger::Get().Error("AstHelper::Run", "DoAnalysis failed.");
        return;
    }
}

void AstHelper::DisplayOptions()
{
#ifdef NDEBUG
#else
    std::ostringstream oss;
    Printer p(oss, 4);
    p << "Options: {";
    p.PNL().Indent();
    p.PVals("enableDesugar: ", options.enableDesugar).PNL();
    p.PVec<std::string>(
         options.filterDecls, [](const std::string& decl) { return decl; }, ", ", "filterDecls: {", "}", true)
        .PNL();
    p.PVec<std::string>(
         options.ignoreDecls, [](const std::string& anno) { return anno; }, ", ", "ignoreDecls: {", "}", true)
        .PNL();
    p.PVec<std::string>(
         options.ignoreAnnotations, [](const std::string& anno) { return anno; }, ", ", "ignoreAnnotations: {", "}",
         true)
        .PNL();
    p.PVec<std::string>(
         options.importedPkgs, [](const std::string& pkg) { return pkg; }, ", ", "importedPkgs: {", "}", true)
        .PNL();
    p.PVec<std::string>(
         options.passes, [](const std::string& pass) { return pass; }, ", ", "passes: {", "}")
        .PNL();
    p.PVec<std::string>(
         options.args, [](const std::string& arg) { return arg; }, ", ", "args: {", "}")
        .PNL();
    p.Unindent();
    p << "}\n";
    Logger::Get().Debug("AstHelper::DisplayOptions", oss.str());
#endif
}

/**
 * @brief 执行解析阶段 复用前端的编译器调用，得到AST
 * @return 解析阶段执行成功返回true，否则返回false
 */
bool AstHelper::DoParse()
{
    Logger::Get().Debug("AstHelper::DoParse");
    // --dump-source 按照 stage 决策执行前端哪些pipeline
    for (int i = 0; i <= static_cast<int>(options.stage); i++) {
        if (i == static_cast<int>(SourceStage::DESUGARED_PARSE) && options.stage > SourceStage::DESUGARED_PARSE) {
            // Skip desugared parse stage when stage including sema.
            continue;
        }
        if (!stageMap.at(static_cast<SourceStage>(i))()) {
            return false;
        }
    }
    if (options.stage == SourceStage::IMPORT) {
        for (auto pkg : mci->GetPackages()) {
            if (options.importedPkgs.count(pkg->fullPackageName)) {
                pkgs.push_back(pkg);
            }
        }
    } else {
        pkgs = mci->GetSourcePackages();
    }
    return true;
}

/**
 * @brief 执行分析阶段 复用前端的编译器调用，得到AST
 * @return 分析阶段执行成功返回true，否则返回false
 */
bool AstHelper::DoAnalysis()
{
    Logger::Get().Debug("AstHelper::DoAnalysis", "add passes: ", options.passes.size());

    for (auto& pkg : pkgs) {
        passManager.Run(*pkg, options.passes);
    }
    return true;
}

/**
 * @brief 解析命令行参数, 拆分当前工具参数和前端工具透传参数
 */
void AstHelper::ParseArgs(const std::vector<std::string>& args)
{
    std::vector<std::string> ciArgs;

    ci.ParseArgs(ciArgs);
}

// 私有函数实现
namespace {
/**
 *  @brief 根据用户输入的选项更新 Builder 配置
 */
inline PassConfig GetPassConfig(const Options& options)
{
    PassConfig config;
    // --dump-desugar=true or false (默认不开启解糖: 尽可能恢复用户源码)
    if (options.enableDesugar) {
        config.EnableDesugar();
    }
    if (options.stage >= SourceStage::IMPORT) {
        config.EnableSema();
    }
    config.Focus(options.filterDecls);
    config.FocusAnnotationAttrs({"C"});
    config.FocusModifierAttrs({"public", "protected", "internal", "private"}, {"func", "var"});
    config.IgnoreDecls(options.ignoreDecls);
    config.IgnoreAnnotations(options.ignoreAnnotations);
    return config;
}
} // namespace
/**
 * 注册所有分析pass
 */
void AstHelper::RegisterPasses()
{
    PassConfig config = GetPassConfig(this->options);
    passManager.RegisterPass("replace-desugar", std::make_unique<ReplaceDesugarPass>(config));

    passManager.RegisterPass("check-desugar", std::make_unique<CheckDesugarPass>(config));

    config.Output(GetOutputDir());
    passManager.RegisterPass("to-source", std::make_unique<ToSourcePass>(config));
}

/**
 * 注册stage回调
 */
void AstHelper::RegisterStage(SourceStage stage, StageFunc fn)
{
    stageMap.emplace(stage, fn);
}

/**
 * 注册所有stage回调
 */
void AstHelper::RegisterStages()
{
    RegisterStage(SourceStage::DEFAULT, [this]() {
        Logger::Get().Debug("Default Stage", "input files: ", ci.globalOptions.srcFiles.size());
        Logger::Get().Debug("Parse Stage", "file paths: ", mci->srcFilePaths.size());
        Logger::Get().Debug("Default Stage", "Output: ", GetOutputDir());
        return true;
    });
    RegisterStage(SourceStage::PARSE, [this]() {
        Logger::Get().Debug("Parse Stage");
        return mci->PerformParse();
    });
    RegisterStage(SourceStage::DESUGARED_PARSE, [this]() {
        Logger::Get().Debug("DesugaredParse Stage");
        for (auto& pkg : mci->GetSourcePackages()) {
            PerformDesugarBeforeTypeCheck(*pkg);
        }
        return true;
    });
    RegisterStage(SourceStage::IMPORT, [this]() {
        Logger::Get().Debug("LoadImports Stage");
        return mci->PerformImportPackage();
    });
    RegisterStage(SourceStage::SEMA, [this]() {
        Logger::Get().Debug("Sema Stage");
        return mci->PerformSema();
    });
    RegisterStage(SourceStage::DESUGARED_SEMA, [this]() {
        Logger::Get().Debug("DesugaredSema stage");
        return mci->PerformDesugarAfterSema();
    });
}
