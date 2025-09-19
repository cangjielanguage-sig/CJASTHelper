/**
 * @file
 *
 * This file declares the wrapper ast nodes.
 */

#pragma once

#include "utils/TypeAlias.h"

#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Sema/Desugar.h"

using Cangjie::CompilerInvocation;

using PkgPtrVec = std::vector<Ptr<Package>>;

class CangjieFrontendHelper {
public:
    /**
     * Create a compiler instance.
     * @param args command line arguments
     * @param env environment variables
     */
    CangjieFrontendHelper(StrVec&& args, StrMap&& env);
    virtual ~CangjieFrontendHelper() = default;

    bool Parse();
    bool DesugaredParse();
    bool ImportPackage();
    bool Sema();
    bool DesugaredSema();

    Str GetOutDir() const;

    PkgPtrVec GetSourcePackages();
    PkgPtrVec GetImportedPackages();

private:
    CompilerInvocation& ParseArgs(StrVec&& args, StrMap&& env);

private:
    Cangjie::DiagnosticEngine diag; /**< 诊断引擎实例 */
    CompilerInvocation ci;          /**< 编译器调用实例 */
    Cangjie::CompilerInstance mci;  /**< 编译器实例 */
};
