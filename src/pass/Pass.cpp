/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "pass/Pass.h"

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

/**
 * 注册一个分析pass
 */
void PassManager::RegisterPass(std::string name, std::unique_ptr<Pass> pass)
{
    passMap.emplace(name, std::move(pass));
}