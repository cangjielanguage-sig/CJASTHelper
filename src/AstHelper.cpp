/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "AstHelper.h"
#include "utils/ArgumentParser.h"
#include "utils/Logger.h"
#include "visitor/Ast2SourceVisitor.h"
#include "visitor/MutAstVisitor.h"
#include "visitor/TestPass.h"

namespace {
/**
 * @brief 用于打印帮助信息的类
 */
class HelperInfoPrinter {
public:
    HelperInfoPrinter(int width = 32) : p(std::cout, 4), width(width)
    {
    }

    inline void PL(const std::string& line, int blanks = 1)
    {
        p << std::left << std::setfill(' ') << std::setw(width) << line;
        p.PNL(blanks);
    }

    inline void PL(const std::string& opt, const std::string& detail, int blanks = 1)
    {
        p << std::left << std::setfill(' ') << std::setw(width) << opt << detail;
        p.PNL(blanks);
    }

    inline void PWILines(const std::vector<std::string>& lines)
    {
        p.Indent();
        for (const auto& line : lines) {
            PL(line);
        }
        p.Unindent();
    }

    inline void PWILines(const std::vector<std::pair<std::string, std::string>>& lines)
    {
        p.Indent();
        for (const auto& pair : lines) {
            PL(pair.first, pair.second);
        }
        p.Unindent();
    }

    inline void Indent()
    {
        p.Indent();
    }

    inline void Unindent()
    {
        p.Unindent();
    }

private:
    Printer p;
    int width; // 对齐宽度
};

void ShowHelperInfo()
{
    HelperInfoPrinter hip;
    hip.PL("Welcome Using Cangjie AST Helper!", 2);
    hip.PL("Usage: cjah [options] [cjc-options]", 2);
    hip.PL("Options: ");
    hip.Indent();
    hip.PL("--dump-source=<stage>", "Dump source after <stage>. Supported stages:");
    hip.PWILines({"<stage>=parse", "<stage>=desugared-parse", "<stage>=sema", "<stage>=desugared-sema"});
    hip.PL("");
    hip.PL(
        "--dump-desugar=<value>", "Dump desugared code when <value> is true. Supported <value>: (default) true, false");
    hip.PL("");
    hip.PL("--filter-decls=<kinds>",
        "Filter top-level decls of <kinds>. Supported <kinds>: func, class, interface, struct, enum, var");
    std::vector<std::pair<std::string, std::string>> filterInfos{
        {"<kinds>=func", "Dump functions."}, {"<kinds>=func,class", "Dump functions and classes."}, {"...", ""}};
    hip.PWILines(filterInfos);
    hip.Unindent();
    hip.PL("");
    hip.PL("CJC-Options: please refer to `cjc -h`.");
}

void PrintArgs(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env)
{
    Printer p(std::cout, 4);
    p.PVec<std::string>(args, [](const std::string& v) { return "\"" + v + "\""; }, ", ", "[", "]", true).PNL();
    p.PVec<std::pair<const std::string, std::string>>(
         env,
         [&p](const std::pair<const std::string, std::string>& kv) {
             p.Indent();
             p.PVal(kv.first).PVal(": ").PVal(kv.second).PNL();
             p.Unindent();
         },
         "", "{\n", "}", true)
        .PNL();
}

/**
 * @brief 将字符串键映射到SourceStage值
 */
const std::unordered_map<std::string, AstHelper::SourceStage> key2Stage{{"parse", AstHelper::SourceStage::PARSE},
    {"desugared-parse", AstHelper::SourceStage::DESUGARED_PARSE}, {"sema", AstHelper::SourceStage::SEMA},
    {"desugared-sema", AstHelper::SourceStage::DESUGARED_SEMA}};

} // namespace

AstHelper::AstHelper(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env)
{
    ci.frontendOptions.ReadPathsFromEnvironmentVars(env);
    ParseArgs(args);
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
    if (ci.frontendOptions.showUsage) {
        ShowHelperInfo();
        return;
    }
    if (!DoParse()) {
        Logger::Get().Error("AstHelper::Run", "DoParse failed.");
        return;
    }
    if (!DoAnalysis()) {
        Logger::Get().Error("AstHelper::Run", "DoAnalysis failed.");
        return;
    }
    if (!DoTransform()) {
        Logger::Get().Error("AstHelper::Run", "DoTransform failed.");
        return;
    }
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
    Logger::Get().Debug("AstHelper::DoAnalysis");
    passes.push_back("test");
    for (auto& pass : passes) {
        if (auto visitor = passMap.find(pass); visitor != passMap.end()) {
            Logger::Get().Debug("AstHelper::DoAnalysis", "do pass: ", pass);
            for (auto& pkg : pkgs) {
                MutTraverse(*pkg, *visitor->second);
            }
        }
    }
    return true;
}

namespace {
/**
 *  @brief 根据用户输入的选项更新 Builder 配置
 */
inline void UpdateAst2SourceVisitorBuilder(Ast2SourceVisitorBuilder& builder, const AstHelper::Options& options)
{
    // --dump-desugar=true or false (默认不开启解糖: 尽可能恢复用户源码)
    if (options.enableDesugar) {
        builder.EnableDesugar();
    }
    if (options.stage >= AstHelper::SourceStage::IMPORT) {
        builder.EnableSema();
    }
    builder.Focus(options.filterDecls);
    // TODO: 默认白名单
    builder.FocusAnnotationAttrs({"C"});
    builder.FocusModifierAttrs({"public", "protected", "internal", "private"}, {"func", "var"});
    builder.IgnoreDecls(options.ignoreDecls);
    builder.IgnoreAnnotations(options.ignoreAnnotations);
}
} // namespace

/**
 * @brief 执行转换阶段, 获取AST并转换为源代码
 * @return 转换阶段执行成功返回true，否则返回false
 */
bool AstHelper::DoTransform() const
{
    Logger::Get().Debug("AstHelper::Run", "Get pkgs: ", pkgs.size());
    Ast2SourceVisitorBuilder asvBuilder;
    asvBuilder.Output(GetOutputDir());
    // 更新Builder
    UpdateAst2SourceVisitorBuilder(asvBuilder, options);
    Ast2SourceVisitor ast2SourceVisitor = asvBuilder.Build();
    for (auto pkg : pkgs) {
        Traverse(*pkg, ast2SourceVisitor);
    }
    return true;
}

/**
 * @brief 解析命令行参数, 拆分当前工具参数和前端工具透传参数
 */
void AstHelper::ParseArgs(const std::vector<std::string>& args)
{
    // 工具的有效选项:
    // key: {} 有效值集合， 空时表示不限制
    std::unordered_map<std::string, std::unordered_set<std::string>> validOpts = {
        {"dump-source", {"parse", "desugared-parse", "sema", "desugared-sema"}}, {"dump-imports", {}},
        {"filter-decls", {"func", "class", "interface", "struct", "enum", "var"}}, {"dump-desugar", {"true", "false"}},
        {"ignore-decls", {}}, {"ignore-annotations", {}}};
    std::vector<std::string> toolArgs;
    std::vector<std::string> ciArgs;
    auto isFilter = [&validOpts](const std::string& arg) {
        return std::any_of(validOpts.begin(), validOpts.end(),
            [&arg](const auto& k) { return arg.find(k.first) != std::string::npos; });
    };
    // 过滤当前工具参数和 CI 参数
    for (auto arg : args) {
        if (isFilter(arg)) {
            toolArgs.push_back(arg);
        } else {
            ciArgs.push_back(arg);
        }
    }
    Logger::Get().Debug(
        "AstHelper::ParseArgs", "args: ", args.size(), ", cjah: ", toolArgs.size(), ", ci: ", ciArgs.size());
    try {
        // 解析并获取工具选项配置
        ArgumentParser ap(validOpts);
        ap.Parse(toolArgs);
        auto stage = ap.GetSingleValue("dump-source", "");
        if (stage != "") {
            this->options.stage = key2Stage.at(stage);
        }
        auto imports = ap.GetMultiValue("dump-imports");
        if (imports.size() > 0) {
            this->options.stage = SourceStage::IMPORT;
            this->options.importedPkgs.insert(imports.begin(), imports.end());
        }
        // TODO: 添加选项互斥检查
        // TODO： 适配默认 false
        this->options.enableDesugar = ap.GetSingleValue("dump-desugar", "true") == "true";
        this->options.filterDecls = ap.GetMultiValue("filter-decls");
        this->options.ignoreDecls = ap.GetMultiValue("ignore-decls");
        this->options.ignoreAnnotations = ap.GetMultiValue("ignore-annotations");

        Logger::Get().Debug("AstHelper::ParseArgs", "Config: { desugar: ", this->options.enableDesugar, "}");
    } catch (InvalidArgumentException& iae) {
        Logger::Get().Error("AstHelper::ParseArgs", iae.what());
        ShowHelperInfo();
    }
    ci.ParseArgs(ciArgs);
}

// 私有函数实现
/**
 * 注册一个分析pass
 */
void AstHelper::RegisterPass(std::string name, std::unique_ptr<MutAstVisitorBase> visitor)
{
    passMap.emplace(name, std::move(visitor));
}

/**
 * 注册所有分析pass
 */
void AstHelper::RegisterPasses()
{
    RegisterPass("test", std::make_unique<TestPass>());
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

// Parse args from command line
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

// Parse environment filtered by some key vars
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
