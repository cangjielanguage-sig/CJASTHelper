/**
 * @file
 *
 * This file declares the AstHelper.
 */

#pragma once
#include "utils/Printer.h"
#include "utils/types/TypeAlias.h"

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
    bool enableMacro = true;                  /**< 是否启用宏展开 */
    StrSet filterDecls;                       /**< 过滤打印decl配置 */
    StrSet ignoreDecls;                       /**< 忽略打印decl配置 */
    StrSet ignoreAnnotations;                 /**< 忽略打印annotations配置 */
    StrSet importedPkgs;                      /**< --dump-imported: 期望打印导入包的包名 */
    StrVec passes;                            /**< 配置需要执行的 passes 列表 */
    StrVec args;                              /**< 需要传递给前端的参数列表 */
    StrMap<Str> env;                          /**< 环境变量 */
    Str passConfig;                           /**< 配置需要使用的 passes 配置文件 */

    Options& Stage(ConStr& stage);
    Options& EnableDesugar(ConStr& enable);
    Options& EnableMacro(ConStr& enable);
    Options& FilterDecls(ConStrVec& decl);
    Options& IgnoreDecls(ConStrVec& decl);
    Options& IgnoreAnnotations(ConStrVec& annotations);
    Options& ImportedPkgs(ConStrVec& pkgs);
    Options& Passes(ConStrVec& passes);
    Options& Args(StrVec&& args);
    Options& Env(StrMap<Str>&& env);
    Options& PassConfig(Str&& path);
};

struct OptionDesc {
    std::string key;      /**< key of option */
    StrVec values;        /**< values of option */
    std::string mainDesc; /**< main description of option */
    StrPairVec subDesc;   /**< sub description of option */
    bool single;          /**< whether option is single */
    bool visible;         /**< whether option is visible */

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

    /**
     * Show helper info
     */
    void ShowHelperInfo();

private:
    void PL(ConStr& info, int blanks = 1);
    void PL(ConStr& opt, ConStr& desc, int blanks = 1);
    void PWILines(ConStrPairVec& lines);

private:
    StrMap<OptionDesc> validOptions; /* key: option, value: description */
    Printer p;
    int width = 32; // 左对齐宽度
};
