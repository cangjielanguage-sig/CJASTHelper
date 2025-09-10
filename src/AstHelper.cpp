/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "AstHelper.h"
#include "pass/AllPasses.h"
#include "utils/Logger.h"

AstHelper::AstHelper(const Options& options)
    : options(options), mci(std::make_unique<CompilerInstance>(ParseArgs(), diag)), passManager(MakePassConfig())
{
    // 注册 stage 回调函数
    RegisterStages();
}

void AstHelper::Run()
{
    DisplayOptions();
    if (!DoParse()) {
        DEBUG("DoParse failed.");
        return;
    }
    if (!DoAnalysis()) {
        DEBUG("DoAnalysis failed.");
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
    DEBUG(oss.str());
#endif
}

/**
 * @brief 执行解析阶段 复用前端的编译器调用，得到AST
 * @return 解析阶段执行成功返回true，否则返回false
 */
bool AstHelper::DoParse()
{
    DEBUG();
    // --dump-source 按照 stage 决策执行前端哪些pipeline
    for (int i = 1; i <= static_cast<int>(options.stage); i++) {
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
    DEBUG("add passes: ", options.passes.size());

    for (auto& pkg : pkgs) {
        passManager.Run(*pkg, options.passes);
    }
    return true;
}

// 私有函数实现
/**
 * @brief 解析命令行参数
 */
CompilerInvocation& AstHelper::ParseArgs()
{
    ci.frontendOptions.ReadPathsFromEnvironmentVars(options.env);
    ci.ParseArgs(options.args);
    return ci;
}

/**
 * @brief 创建PassConfig
 */
std::unique_ptr<PassConfig> AstHelper::MakePassConfig()
{
    auto config = std::unique_ptr<ToSourcePassConfig>(new ToSourcePassConfig());
    // --dump-desugar=true or false (默认不开启解糖: 尽可能恢复用户源码)
    if (options.enableDesugar) {
        config->EnableDesugar();
    }
    if (options.stage >= SourceStage::IMPORT) {
        config->EnableSema();
    }
    config->Focus(options.filterDecls);
    config->FocusAnnotationAttrs({"C"});
    config->FocusModifierAttrs({"public", "protected", "internal", "private"}, {"func", "var"});
    config->IgnoreDecls(options.ignoreDecls);
    config->IgnoreAnnotations(options.ignoreAnnotations);
    config->Output(ci.globalOptions.outputDir.value_or("."));
    return config;
}

/**
 * 注册所有stage回调
 */
void AstHelper::RegisterStages()
{
    stageMap.emplace(SourceStage::PARSE, [this]() {
        DEBUG();
        return mci->PerformParse();
    });
    stageMap.emplace(SourceStage::DESUGARED_PARSE, [this]() {
        DEBUG();
        for (auto& pkg : mci->GetSourcePackages()) {
            PerformDesugarBeforeTypeCheck(*pkg);
        }
        return true;
    });
    stageMap.emplace(SourceStage::IMPORT, [this]() {
        DEBUG();
        return mci->PerformImportPackage();
    });
    stageMap.emplace(SourceStage::SEMA, [this]() {
        DEBUG();
        return mci->PerformSema();
    });
    stageMap.emplace(SourceStage::DESUGARED_SEMA, [this]() {
        DEBUG();
        return mci->PerformDesugarAfterSema();
    });
}
