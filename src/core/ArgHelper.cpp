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
#include <map>
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

Options& Options::CheckSyntax(ConStr& enable)
{
    this->checkSyntax = enable == "true";
    return *this;
}

Options& Options::EnableAstOutPath(ConStr& path)
{
    if (!path.empty()) {
        this->astOutPath = path;
    }
    return *this;
}

Options& Options::EnableAstTreePrint(ConStr& path)
{
    if (!path.empty()) {
        this->astTreeOutPath = path;
        this->passes.push_back("ast-tree-print");   // --print-ast 走 pass（实现已迁入 AstTreePrintPass）
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
        // 语法检查模式: 强制只跑 parse 阶段, 跳过分析与转换
        op.checkSyntax = j.value("checkSyntax", false);
        if (op.checkSyntax) {
            op.stage = SourceStage::PARSE;
            op.passes.clear();
        }
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
    // ★ 语法检查模式不执行任何 pass（check-syntax 只跑 parse 阶段）⇒ 不加载 pass 配置。
    //   好处：最小发布包（config/ 仅含 valid_options.json）无需携带 passes.json；
    //   其它模式保持原有强校验：pass 配置缺失即报错，避免静默少跑 pass。
    if (ap.GetSingleValue("check-syntax", "false") != "true") {
        // 解析并获取pass配置
        auto passCfg = ap.GetSingleValue("pass-config", "passes.json");
        LOGD("pass config: ", passCfg);
        PassManager::Init(passCfg);
    } else {
        LOGD("check-syntax mode: skip pass config loading");
    }
    // 解析并获取 parallels
    int val = 1;
    auto parallels = ap.GetSingleValue("parallel-tasks", "1");
    auto [_, ec] = std::from_chars(parallels.data(), parallels.data() + parallels.size(), val);
    if (ec == std::errc()) {
        Options::SetParallels(val);
    }
    LOGD("paralles: ", Options::Parallels());
}

namespace {
/**
 * @brief cjc 透传选项里「取值写在下一个 token」的选项清单。
 *
 * check-syntax 需要按包重排输入文件，必须把这些取值从输入列表里剔除：
 * 否则 `--output-dir /tmp/out` 的取值 `/tmp/out` 会被当成输入目录，
 * 而孤立的 `--output-dir` 又会把后面的 `.cj` 路径吃成它的取值
 * ⇒ 输入列表变空 ⇒ 前端不报错 ⇒ 误判为「语法正确」（退出码 0）。
 *
 * 注意：清单只影响 check-syntax 的输入识别；用 `--opt=value` 写法的参数不受影响。
 */
bool IsValueTakingOpt(ConStr& arg)
{
    static const StrSet opts{
        "-o", "--output-dir", "--output-type", "--target", "--sysroot", "-L", "-l", "--import-path",
        "--module-name", "--package", "--link-options", "--macro-lib", "-j", "--jobs", "--error-count-limit",
        "--diagnostic-format", "--save-temps", "--prefix", "--profile", "--coverage", "--sanitizer", "--lto",
        "--env", "--target-cpu", "--module-version", "--features"};
    return opts.count(arg) > 0;
}

/**
 * @brief 参数里是否已显式指定输出目录（`--output-dir <dir>` / `--output-dir=<dir>` / `-o`）
 */
bool HasOutputDirArg(ConStrVec& args)
{
    for (auto& arg : args) {
        if (arg == "--output-dir" || arg == "-o" || arg.rfind("--output-dir=", 0) == 0) {
            return true;
        }
    }
    return false;
}
} // namespace

/**
 * @brief 配置选项并生成检查任务
 * check-syntax 模式下, 若输入是目录, 按包分组: 每个直接包含 .cj 文件的目录视为一个包,
 * 返回 每个包一个 Options(各自独立一次前端调用), 避免把多个包的文件混在一次前端调用中
 * 导致部分包的文件未被解析而漏报语法错误。其余模式返回单个 Options。
 */
Vec<Options> ConfigOptions(ArgumentParser& ap, Options options)
{
    // config options
    options.CheckSyntax(ap.GetSingleValue("check-syntax", "false"));
    if (options.checkSyntax) {
        // 语法检查模式: 只需要 parse 阶段, 无需 --dump-source, 不配置任何 pass
        options.Stage("parse");

        // ★ cjc 缺省把输出目录当作 "./"：只读 cwd 下前端因写不进去而提前退出，且不产生诊断
        //   ⇒ 会被误判为「语法正确」。未显式给出输出目录且 cwd 不可写时，改用进程专属临时目录。
        if (!HasOutputDirArg(options.args)) {
            Str cwd = ".";
            if (!IsDirWritable(cwd)) {
                Str tmp = CreateTempDir("cjah-check-syntax");
                if (tmp.empty()) {
                    throw std::invalid_argument(
                        "current directory is not writable; please pass --output-dir=<writable dir>");
                }
                std::cerr << "warning: current directory is not writable, using temporary output dir: " << tmp
                          << std::endl;
                options.args.push_back("--output-dir=" + tmp);
            }
        }

        // 按包分组: 包目录(排序保证包序确定) -> 该目录下的 .cj 文件
        std::map<Str, StrVec> pkgFiles;
        StrVec commonArgs;
        if (!options.args.empty()) {
            commonArgs.push_back(options.args.front()); // argv0 保留在每个包的最前面
        }
        for (size_t i = 1; i < options.args.size(); ++i) {
            auto& arg = options.args[i];
            // ★ 透传选项及其取值一律不参与输入识别，否则 `--output-dir /tmp/out` 会被
            //   拆成「输入目录 /tmp/out」+「孤立 --output-dir（把 .cj 吃成取值）」⇒ 假通过
            if (!arg.empty() && arg[0] == '-') {
                commonArgs.push_back(arg);
                if (IsValueTakingOpt(arg) && i + 1 < options.args.size()) {
                    commonArgs.push_back(options.args[++i]);
                }
                continue;
            }
            std::error_code ec;
            if (std::filesystem::is_directory(arg, ec)) {
                auto groups = GroupCjFilesByDir(arg);
                if (groups.empty()) {
                    std::cerr << "warning: no .cj file found in directory: " << arg << std::endl;
                }
                for (auto& [pkgDir, files] : groups) {
                    auto& dst = pkgFiles[pkgDir];
                    dst.insert(dst.end(), files.begin(), files.end());
                }
            } else if (std::filesystem::is_regular_file(arg, ec) && arg.ends_with(".cj")) {
                // 显式指定的 .cj 文件: 按所在目录归入对应包
                auto dir = std::filesystem::path(arg).parent_path().string();
                pkgFiles[dir.empty() ? "." : dir].push_back(arg);
            } else {
                commonArgs.push_back(arg);
            }
        }
        if (pkgFiles.empty()) {
            // ★ 没有任何可检查的 .cj 输入时必须报错：前端拿不到输入不会报错，
            //   静默返回会变成「假通过」（退出码 0）。
            throw std::invalid_argument(
                "no .cj input found; pass a .cj file or a directory containing .cj files");
        }
        Vec<Options> opts;
        for (auto& [pkgDir, files] : pkgFiles) {
            std::sort(files.begin(), files.end());
            files.erase(std::unique(files.begin(), files.end()), files.end());
            Options o = options;
            o.args = commonArgs;
            o.args.insert(o.args.end(), files.begin(), files.end());
            opts.push_back(std::move(o));
        }
        return opts;
    }
    // ★ `--dump-source` 缺省时不能直接取单值（会抛 "Invalid option: dump-source"）：
    //   只要给了别的「动作型」选项（--print-ast / --dump-ast / --enable-passes），就按最小
    //   可用阶段 parse 跑；一个动作都没给才保持 DEFAULT（main 打印帮助）。
    const auto dumpSource = ap.GetSingleValue("dump-source", "");
    options.EnableAstOutPath(ap.GetSingleValue("dump-ast", ""));
    options.EnableAstTreePrint(ap.GetSingleValue("print-ast", ""));
    const auto passes = ap.GetMultiValue("enable-passes");
    if (!dumpSource.empty()) {
        options.Stage(dumpSource);
    } else if (options.astOutPath.has_value() || options.astTreeOutPath.has_value() || !passes.empty()) {
        options.Stage("parse");
    }
    options.FilterDecls(ap.GetMultiValue("filter-decls"));
    // TODO: update default false
    options.EnableDesugar(ap.GetSingleValue("enable-desugar", "true"));
    options.EnableMacro(ap.GetSingleValue("enable-macro", "true"));
    options.IgnoreAnnotations(ap.GetMultiValue("ignore-annotations"));
    options.IgnoreDecls(ap.GetMultiValue("ignore-decls"));
    options.ImportedPkgs(ap.GetMultiValue("dump-import"));

    if (!passes.empty()) {
        options.Passes(passes);
        return {std::move(options)};
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
    return {std::move(options)};
}
} // namespace

/**
 * 任务并发度的唯一存储点(core DLL 单 TU 导出, exe 经导入解析访问,
 * 避免 static inline 成员跨 DLL 各持副本)。
 */
namespace {
int& ParallelCountStorage()
{
    static int parallels = 1;
    return parallels;
}
} // namespace

int Options::Parallels()
{
    return ParallelCountStorage();
}

void Options::SetParallels(int value)
{
    ParallelCountStorage() = value;
}

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
    if (toolArgs.empty()) {
        // 没有任何工具选项：`cjah` 裸调用保持「仅展示帮助」并返回成功；
        // 但用户确实传了参数（误用本版本裁掉的选项、或只给了 cjc 透传参数）时若仍返回 0，
        // 脚本会把它当成「检查通过」⇒ 假通过。此类情况标记解析失败，由 main 以非 0 退出。
        if (args.size() > 1) {
            std::cerr << "error: no recognized cjah tool option in arguments; nothing was done (see --help)"
                      << std::endl;
            options.valid = false;
        }
        return {options};
    }
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
        return ConfigOptions(ap, options);
    } catch (std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << std::endl;
        // Only do show help info.
        options.stage = SourceStage::DEFAULT;
        options.valid = false;
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
