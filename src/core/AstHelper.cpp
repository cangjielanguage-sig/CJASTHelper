/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include <fstream>

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

bool AstHelper::Run()
{
    DisplayOptions();
    if (!DoParse()) {
        LOGD("DoParse failed.");
        return false;
    }
    if (options.checkSyntax) {
        // 语法检查模式: 检查完语法后提前结束, 不做 ast 输出/分析/转换
        bool succeed = cjfeHelper.GetErrorCount() == 0;
        LOGD("check-syntax mode: skip ast dump, analysis and conversion.");
        return succeed;
    }
    DumpAst();
    if (!DoAnalysis()) {
        LOGD("DoAnalysis failed.");
        return false;
    }
    return true;
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
    if (options.stage >= SourceStage::SEMA) {
        config->EnableSema();
    }
    config->Focus(options.filterDecls);
    config->FocusAnnotationAttrs({"C"});
    config->FocusModifierAttrs({"public", "protected", "internal", "private", "static", "override"}, {"func", "var"});
    config->IgnoreDecls(options.ignoreDecls);
    config->IgnoreAnnotations(options.ignoreAnnotations);
    // --print-ast 指定了目录 ⇒ 打印 pass 写该目录；否则沿用工具输出目录
    config->Output(options.astTreeOutPath.has_value() ? options.astTreeOutPath.value() : cjfeHelper.GetOutDir());
    // 注：同包多文件的输出顺序由各 pass 自行决定（ast-tree-print 按文件名升序），
    //     不再从命令行提取"源文件序"下发。
    return config;
}

/**
 * 注册所有stage回调
 */
void AstHelper::RegisterStages()
{
    stageMap.emplace(SourceStage::PARSE, [this]() {
        LOGD();
        // ★★ `parse` 阶段 = **纯语法解析**，**不走条件编译** ✗（用户口径 ✓）
        //   为何：`ConditionCompile()` 会**消费语法注解** ✗ —— 实测 `@When[env != "ohos"]` 的
        //   `Annotation`（`ParseAnnotations.cpp:69` 建于解析期 ✓ 带 `condExpr` ✓）在此步之后
        //   **从 `decl.annotations` 消失** ✗（探针实测：decl=f annos=0 ／ 同文件 decl=g annos=1 ✓）
        //   ⇒ 打印阶段已无从取得 ✗ ⇒ 任意阶段切换（parse/desugared-parse/macro 三阶段 `When` 皆 0 ✗）都救不回 ✓
        //   影响面：仅 stage == PARSE ⇒ 更后阶段（macro/sema ✓）**仍执行**条件编译 ✓（它们依赖其结果 ✓）
        //   收益：parse 阶段的树变为**与目标平台无关**的纯语法树 ✓（与 LLT 侧读 SDK `std.ast` 同性质 ✓）
        if (options.stage == SourceStage::PARSE) {
            return cjfeHelper.Parse();
        }
        return cjfeHelper.Parse() && cjfeHelper.ConditionCompile();
    });
    stageMap.emplace(SourceStage::DESUGARED_PARSE, [this]() {
        LOGD();
        return cjfeHelper.DesugaredParse();
    });
    stageMap.emplace(SourceStage::IMPORT, [this]() {
        LOGD();
        return cjfeHelper.ImportPackage();
    });
    stageMap.emplace(SourceStage::MACRO_EXPAND, [this]() {
        if (options.enableMacro) {
            LOGD();
            return cjfeHelper.MacroExpand();
        }
        return true;
    });
    stageMap.emplace(SourceStage::SEMA, [this]() {
        LOGD();
        return cjfeHelper.Sema();
    });
    stageMap.emplace(SourceStage::DESUGARED_SEMA, [this]() {
        LOGD();
        return cjfeHelper.DesugaredSema();
    });
}
