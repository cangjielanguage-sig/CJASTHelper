/**
 * @file
 *
 * This file declares the AstHelper.
 */
#ifndef AST_HELPER_H
#define AST_HELPER_H

#include "Printer.h"
#include "cangjie/FrontendTool/DefaultCompilerInstance.h"
#include <memory>

#ifdef NDEBUG
#define AH_ASSERT(f) static_cast<void>(f)
#else
#define AH_ASSERT(f) assert(f)
#endif
#define AH_CHECK_NULL(p) AH_ASSERT((p) != nullptr)

/**
 * 实现对Cangjie前端工具的封装
 */
class AstHelper {
public:
    explicit AstHelper(const std::vector<std::string>& args, const std::unordered_map<std::string, std::string>& env);

    ~AstHelper() = default;

    std::string GetOutputDir() const;

    void Run();

private:
    enum class SourceStage {
        DEFAULT = 0,     /**< No Source. */
        PARSE,           /**< Source of parsed ast. */
        DESUGARED_PARSE, /**< Source of desugared parsed ast. */
        SEMA,            /**< Source of typechecked ast. */
        DESUGARED_SEMA,  /**< Source of desugared typechecked ast. */
    };

private:
    void ParseArgs(const std::vector<std::string>& args);
    bool Default();
    bool Parse();
    bool DesugaredParse();
    bool Sema();
    bool DesugaredSema();

private:
    std::unique_ptr<Cangjie::SourceManager> sm;
    std::unique_ptr<Cangjie::DiagnosticEngine> diag;
    std::unique_ptr<Cangjie::CompilerInvocation> ci;
    std::unique_ptr<Cangjie::DefaultCompilerInstance> dci;
    SourceStage stage = SourceStage::DEFAULT;

    static inline const std::unordered_map<std::string, SourceStage> key2Stage{{"parse", SourceStage::PARSE},
        {"desugared-parse", SourceStage::DESUGARED_PARSE}, {"sema", SourceStage::SEMA},
        {"desugared-sema", SourceStage::DESUGARED_SEMA}};
    static inline const std::unordered_map<SourceStage, std::function<bool(AstHelper*)>> stageMap{
        {SourceStage::DEFAULT, &AstHelper::Default}, {SourceStage::PARSE, &AstHelper::Parse},
        {SourceStage::DESUGARED_PARSE, &AstHelper::DesugaredParse}, {SourceStage::SEMA, &AstHelper::Sema},
        {SourceStage::DESUGARED_SEMA, &AstHelper::DesugaredSema}};
};

#endif // AST_HELPER_H