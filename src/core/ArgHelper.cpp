/**
 * @file
 *
 * This file implements the ArgParser & ArgHelper.
 */
#include "core/ArgHelper.h"
#include "core/pass/Pass.h"
#include "utils/ArgParser.h"
#include "utils/FileHelper.h"
#include "utils/Logger.h"
#include <charconv>
#include <iomanip>
#include <stdexcept>

/// Option 配置方法实现
namespace {
/**
 * @brief 将字符串键映射到SourceStage值
 */
ConStrMap<SourceStage> key2Stage{{"parse", SourceStage::PARSE}, {"desugared-parse", SourceStage::DESUGARED_PARSE},
    {"macro", SourceStage::MACRO_EXPAND}, {"sema", SourceStage::SEMA}, {"desugared-sema", SourceStage::DESUGARED_SEMA}};
} // namespace
Options& Options::Stage(ConStr& stage)
{
    this->stage = key2Stage.at(stage);
    return *this;
}
Options& Options::EnableDesugar(ConStr& enable)
{
    this->enableDesugar = enable == "true";
    return *this;
}

Options& Options::EnableMacro(ConStr& enable)
{
    this->enableMacro = enable == "true";
    return *this;
}

Options& Options::EnableAstOutPath(ConStr& path)
{
    if (!path.empty()) {
        this->astOutPath = path;
    }
    return *this;
}

Options& Options::FilterDecls(ConStrVec& decl)
{
    this->filterDecls = StrSet{decl.begin(), decl.end()};
    return *this;
}
Options& Options::IgnoreDecls(ConStrVec& decl)
{
    this->ignoreDecls = StrSet{decl.begin(), decl.end()};
    return *this;
}
Options& Options::IgnoreAnnotations(ConStrVec& annotations)
{
    this->ignoreAnnotations = StrSet{annotations.begin(), annotations.end()};
    return *this;
}
Options& Options::ImportedPkgs(ConStrVec& pkgs)
{
    if (pkgs.empty()) {
        return *this;
    }
    this->stage = SourceStage::IMPORT;
    this->importedPkgs = StrSet{pkgs.begin(), pkgs.end()};
    return *this;
}
Options& Options::Passes(ConStrVec& passes)
{
    this->passes = passes;
    return *this;
}
Options& Options::Args(StrVec&& args)
{
    this->args = std::move(args);
    return *this;
}
Options& Options::Env(StrMap<Str>&& env)
{
    this->env = std::move(env);
    return *this;
}

namespace nlohmann {
template <> struct adl_serializer<OptionDesc> {
    // 只实现反序列化
    static void from_json(const json& j, OptionDesc& od)
    {
        j.at("key").get_to(od.key);
        j.at("values").get_to(od.values);
        j.at("mainDesc").get_to(od.mainDesc);
        for (const auto& item : j.at("subDesc")) {
            od.subDesc.emplace_back(item["sub"], item["desc"]);
        }
        j.at("single").get_to(od.single);
        j.at("visible").get_to(od.visible);
    }
};

template <> struct adl_serializer<Options> {
    // 只实现反序列化
    static void from_json(const json& j, Options& op)
    {
        op.Stage(j.at("stage").get<Str>());
        j.at("enableDesugar").get_to(op.enableDesugar);
        j.at("enableMacro").get_to(op.enableMacro);
        j.at("filterDecls").get_to(op.filterDecls);
        j.at("ignoreDecls").get_to(op.ignoreDecls);
        j.at("ignoreAnnotations").get_to(op.ignoreAnnotations);
        j.at("passes").get_to(op.passes);
        j.at("args").get_to(op.args);
        op.args.insert(op.args.begin(), "cjah");
    }
};
} // namespace nlohmann

/// ArgHelper 实现函数
ArgHelper::ArgHelper() : p(std::cout, 4)
{
    ConfigParser parser("valid_options.json");
    auto opts = parser.Parse<Vec<OptionDesc>>();
    for (auto& od : opts) {
        validOptions.emplace(od.key, od);
    }
}

void ArgHelper::ShowHelperInfo()
{
    std::ios::fmtflags bakflags(std::cout.flags());
    std::cout << std::left;
    PL("Welcome Using Cangjie AST Helper!", 2);
    PL("Usage: cjah [options] [cjc-options]", 2);
    PL("options: ");
    p.Indent();
    for (auto& [key, value] : validOptions) {
        if (!value.visible)
            continue;
        if (key == "help") {
            PL("--help", value.mainDesc);
            continue;
        }
        PL("--" + key + "=<value>", value.mainDesc);
        PWILines(value.subDesc);
        p.PNL();
    }
    p.Unindent();
    p.PNL();
    PL("cjc-options: please refer to `cjc -h`.");
    p.Flush();
    std::cout.flags(bakflags);
}

namespace {
// Parse args from command line
/**
 * @brief 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数字符串数组
 * @return 解析后的参数向量
 */
StrVec ParseRawArgs(int argc, const char* const* argv)
{
    StrVec args;
    for (int i = 0; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }
        args.emplace_back(argv[i]);
    }
    return std::move(args);
}

inline bool ContainHelpArg(ConStrVec& args)
{
    for (auto arg : args) {
        if (arg == "--help" || arg == "-h") {
            return true;
        }
    }
    return false;
}

inline void SplitArgs(ConStrVec& args, StrVec& toolArgs, StrVec& ciArgs, ConStrMap<StrSet>& validOpts)
{
    // 过滤当前工具参数和其它参数
    for (auto& arg : args) {
        if (std::any_of(
                validOpts.begin(), validOpts.end(), [&arg](const auto& k) { return arg.find(k.first) != Str::npos; })) {
            toolArgs.push_back(arg);
        } else {
            ciArgs.push_back(arg);
        }
    }
}

/**
 * @brief 解析环境变量
 * @param envp 环境变量字符串数组
 * @param focus 需要关注的键集合
 * @return 解析后的环境变量映射
 */
StrMap<Str> ParseEnv(const char* const* envp, const StrSet& focus)
{
    StrMap<Str> env;
    if (!envp) {
        return env;
    }
    int i = 0;
    constexpr char ASSIGN = '=';
    while (true) {
        if (!envp[i]) {
            break;
        }
        Str kv(envp[i]);
        if (auto pos = kv.find(ASSIGN); pos != Str::npos) {
            Str key = kv.substr(0, pos);
            if (focus.find(key) != focus.end()) {
                env.emplace(key, kv.substr(pos + 1));
            }
        }
        i++;
    }
    return std::move(env);
}

void ConfigGlobalOptions(ArgumentParser& ap)
{
    // 解析并获取pass配置
    auto passCfg = ap.GetSingleValue("pass-config", "passes.json");
    LOGD("pass config: ", passCfg);
    PassManager::Init(passCfg);
    // 解析并获取 parallels
    int val = 1;
    auto parallels = ap.GetSingleValue("parallel-tasks", "1");
    auto [_, ec] = std::from_chars(parallels.data(), parallels.data() + parallels.size(), val);
    if (ec == std::errc()) {
        Options::parallels = val;
    }
    LOGD("paralles: ", Options::parallels);
}

void ConfigOptions(Options& options, ArgumentParser& ap)
{
    // config options
    options.Stage(ap.GetSingleValue("dump-source"));
    options.EnableAstOutPath(ap.GetSingleValue("dump-ast", ""));
    options.FilterDecls(ap.GetMultiValue("filter-decls"));
    // TODO: update default false
    options.EnableDesugar(ap.GetSingleValue("enable-desugar", "true"));
    options.EnableMacro(ap.GetSingleValue("enable-macro", "true"));
    options.IgnoreAnnotations(ap.GetMultiValue("ignore-annotations"));
    options.IgnoreDecls(ap.GetMultiValue("ignore-decls"));
    options.ImportedPkgs(ap.GetMultiValue("dump-import"));

    auto passes = ap.GetMultiValue("enable-passes");
    if (!passes.empty()) {
        options.Passes(passes);
        return;
    }
    // config passes
    if (options.stage > SourceStage::PARSE) {
        options.passes.push_back("check-desugar");
        if (options.enableDesugar) {
            options.passes.push_back("replace-desugar");
        } else {
            options.passes.push_back("recover-desugar");
        }
        options.passes.push_back("check-desugar");
    }
    // 添加 to-cangjie 作为最后一个 pass
    options.passes.push_back("to-cangjie");
}
} // namespace
Vec<Options> ArgHelper::ParseArgs(int argc, const char* const* argv, const char* const* envp)
{
    Options options;
    StrVec args = ParseRawArgs(argc, argv);
    // Help
    if (ContainHelpArg(args)) {
        return {options};
    }
    StrMap<StrSet> validOpts;
    for (auto& [key, opt] : validOptions) {
        validOpts.emplace(key, StrSet(opt.values.begin(), opt.values.end()));
    }

    StrVec toolArgs;
    SplitArgs(args, toolArgs, options.args, validOpts);
    try {
        Options::env = ParseEnv(
            envp, {"CANGJIE_PATH", "CANGJIE_HOME", "LIBRARY_PATH", "LD_LIBRARY_PATH", "PATH", "SDKROOT", "cjHeapSize"});
        // 解析并获取工具选项配置
        ArgumentParser ap(validOpts);
        ap.Parse(toolArgs);
        // 配置全局选项
        ConfigGlobalOptions(ap);
        // 解析并获取task配置
        auto taskCfg = ap.GetSingleValue("task-config", "");
        LOGD("task config: ", taskCfg);
        if (!taskCfg.empty()) {
            ConfigParser parser(taskCfg);
            return parser.Parse<Vec<Options>>();
        }
        ConfigOptions(options, ap);
    } catch (std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << std::endl;
        // Only do show help info.
        options.stage = SourceStage::DEFAULT;
    }
    return {options};
}

inline void ArgHelper::PL(ConStr& info, int blanks)
{
    p << std::setw(width) << info;
    p.PNL(blanks);
}

inline void ArgHelper::PL(ConStr& opt, ConStr& desc, int blanks)
{
    p << std::setw(width) << opt << desc;
    p.PNL(blanks);
}

inline void ArgHelper::PWILines(ConStrPairVec& lines)
{
    p.Indent();
    for (const auto& [p0, p1] : lines) {
        PL(p0, p1);
    }
    p.Unindent();
}
