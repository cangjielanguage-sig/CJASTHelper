/**
 * @file
 *
 * This file implements the ArgParser & ArgHelper.
 */
#include "core/ArgHelper.h"
#include "utils/ArgParser.h"
#include "utils/FileHelper.h"
#include "utils/Printer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

/// OptionDes 实现函数

OptionDesc& OptionDesc::Key(std::string&& key)
{
    this->key = std::move(key);
    return *this;
}
OptionDesc& OptionDesc::Values(StrVec&& values)
{
    this->values = std::move(values);
    return *this;
}
OptionDesc& OptionDesc::MainDesc(std::string&& desc)
{
    mainDesc = std::move(desc);
    return *this;
}
OptionDesc& OptionDesc::SubDesc(StrPairVec&& descs)
{
    this->subDesc = std::move(descs);
    return *this;
}
OptionDesc& OptionDesc::Single(bool single)
{
    this->single = single;
    return *this;
}
OptionDesc& OptionDesc::Visible(bool visible)
{
    this->visible = visible;
    return *this;
}
/// Option 配置方法实现
namespace {
/**
 * @brief 将字符串键映射到SourceStage值
 */
ConStrMap<SourceStage> key2Stage{{"parse", SourceStage::PARSE}, {"desugared-parse", SourceStage::DESUGARED_PARSE},
    {"sema", SourceStage::SEMA}, {"desugared-sema", SourceStage::DESUGARED_SEMA}};
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

Options& Options::PassConfig(Str&& path)
{
    this->passConfig = std::move(path);
    return *this;
}

/// ArgHelper 实现函数
ArgHelper::ArgHelper() : p(std::cout, 4)
{
    OptionDesc od;
    // --dump-source=[parse, desugared-parse, sema, desugared-sema]
    od.Key("dump-source")
        .MainDesc("Dump source after <value> stage. Supported value: parse, desugared-parse, sema, desugared-sema.")
        .Values({"parse", "desugared-parse", "sema", "desugared-sema"})
        .Visible(true)
        .Single(true)
        .SubDesc({});
    validOptions.emplace("dump-source", od);
    // --pass-config=./config/pass.json
    od.Key("pass-config").MainDesc("Pass config path, default: ./config/passes.json.");
    validOptions.emplace("pass-config", od);
    // --enable-macro=[true,false]
    od.Key("enable-macro")
        .MainDesc("Enable macro when <value> is true. Supported <value>: (default) true, false.")
        .Values({"true", "false"})
        .SubDesc({{"<value>=true", "Do macro expansion, please ensure the runtime and macro libs are in CANGJIE_HOME."},
            {"<value>=false", "Skip macro expansions when no macro using."}});
    validOptions.emplace("enable-macro", od);
    // --filter-decls=id1,id2,id3
    od.Key("filter-decls")
        .MainDesc("Filter top-level decls of <value>. Supported <value>: func, var, struct, enum, interface, class")
        .Values({"func", "var", "struct", "enum", "interface", "class"})
        .SubDesc(
            {{"<value>=func", "Dump functions."}, {"<kinds>=func,class", "Dump functions and classes."}, {"...", ""}})
        .Single(false);
    validOptions.emplace("filter-decls", od);

    // --help
    od.Key("help").MainDesc("Show help info.").Values({}).SubDesc({});
    validOptions.emplace("help", od);
    // --ignore-decls=id1,id2,id3
    od.Key("ignore-decls").MainDesc("Ignore top-level decls whoes identifier is <value>.").Values({}).Visible(false);
    validOptions.emplace("ignore-decls", od);
    // --ignore-annotations=id1,id2,id3
    od.Key("ignore-annotations").MainDesc("Ignore annotations whoes identifier is <value>.").Values({});
    validOptions.emplace("ignore-annotations", od);
    // --enable-desugar=[true,false]
    od.Key("enable-desugar")
        .MainDesc("Dump desugared code when <value> is true. Supported <value>: (default) true, false.")
        .Values({"true", "false"})
        .Single(true);
    validOptions.emplace("enable-desugar", od);
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

inline void SplitArgs(
    ConStrVec& args, StrVec& toolArgs, StrVec& ciArgs, const std::unordered_map<std::string, StrSet>& validOpts)
{
    // 过滤当前工具参数和其它参数
    for (auto& arg : args) {
        if (std::any_of(validOpts.begin(), validOpts.end(),
                [&arg](const auto& k) { return arg.find(k.first) != std::string::npos; })) {
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
std::unordered_map<std::string, std::string> ParseEnv(const char* const* envp, const StrSet& focus)
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
    return std::move(env);
}

void ValidateConfigPath(Str& path)
{
    if (!path.empty()) {
        if (!CheckExist(path)) {
            throw std::invalid_argument("the path of config file is not exist: " + path);
        }
    }
    // path is empty, find default path
    auto pre = getExecutablePath().parent_path().string();
    auto suf = "config/passes.json";
    StrVec candidatesPaths{pre + "/" + suf, pre + "/../" + suf, pre + "/../../" + suf};
    for (auto& p : candidatesPaths) {
        if (CheckExist(p)) {
            path = p;
            return;
        }
    }
    throw std::invalid_argument(
        "The pass config file is not found, please provide by `--pass-config=./config/passes.json`");
}
} // namespace
Options ArgHelper::ParseArgs(int argc, const char* const* argv, const char* const* envp)
{
    Options options;
    StrVec args = ParseRawArgs(argc, argv);
    // Help
    if (ContainHelpArg(args)) {
        return options;
    }
    StrMap<StrSet> validOpts;
    for (auto& [key, opt] : validOptions) {
        validOpts.emplace(key, StrSet(opt.values.begin(), opt.values.end()));
    }

    StrVec toolArgs;
    SplitArgs(args, toolArgs, options.args, validOpts);
    try {
        // 解析并获取工具选项配置
        ArgumentParser ap(validOpts);
        ap.Parse(toolArgs);
        // config options
        options.Stage(ap.GetSingleValue("dump-source"));
        options.FilterDecls(ap.GetMultiValue("filter-decls"));
        // TODO: update default false
        options.EnableDesugar(ap.GetSingleValue("enable-desugar", "true"));
        options.EnableMacro(ap.GetSingleValue("enable-macro", "true"));
        options.IgnoreAnnotations(ap.GetMultiValue("ignore-annotations"));
        options.IgnoreDecls(ap.GetMultiValue("ignore-decls"));
        options.ImportedPkgs(ap.GetMultiValue("dump-import"));

        Str configPath = ap.GetSingleValue("pass-config", "");
        ValidateConfigPath(configPath);
        options.PassConfig(std::move(configPath));
        // config passes
        if (options.stage > SourceStage::PARSE && options.enableDesugar) {
            options.passes.push_back("check-desugar");
            options.passes.push_back("replace-desugar");
            options.passes.push_back("check-desugar");
            options.passes.push_back("to-java");
        }
        // 添加 to-source 作为最后一个 pass
        options.passes.push_back("to-cangjie");
        // config env
        options.env =
            ParseEnv(envp, {"CANGJIE_PATH", "CANGJIE_HOME", "LIBRARY_PATH", "LD_LIBRARY_PATH", "PATH", "SDKROOT"});
    } catch (std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << std::endl;
        // Only do show help info.
        options.stage = SourceStage::DEFAULT;
    }
    return options;
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