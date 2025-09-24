/**
 * @file
 *
 * This file implements the ToJavaPass.
 */
#include "ToSourcePass.h"
#include "utils/Cast.h"
#include "utils/FileHelper.h"
#include "utils/Logger.h"

ToSourcePass::ToSourcePass(const ToSourcePassConfig& config) : Pass(config), prt(ofs, Config().indent)
{
}

void ToSourcePass::Run(AstNode& node)
{
    CreateDirIfNotExists(Config().out);
    (void)Traverse(node, visitor);
}

Printer& ToSourcePass::PRT()
{
    return prt;
}

const ToSourcePassConfig& ToSourcePass::Config() const
{
    return Cast<const ToSourcePassConfig&>(config);
}
