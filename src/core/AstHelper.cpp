/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "core/AstHelper.h"
#include "core/pass/Pass.h"
#include "utils/Logger.h"
#include "wrapper/AstNodeHelper.h"

AstHelper::AstHelper(Options&& options)
    : options(std::move(options)), cjfeHelper(this->options.args, Options::env), passManager(MakePassConfig())
{
    // 注册 stage 回调函数
    RegisterStages();
}

void AstHelper::Run()
{
    DisplayOptions();
    if (!DoParse()) {
        LOGD("DoParse failed.");
        return;
    }
    DumpAst();
    if (!DoAnalysis()) {
        LOGD("DoAnalysis failed.");
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
    if (options.astOutPath.has_value()) {
        p.PVals("astOutPath: ", options.astOutPath.value()).PNL();
    }
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
    LOGD(oss.str());
#endif
}

/**
 * @brief 执行解析阶段 复用前端的编译器调用，得到AST
 * @return 解析阶段执行成功返回true，否则返回false
 */
bool AstHelper::DoParse()
{
    LOGD();
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
 * @brief 执行打印AST阶段 (文件粒度)
 */
void AstHelper::DumpAst()
{
    if (!options.astOutPath.has_value()) {
        return;
    }
    if (pkgs.empty()) {
        return;
    }
    auto& outPath = options.astOutPath.value();
    for (auto& file : pkgs[0]->files) {
        AstNodeHelper::DumpAst(*file, outPath + "/" + file->fileName + ".ast");
    }
}

/**
 * @brief 执行分析阶段 复用前端的编译器调用，得到AST
 * @return 分析阶段执行成功返回true，否则返回false
 */
bool AstHelper::DoAnalysis()
{
    LOGD("add passes: ", options.passes.size());

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
    config->FocusModifierAttrs({"public", "protected", "internal", "private", "static"}, {"func", "var"});
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
        LOGD();
        return cjfeHelper.Parse();
    });
    stageMap.emplace(SourceStage::DESUGARED_PARSE, [this]() {
        LOGD();
        return cjfeHelper.DesugaredParse();
    });
    stageMap.emplace(SourceStage::IMPORT, [this]() {
        LOGD();
        return cjfeHelper.ImportPackage();
    });
    stageMap.emplace(SourceStage::SEMA, [this]() {
        LOGD();
        if (options.enableMacro) {
            cjfeHelper.MacroExpand();
        }
        return cjfeHelper.Sema();
    });
    stageMap.emplace(SourceStage::DESUGARED_SEMA, [this]() {
        LOGD();
        return cjfeHelper.DesugaredSema();
    });
}
