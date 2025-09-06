/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "pass/PassConfig.h"

// 默认配置
PassConfig::PassConfig() : indent(4), out("."), suffix("_source.cj"), flags(0)
{
}

bool PassConfig::Desugar() const
{
    return flags & DESUGAR_FLAG;
}

bool PassConfig::Sema() const
{
    return flags & SEMA_FLAG;
}

bool PassConfig::Focus(const Decl& decl) const
{
    return (focusDecls.empty() || focusDecls.count(decl.astKind)) && !ignoreDecls.count(decl.identifier.Val());
}
