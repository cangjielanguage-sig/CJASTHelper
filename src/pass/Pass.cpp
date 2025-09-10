/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "pass/Pass.h"

/// PassConfig 实现方法
bool PassConfig::Desugar() const
{
    return flags & DESUGAR_FLAG;
}

bool PassConfig::Sema() const
{
    return flags & SEMA_FLAG;
}

PassConfig& PassConfig::EnableDesugar()
{
    this->flags |= DESUGAR_FLAG;
    return *this;
}

PassConfig& PassConfig::EnableSema()
{
    this->flags |= SEMA_FLAG;
    return *this;
}

void PassManager::Run(AstNode& node, const std::vector<std::string>& passes)
{
    for (auto& name : passes) {
        if (auto pass = TryGetPass(name)) {
            pass->Run(node);
        }
    }
}

Pass* PassManager::TryGetPass(const std::string& name)
{
    if (auto it = passMap.find(name); it != passMap.end()) {
        return it->second.get();
    }
    if (auto it = passBuilderMap.find(name); it != passBuilderMap.end() && config) {
        passMap.emplace(name, it->second(*config));
        return passMap[name].get();
    }
    return nullptr;
}

void PassManager::RegPassBuilder(const std::string& name, const PassBuilder& builder)
{
    passBuilderMap.emplace(name, builder);
}
