/**
 * @file
 *
 * This file declares the AstHelper ArgParser.
 */

#pragma once
#include "utils/ArgumentParser.h"
#include "utils/Printer.h"

using StrVec = std::vector<std::string>;
using StrSet = std::unordered_set<std::string>;
using StrMap = std::unordered_map<std::string, std::string>;
using StrPair = std::pair<std::string, std::string>;
using StrPairVec = std::vector<StrPair>;
using ConStr = const std::string;

/**
 * @enum SourceStage
 * 表示源代码处理的不同阶段
 */
enum class SourceStage {
    DEFAULT = 0,     /**< No Source. */
    PARSE,           /**< Source of parsed ast. */
    DESUGARED_PARSE, /**< Source of desugared parsed ast. */
    IMPORT,          /**< Import Depend Packages for dump imports. */
    SEMA,            /**< Source of typechecked ast. */
    DESUGARED_SEMA,  /**< Source of desugared typechecked ast. */
};

/**
 * @brief helper 自定义选项定义
 */
struct Options {
    SourceStage stage = SourceStage::DEFAULT; /**< 当前的源代码阶段 */
    bool enableDesugar = false;               /**< 是否启用语法糖打印 */
    StrSet filterDecls;                       /**< 过滤打印decl配置 */
    StrSet ignoreDecls;                       /**< 忽略打印decl配置 */
    StrSet ignoreAnnotations;                 /**< 忽略打印annotations配置 */
    StrSet importedPkgs;                      /**< --dump-imported: 期望打印导入包的包名 */
    StrVec passes;                            /**< 配置需要执行的 passes 列表 */
    StrVec args;                              /**< 需要传递给前端的参数列表 */
    StrMap env;                               /**< 环境变量 */

    Options& Stage(ConStr& stage);
    Options& EnableDesugar(ConStr& enable);
    Options& FilterDecls(const StrVec& decl);
    Options& IgnoreDecls(const StrVec& decl);
    Options& IgnoreAnnotations(const StrVec& annotations);
    Options& ImportedPkgs(const StrVec& pkgs);
    Options& Passes(const StrVec& passes);
    Options& Args(StrVec&& args);
    Options& Env(StrMap&& env);
};

struct OptionDesc {
    std::string key;
    StrVec values;
    std::string mainDesc;
    StrPairVec subDesc;
    bool single;
    bool visible;

    OptionDesc& Key(std::string&& key);
    OptionDesc& Values(StrVec&& values);
    OptionDesc& MainDesc(std::string&& desc);
    OptionDesc& SubDesc(StrPairVec&& descs);
    OptionDesc& Single(bool single);
    OptionDesc& Visible(bool visible);
};

/**
 * @brief 命令行参数解析器 & helper 信息打印
 */
class ArgHelper {
public:
public:
    /**
     * @brief 构造ArgHelper实例
     * @param args 命令行参数的向量
     * @param env 环境变量的映射
     */
    ArgHelper();
    virtual ~ArgHelper() = default;

    /**
     * Parse arguments
     * @param args input arguments
     * @param env environment variables
     * @return parsed options
     */
    Options ParseArgs(int argc, const char* const* argv, const char* const* envp);

    void ShowHelperInfo();

private:
    void PL(ConStr& info, int blanks = 1);
    void PL(ConStr& opt, ConStr& desc, int blanks = 1);
    void PWILines(const StrPairVec& lines);

private:
    std::unordered_map<std::string, OptionDesc> validOptions; /* key: option, value: description */
    Printer p;
    int width = 32; // 对齐宽度
};
