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

/// PassConfig 实现方法
PassConfig& PassConfig::Output(const std::string& out)
{
    this->out = out;
    return *this;
}

PassConfig& PassConfig::Suffix(const std::string& suffix)
{
    this->suffix = suffix;
    return *this;
}

PassConfig& PassConfig::Indent(int indent)
{
    this->indent = indent;
    return *this;
}

PassConfig& PassConfig::EnableDesugar()
{
    this->flags |= PassConfig::DESUGAR_FLAG;
    return *this;
}

PassConfig& PassConfig::EnableSema()
{
    this->flags |= PassConfig::SEMA_FLAG;
    return *this;
}

namespace {
/**
 * @brief 将字符串键映射到AstKind
 */
const std::unordered_map<std::string, AstKind> key2DeclKind{{"func", AstKind::FUNC_DECL},
    {"class", AstKind::CLASS_DECL}, {"interface", AstKind::INTERFACE_DECL}, {"struct", AstKind::STRUCT_DECL},
    {"var", AstKind::VAR_DECL}};
} // namespace

PassConfig& PassConfig::Focus(const std::unordered_set<std::string>& kinds)
{
    for (auto& kind : kinds) {
        this->focusDecls.insert(key2DeclKind.at(kind));
    }
    return *this;
}

/**
 * @brief 设置关注的注解属性。
 * @param attrs 关注的注解属性名称列表。
 */
PassConfig& PassConfig::FocusAnnotationAttrs(const std::vector<std::string>& attrs)
{
    this->focusAnnotationAttrs.insert(attrs.begin(), attrs.end());
    return *this;
}

/**
 * @brief 设置关注的修饰符属性。
 * @param attrs 关注的修饰符属性名称列表。
 */
PassConfig& PassConfig::FocusModifierAttrs(const std::vector<std::string>& attrs, const std::vector<std::string>& kinds)
{
    this->focusModifierAttrs.insert(attrs.begin(), attrs.end());
    for (auto& kind : kinds) {
        this->focusModifierWhiteList.insert(key2DeclKind.at(kind));
    }
    return *this;
}

/**
 * @brief 设置忽略的顶层声明。
 * @param decls 忽略的声明标识符列表。
 */
PassConfig& PassConfig::IgnoreDecls(const std::unordered_set<std::string>& decls)
{
    this->ignoreDecls = decls;
    return *this;
}

/**
 * @brief 设置忽略的注解。
 * @param annos 忽略的注解名称列表。
 */
PassConfig& PassConfig::IgnoreAnnotations(const std::unordered_set<std::string>& annos)
{
    this->ignoreAnnotations.insert(annos.begin(), annos.end());
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
