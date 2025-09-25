/**
 * @file
 *
 * This file declares the wrapper ast nodes.
 */

#pragma once

#include "utils/types/TypeAlias.h"

#include "cangjie/Frontend/CompilerInstance.h"
#include "cangjie/Sema/Desugar.h"

using Cangjie::CompilerInvocation;

using PkgPtrVec = Vec<Ptr<Package>>;

class CangjieFrontendHelper {
public:
    /**
     * Create a compiler instance.
     * @param args command line arguments
     * @param env environment variables
     */
    CangjieFrontendHelper(StrVec&& args, StrMap<Str>&& env);
    virtual ~CangjieFrontendHelper() = default;

    bool Parse();
    bool DesugaredParse();
    bool ImportPackage();
    bool MacroExpand();
    bool Sema();
    bool DesugaredSema();

    Str GetOutDir() const;

    PkgPtrVec GetSourcePackages();
    PkgPtrVec GetImportedPackages();

private:
    CompilerInvocation& ParseArgs(StrVec&& args, StrMap<Str>&& env);

private:
    Cangjie::DiagnosticEngine diag; /**< 诊断引擎实例 */
    CompilerInvocation ci;          /**< 编译器调用实例 */
    Cangjie::CompilerInstance mci;  /**< 编译器实例 */
};
