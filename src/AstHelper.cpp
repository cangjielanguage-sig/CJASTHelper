/**
 * @file
 *
 * This file implements the AstHelper.
 */
#include "AstHelper.h"
#include "ArgumentParser.h"
#include "Ast2SourceVisitor.h"
#include "Logger.h"
#include "cangjie/Sema/Desugar.h"

using namespace Cangjie;

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
 * @brief 将字符串键映射到AstKind
 */
const std::unordered_map<std::string, AstKind> key2DeclKind{{"func", AstKind::FUNC_DECL},
    {"class", AstKind::CLASS_DECL}, {"interface", AstKind::INTERFACE_DECL}, {"struct", AstKind::STRUCT_DECL},
    {"var", AstKind::VAR_DECL}};
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
    for (int i = 0; i <= static_cast<int>(stage); i++) {
        if (i == static_cast<int>(SourceStage::DESUGARED_PARSE) && stage > SourceStage::DESUGARED_PARSE) {
            // Skip desugared parse stage when stage including sema.
            continue;
        }
        if (!stageMap.at(static_cast<SourceStage>(i))(this)) {
            return;
        }
    }
    auto pkgs = mci->GetSourcePackages();
    Logger::Get().Debug("AstHelper::Run", "Get pkgs: ", pkgs.size());
    Ast2SourceVisitor ast2SourceVisitor(GetOutputDir());
    if (stage >= SourceStage::DESUGARED_PARSE) {
        ast2SourceVisitor.EnableDusgar();
    }
    if (stage >= SourceStage::SEMA) {
        ast2SourceVisitor.EnableSeam();
    }
    for (auto& decl : filterDecls) {
        Logger::Get().Debug("AstHelper::Run", "Focus Decl: ", decl);
        ast2SourceVisitor.Focus(key2DeclKind.at(decl));
    }
    for (auto pkg : pkgs) {
        Logger::Get().Debug("AstHelper::Run", "Traverse ", pkg->fullPackageName, " by Ast2SourceVisitor");
        Traverse(*pkg, ast2SourceVisitor);
    }
}

void AstHelper::ParseArgs(const std::vector<std::string>& args)
{
    ArgumentParser ap({{"dump-source", {"parse", "desugared-parse", "sema", "desugared-sema"}},
        {"filter-decls", {"func", "class", "interface", "struct", "enum", "var"}}});
    const std::string& DS_KEY = "--dump-source=";
    std::vector<std::string> filterKeys{"--dump-source=", "--filter-decls"};
    std::vector<std::string> filterArgs;
    std::vector<std::string> ciArgs;
    auto isFilter = [&filterKeys](const std::string& arg) {
        return std::any_of(filterKeys.begin(), filterKeys.end(),
            [&arg](const std::string& k) { return arg.find(k) != std::string::npos; });
    };
    for (auto arg : args) {
        if (isFilter(arg)) {
            filterArgs.push_back(arg);
        } else {
            ciArgs.push_back(arg);
        }
    }
    try {
        ap.Parse(filterArgs);
        auto stage = ap.GetSingleValue("dump-source");
        this->stage = key2Stage.at(stage);
        this->filterDecls = ap.GetMultiValue("filter-decls");
    } catch (InvalidArgumentException& iae) {
        Logger::Get().Error("AstHelper::ParseArgs", iae.what(), ", dump all decls!");
    }
    Logger::Get().Debug("AstHelper::ParseArgs", "args: ", ciArgs.size());
    ci.ParseArgs(ciArgs);
}

bool AstHelper::Default()
{
    Logger::Get().Debug("AstHelper::Default", "input files: ", ci.globalOptions.srcFiles.size());
    Logger::Get().Debug("AstHelper::Default", "Output", GetOutputDir());
    return true;
}

bool AstHelper::Parse()
{
    Logger::Get().Debug("AstHelper::Parse");
    Logger::Get().Debug("AstHelper::Parse", "file paths: ", mci->srcFilePaths.size());

    return mci->PerformParse();
}

bool AstHelper::DesugaredParse()
{
    Logger::Get().Debug("AstHelper::DesugaredParse");
    for (auto& pkg : mci->GetPackages()) {
        PerformDesugarBeforeTypeCheck(*pkg);
    }
    return true;
}

bool AstHelper::Sema()
{
    Logger::Get().Debug("AstHelper::Sema");
    // Necessary pipeline
    mci->PerformImportPackage();
    return mci->PerformSema();
}

bool AstHelper::DesugaredSema()
{
    Logger::Get().Debug("AstHelper::DesugaredSema");
    return mci->PerformDesugarAfterSema();
}

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

// Filter some key vars
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