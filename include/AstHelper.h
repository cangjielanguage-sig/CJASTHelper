/**
 * @file
 *
 * This file declares the AstHelper.
 */
#ifndef AST_HELPER_H
#define AST_HELPER_H

#include "Printer.h"
#include "cangjie/Frontend/CompilerInstance.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>

/**
 * 封装了对Cangjie前端工具的操作
 */
class AstHelper {
public:
    /**
     * @enum SourceStage
     * 表示源代码处理的不同阶段
     */
    enum class SourceStage {
        DEFAULT = 0,     /**< No Source. */
        PARSE,           /**< Source of parsed ast. */
        DESUGARED_PARSE, /**< Source of desugared parsed ast. */
        SEMA,            /**< Source of typechecked ast. */
        DESUGARED_SEMA,  /**< Source of desugared typechecked ast. */
    };

    /**
     * @brief helper 自定义选项定义
     */
    struct Options {
        SourceStage stage = SourceStage::DEFAULT;   /**< 当前的源代码阶段 */
        bool enableDesugar = false;                 /**< 是否启用语法糖打印 */
        std::vector<std::string> filterDecls;       /**< 过滤打印decl配置 */
        std::vector<std::string> ignoreDecls;       /**< 忽略打印decl配置 */
        std::vector<std::string> ignoreAnnotations; /**< 忽略打印annotations配置 */
    };

public:
    /**
     * @brief 构造AstHelper实例
     * @param args 命令行参数的向量
     * @param env 环境变量的映射
     */
    explicit AstHelper(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env);

    /**
     * @brief 默认析构函数
     */
    ~AstHelper() = default;

    /**
     * @brief 获取输出目录
     * @return 表示输出目录的字符串
     */
    std::string GetOutputDir() const;

    /**
     * @brief 根据当前配置执行相应的阶段
     */
    void Run();

    /**
     * @brief 获取当前的源代码阶段
     * @return 当前的源代码阶段
     */
    SourceStage GetStage() const
    {
        return options.stage;
    }

protected:
    /**
     * @brief 执行分析阶段 复用前端的编译器调用，得到AST
     * @return 分析阶段执行成功返回true，否则返回false
     */
    bool DoAnalysis();
    /**
     * @brief 执行转换阶段, 获取AST并转换为源代码
     * @return 转换阶段执行成功返回true，否则返回false
     */
    bool DoTransform() const;

private:
    /**
     * @brief 解析提供的命令行参数
     * @param args 命令行参数的向量
     */
    void ParseArgs(const std::vector<std::string>& args);

    /**
     * @brief 执行默认阶段
     * @return 如果成功返回true，否则返回false
     */
    bool Default();

    /**
     * @brief 执行解析阶段
     * @return 如果成功返回true，否则返回false
     */
    bool Parse();

    /**
     * @brief 执行语法糖解析阶段
     * @return 如果成功返回true，否则返回false
     */
    bool DesugaredParse();

    /**
     * @brief 执行语义分析阶段
     * @return 如果成功返回true，否则返回false
     */
    bool Sema();

    /**
     * @brief 执行语法糖处理后的语义分析阶段
     * @return 如果成功返回true，否则返回false
     */
    bool DesugaredSema();

private:
    using DiagnosticEngine = Cangjie::DiagnosticEngine;
    using CompilerInvocation = Cangjie::CompilerInvocation;
    using CompilerInstance = Cangjie::CompilerInstance;
    DiagnosticEngine diag;                 /**< 诊断引擎实例 */
    CompilerInvocation ci;                 /**< 编译器调用实例 */
    std::unique_ptr<CompilerInstance> mci; /**< 编译器实例的智能指针 */

    Options options; /**< 用户选项 */
    /**
     * @brief 将SourceStage值映射到对应的执行函数
     */
    static inline const std::unordered_map<SourceStage, std::function<bool(AstHelper*)>> stageMap{
        {SourceStage::DEFAULT, &AstHelper::Default}, {SourceStage::PARSE, &AstHelper::Parse},
        {SourceStage::DESUGARED_PARSE, &AstHelper::DesugaredParse}, {SourceStage::SEMA, &AstHelper::Sema},
        {SourceStage::DESUGARED_SEMA, &AstHelper::DesugaredSema}};
};

/**
 * @brief 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数字符串数组
 * @return 解析后的参数向量
 */
std::vector<std::string> ParseArgs(int argc, const char* const* argv);

/**
 * @brief 解析环境变量
 * @param envp 环境变量字符串数组
 * @param focus 需要关注的键集合
 * @return 解析后的环境变量映射
 */
std::unordered_map<std::string, std::string> ParseEnv(
    const char* const* envp, const std::unordered_set<std::string>& focus);

#endif // AST_HELPER_H