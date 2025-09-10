/**
 * @file
 *
 * This file implements the ArgParser & ArgHelper.
 */
#include "utils/ArgHelper.h"
#include "utils/Printer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

/// ArgParser

ArgumentParser::ArgumentParser(const std::unordered_map<std::string, std::unordered_set<std::string>>& validOptions)
    : validOptions(validOptions)
{
}

void ArgumentParser::Parse(const std::vector<std::string>& args)
{
    for (const auto& arg : args) {
        if (arg.rfind("--", 0) != 0) {
            throw std::invalid_argument("ArgumentParser: Invalid argument format: " + arg);
        }

        size_t equalPos = arg.find('=');
        if (equalPos == std::string::npos) {
            throw std::invalid_argument("ArgumentParser: Missing '=' in argument: " + arg);
        }

        std::string option = arg.substr(2, equalPos - 2);
        std::string valueStr = arg.substr(equalPos + 1);

        std::vector<std::string> values;
        std::stringstream ss(valueStr);
        std::string value;
        while (std::getline(ss, value, ',')) {
            values.push_back(value);
        }
        ValidateOption(option, values);
        parsedOptions[option] = values;
    }
}

std::string ArgumentParser::GetSingleValue(const std::string& option, const std::string& dv) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end() || it->second.size() != 1) {
        return dv;
    }
    return it->second[0];
}

std::vector<std::string> ArgumentParser::GetMultiValue(const std::string& option) const
{
    auto it = parsedOptions.find(option);
    if (it == parsedOptions.end()) {
        return {};
    }
    return it->second;
}

void ArgumentParser::ValidateOption(const std::string& option, const std::vector<std::string>& values) const
{
    auto it = validOptions.find(option);
    if (it == validOptions.end()) {
        throw std::invalid_argument("ArgumentParser: Invalid option: " + option);
    }
    // 未配置有效选项值，默认不限制
    if (it->second.empty()) {
        return;
    }
    for (const auto& value : values) {
        if (it->second.find(value) == it->second.end()) {
            throw std::invalid_argument("ArgumentParser: Invalid value for option " + option + ": " + value);
        }
    }
}

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
const std::unordered_map<std::string, SourceStage> key2Stage{{"parse", SourceStage::PARSE},
    {"desugared-parse", SourceStage::DESUGARED_PARSE}, {"sema", SourceStage::SEMA},
    {"desugared-sema", SourceStage::DESUGARED_SEMA}};
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
Options& Options::FilterDecls(const StrVec& decl)
{
    this->filterDecls = std::unordered_set<std::string>{decl.begin(), decl.end()};
    return *this;
}
Options& Options::IgnoreDecls(const StrVec& decl)
{
    this->ignoreDecls = std::unordered_set<std::string>{decl.begin(), decl.end()};
    return *this;
}
Options& Options::IgnoreAnnotations(const StrVec& annotations)
{
    this->ignoreAnnotations = std::unordered_set<std::string>{annotations.begin(), annotations.end()};
    return *this;
}
Options& Options::ImportedPkgs(const StrVec& pkgs)
{
    if (pkgs.empty()) {
        return *this;
    }
    this->stage = SourceStage::IMPORT;
    this->importedPkgs = std::unordered_set<std::string>{pkgs.begin(), pkgs.end()};
    return *this;
}
Options& Options::Passes(const StrVec& passes)
{
    this->passes = passes;
    return *this;
}
Options& Options::Args(StrVec&& args)
{
    this->args = std::move(args);
    return *this;
}
Options& Options::Env(StrMap&& env)
{
    this->env = std::move(env);
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
    PL("Options: ");
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
std::vector<std::string> ParseRawArgs(int argc, const char* const* argv)
{
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }
        args.emplace_back(argv[i]);
    }
    return std::move(args);
}

inline bool ContainHelpArg(const StrVec& args)
{
    for (auto arg : args) {
        if (arg == "--help" || arg == "-h") {
            return true;
        }
    }
    return false;
}

inline void SplitArgs(
    const StrVec& args, StrVec& toolArgs, StrVec& ciArgs, const std::unordered_map<std::string, StrSet>& validOpts)
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
} // namespace
Options ArgHelper::ParseArgs(int argc, const char* const* argv, const char* const* envp)
{
    Options options;
    std::vector<std::string> args = ParseRawArgs(argc, argv);
    if (ContainHelpArg(args)) {
        return options;
    }
    std::unordered_map<std::string, StrSet> validOpts;
    for (auto& [key, opt] : validOptions) {
        validOpts.emplace(key, std::unordered_set<std::string>(opt.values.begin(), opt.values.end()));
    }

    StrVec toolArgs;
    SplitArgs(args, toolArgs, options.args, validOpts);
    try {
        // 解析并获取工具选项配置
        ArgumentParser ap(validOpts);
        ap.Parse(toolArgs);
        // config options
        options.Stage(ap.GetSingleValue("dump-source", ""));
        options.FilterDecls(ap.GetMultiValue("filter-decls"));
        // TODO: update default false
        options.EnableDesugar(ap.GetSingleValue("enable-desugar", "true"));
        options.IgnoreAnnotations(ap.GetMultiValue("ignore-annotations"));
        options.IgnoreDecls(ap.GetMultiValue("ignore-decls"));
        options.ImportedPkgs(ap.GetMultiValue("dump-import"));
        // config passes
        if (options.stage > SourceStage::PARSE && options.enableDesugar) {
            options.passes.push_back("check-desugar");
            options.passes.push_back("replace-desugar");
            options.passes.push_back("check-desugar");
        }
        // 添加 to-source 作为最后一个 pass
        options.passes.push_back("to-source");
        // config env
        options.env =
            ParseEnv(envp, {"CANGJIE_PATH", "CANGJIE_HOME", "LIBRARY_PATH", "LD_LIBRARY_PATH", "PATH", "SDKROOT"});
    } catch (std::invalid_argument& e) {
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

inline void ArgHelper::PWILines(const StrPairVec& lines)
{
    p.Indent();
    for (const auto& [p0, p1] : lines) {
        PL(p0, p1);
    }
    p.Unindent();
}