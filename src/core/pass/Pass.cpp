/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "core/pass/Pass.h"
#include "utils/FileHelper.h"
#include "utils/LibraryLoader.h"

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

/// ToSourcePassConfig
bool ToSourcePassConfig::Focus(const Decl& decl) const
{
    return (focusDecls.empty() || focusDecls.count(decl.astKind)) && !ignoreDecls.count(decl.identifier.Val());
}

ToSourcePassConfig::ToSourcePassConfig() : indent(4), suffix("_source.cj")
{
}

ToSourcePassConfig& ToSourcePassConfig::Output(ConStr& out)
{
    this->out = out;
    return *this;
}

ToSourcePassConfig& ToSourcePassConfig::Suffix(ConStr& suffix)
{
    this->suffix = suffix;
    return *this;
}

ToSourcePassConfig& ToSourcePassConfig::Indent(int indent)
{
    this->indent = indent;
    return *this;
}

/**
 * @brief 设置关注的注解属性。
 * @param attrs 关注的注解属性名称列表。
 */
ToSourcePassConfig& ToSourcePassConfig::FocusAnnotationAttrs(ConStrVec& attrs)
{
    this->focusAnnotationAttrs.insert(attrs.begin(), attrs.end());
    return *this;
}

namespace {
/**
 * @brief 将字符串键映射到AstKind
 */
const StrMap<AstKind> key2DeclKind{{"func", AstKind::FUNC_DECL}, {"class", AstKind::CLASS_DECL},
    {"interface", AstKind::INTERFACE_DECL}, {"struct", AstKind::STRUCT_DECL}, {"var", AstKind::VAR_DECL}};
} // namespace

ToSourcePassConfig& ToSourcePassConfig::Focus(ConStrSet& kinds)
{
    for (auto& kind : kinds) {
        this->focusDecls.insert(key2DeclKind.at(kind));
    }
    return *this;
}

/**
 * @brief 设置关注的修饰符属性。
 * @param attrs 关注的修饰符属性名称列表。
 */
ToSourcePassConfig& ToSourcePassConfig::FocusModifierAttrs(ConStrVec& attrs, ConStrVec& kinds)
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
ToSourcePassConfig& ToSourcePassConfig::IgnoreDecls(ConStrSet& decls)
{
    this->ignoreDecls = decls;
    return *this;
}

/**
 * @brief 设置忽略的注解。
 * @param annos 忽略的注解名称列表。
 */
ToSourcePassConfig& ToSourcePassConfig::IgnoreAnnotations(ConStrSet& annos)
{
    this->ignoreAnnotations.insert(annos.begin(), annos.end());
    return *this;
}

namespace nlohmann {
template <> struct adl_serializer<PassInfo> {
    // 只实现反序列化
    static void from_json(const json& j, PassInfo& p)
    {
        j.at("group").get_to(p.group);
        j.at("names").get_to(p.names);
        j.at("lib").get_to(p.lib);
        j.at("description").get_to(p.desc);
        j.at("version").get_to(p.version);
        j.at("dependencies").get_to(p.depends);
    }
};
} // namespace nlohmann

void PassManager::Init(ConStr& path)
{
    if (!passInfoMap.empty()) {
        return;
    }
    LOGD("Init pass manager...", path);
    ConfigParser parser(path);
    auto passes = parser.Parse<Vec<PassInfo>>();
    for (auto& pass : passes) {
        LOGD("Reg Info for", pass.group);
        auto res = PassManager::passInfoMap.emplace(pass.group, pass);
        if (!res.second) {
            LOGW("Pass info already exists: ", pass.group);
        }
        for (auto& name : pass.names) {
            groupMap.emplace(name, pass.group);
        }
    }
}

void PassManager::Run(AstNode& node, ConStrVec& passes)
{
    for (auto& name : passes) {
        if (auto pass = TryGetPass(name)) {
            pass->Run(node);
        }
    }
}

bool PassManager::LoadPass(ConStr& lib)
{
    Str libname = lib;
    LOGD("Load pass: ", lib);
    Handle handle = LibraryLoader::GetInstance().LoadLib(libname);
    if (!handle) {
        LOGE("Failed to load pass from lib: ", libname);
        return false;
    }
    LOGI("Load pass from lib: ", libname, " successfully!");
    return true;
}

Pass* PassManager::TryGetPass(ConStr& name)
{
    if (auto it = passMap.find(name); it != passMap.end()) {
        return it->second.get();
    }
    if (auto it = builderMap.find(name); it != builderMap.end() && config) {
        passMap.emplace(name, (it->second)(*config));
        return passMap[name].get();
    }
    auto group = groupMap[name];
    if (auto it = passInfoMap.find(group); it != passInfoMap.end()) {
        if (LoadPass(it->second.lib)) {
            return TryGetPass(name);
        }
    }
    LOGE("Invalid pass name is not registered: " + name);
    return nullptr;
}

void PassManager::RegPassBuilder(ConStr& name, const PassBuilder& builder)
{
    if (!groupMap.count(name)) {
        LOGW("Invalid pass name is not registered: " + name);
        return;
    }
    auto group = groupMap[name];
    if (passInfoMap.count(group)) {
        builderMap.emplace(name, builder);
        LOGD("Register pass builder successfully: ", name, " group: ", group);
    } else {
        LOGW("Pass info not found: ", name, " group: ", group);
    }
}
