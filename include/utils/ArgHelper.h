/**
 * @file
 *
 * This file declares the ArgParser & AstHelper.
 */

#pragma once
#include "utils/Printer.h"
#include "utils/TypeAlias.h"

/**
 * @class ArgumentParser
 * @brief 用于解析命令行参数的类。
 */
class ArgumentParser {
public:
    /**
     * @brief 构造函数，初始化合法选项及其取值范围。
     * @param validOptions 合法选项及其对应的取值集合。
     */
    explicit ArgumentParser(const std::unordered_map<std::string, std::unordered_set<std::string>>& validOptions);

    /**
     * @brief 解析命令行参数。
     * @param args 命令行参数列表。
     */
    void Parse(const std::vector<std::string>& args);

    /**
     * @brief 获取单值选项的值。
     * @param option 选项名称。
     * @return 选项的值, 如果不存在则抛异常。
     */
    std::string GetSingleValue(const std::string& option) const;

    /**
     * @brief 获取单值选项的值。
     * @param option 选项名称。
     * @param dv 如果不存在的话，返回默认值。
     * @return 选项的值。
     */
    std::string GetSingleValue(const std::string& option, const std::string& dv) const;

    /**
     * @brief 获取多值选项的值，不存在返回空列表。
     * @param option 选项名称。
     * @return 选项的值列表。
     */
    std::vector<std::string> GetMultiValue(const std::string& option) const;

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> validOptions; // 合法选项及其取值范围
    std::unordered_map<std::string, std::vector<std::string>> parsedOptions;       // 已解析的选项及其值

    /**
     * @brief 检查选项和值的合法性。
     * @param option 选项名称。
     * @param values 选项的值列表。
     */
    void ValidateOption(const std::string& option, const std::vector<std::string>& values) const;
};

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
    StrMap env;                               /**< 环境变量 */
    Str passConfig;                           /**< 配置需要使用的 passes 配置文件 */

    Options& Stage(ConStr& stage);
    Options& EnableDesugar(ConStr& enable);
    Options& EnableMacro(ConStr& enable);
    Options& FilterDecls(const StrVec& decl);
    Options& IgnoreDecls(const StrVec& decl);
    Options& IgnoreAnnotations(const StrVec& annotations);
    Options& ImportedPkgs(const StrVec& pkgs);
    Options& Passes(const StrVec& passes);
    Options& Args(StrVec&& args);
    Options& Env(StrMap&& env);
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
    void PWILines(const StrPairVec& lines);

private:
    std::unordered_map<std::string, OptionDesc> validOptions; /* key: option, value: description */
    Printer p;
    int width = 32; // 左对齐宽度
};
