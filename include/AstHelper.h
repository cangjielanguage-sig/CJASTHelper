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
 * 实现对Cangjie前端工具的封装
 */
class AstHelper {
public:
    enum class SourceStage {
        DEFAULT = 0,     /**< No Source. */
        PARSE,           /**< Source of parsed ast. */
        DESUGARED_PARSE, /**< Source of desugared parsed ast. */
        SEMA,            /**< Source of typechecked ast. */
        DESUGARED_SEMA,  /**< Source of desugared typechecked ast. */
    };

public:
    explicit AstHelper(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env);

    ~AstHelper() = default;

    std::string GetOutputDir() const;

    void Run();

    SourceStage GetStage() const
    {
        return stage;
    }

private:
    void ParseArgs(const std::vector<std::string>& args);
    bool Default();
    bool Parse();
    bool DesugaredParse();
    bool Sema();
    bool DesugaredSema();

private:
    using DiagnosticEngine = Cangjie::DiagnosticEngine;
    using CompilerInvocation = Cangjie::CompilerInvocation;
    using CompilerInstance = Cangjie::CompilerInstance;
    DiagnosticEngine diag;
    CompilerInvocation ci;
    std::unique_ptr<CompilerInstance> mci;

    SourceStage stage = SourceStage::DEFAULT;

    static inline const std::unordered_map<std::string, SourceStage> key2Stage{{"parse", SourceStage::PARSE},
        {"desugared-parse", SourceStage::DESUGARED_PARSE}, {"sema", SourceStage::SEMA},
        {"desugared-sema", SourceStage::DESUGARED_SEMA}};
    static inline const std::unordered_map<SourceStage, std::function<bool(AstHelper*)>> stageMap{
        {SourceStage::DEFAULT, &AstHelper::Default}, {SourceStage::PARSE, &AstHelper::Parse},
        {SourceStage::DESUGARED_PARSE, &AstHelper::DesugaredParse}, {SourceStage::SEMA, &AstHelper::Sema},
        {SourceStage::DESUGARED_SEMA, &AstHelper::DesugaredSema}};
};

std::vector<std::string> ParseArgs(int argc, const char* const* argv);
std::unordered_map<std::string, std::string> ParseEnv(
    const char* const* envp, const std::unordered_set<std::string>& focus);
#endif // AST_HELPER_H