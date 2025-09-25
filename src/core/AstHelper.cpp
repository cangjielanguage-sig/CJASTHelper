/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "core/AstHelper.h"
#include "core/pass/Pass.h"
#include "utils/Logger.h"

AstHelper::AstHelper(Options&& options)
    : options(std::move(options)),
      cjfeHelper(std::move(this->options.args), std::move(this->options.env)),
      passManager(MakePassConfig())
{
    // 注册 stage 回调函数
    RegisterStages();
    passManager.Init(this->options.passConfig);
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
    p.PVec<Str>(
         options.filterDecls, [](ConStr& decl) { return decl; }, ", ", "filterDecls: {", "}", true)
        .PNL();
    p.PVec<Str>(
         options.ignoreDecls, [](ConStr& anno) { return anno; }, ", ", "ignoreDecls: {", "}", true)
        .PNL();
    p.PVec<Str>(
         options.ignoreAnnotations, [](ConStr& anno) { return anno; }, ", ", "ignoreAnnotations: {", "}", true)
        .PNL();
    p.PVec<Str>(
         options.importedPkgs, [](ConStr& pkg) { return pkg; }, ", ", "importedPkgs: {", "}", true)
        .PNL();
    p.PVec<Str>(
         options.passes, [](ConStr& pass) { return pass; }, ", ", "passes: {", "}")
        .PNL();
    p.PVec<Str>(
         options.args, [](ConStr& arg) { return arg; }, ", ", "args: {", "}")
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
        for (auto pkg : cjfeHelper.GetImportedPackages()) {
            if (options.importedPkgs.count(pkg->fullPackageName)) {
                pkgs.push_back(pkg);
            }
        }
    } else {
        pkgs = cjfeHelper.GetSourcePackages();
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
 * @brief 创建PassConfig
 */
UniquePtr<PassConfig> AstHelper::MakePassConfig()
{
    auto config = UniquePtr<ToSourcePassConfig>(new ToSourcePassConfig());
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
    config->Output(cjfeHelper.GetOutDir());
    return config;
}

/**
 * 注册所有stage回调
 */
void AstHelper::RegisterStages()
{
    stageMap.emplace(SourceStage::PARSE, [this]() {
        DEBUG();
        return cjfeHelper.Parse();
    });
    stageMap.emplace(SourceStage::DESUGARED_PARSE, [this]() {
        DEBUG();
        return cjfeHelper.DesugaredParse();
    });
    stageMap.emplace(SourceStage::IMPORT, [this]() {
        DEBUG();
        return cjfeHelper.ImportPackage();
    });
    stageMap.emplace(SourceStage::SEMA, [this]() {
        DEBUG();
        if (options.enableMacro) {
            cjfeHelper.MacroExpand();
        }
        return cjfeHelper.Sema();
    });
    stageMap.emplace(SourceStage::DESUGARED_SEMA, [this]() {
        DEBUG();
        return cjfeHelper.DesugaredSema();
    });
}
