/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "core/pass/Pass.h"
#include "utils/LibraryLoader.h"
#include <fstream>
#include <nlohmann/json.hpp>

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

using json = nlohmann::json;

void from_json(const json& j, PassInfo& p)
{
    j.at("name").get_to(p.name);
    j.at("lib").get_to(p.lib);
    j.at("description").get_to(p.desc);
    j.at("version").get_to(p.version);
    j.at("dependencies").get_to(p.depends);
}

void PassManager::Init(ConStr& path)
{
    if (!passInfoMap.empty()) {
        return;
    }
    LOGD("Init pass manager...", path);
    std::fstream fs(path);
    // 打开 JSON 文件
    if (!fs.is_open()) {
        LOGE("Failed to open file: ", path);
        throw std::logic_error("Failed to open file: " + path);
    }

    // 读取整个文件到 json 对象
    json j;
    try {
        fs >> j;
    } catch (const json::parse_error& e) {
        throw std::logic_error("Parse json file error: " + path);
    }

    Vec<PassInfo> passes = j.get<Vec<PassInfo>>();
    for (auto& pass : passes) {
        LOGD("Reg Info for", pass.name);
        auto res = PassManager::passInfoMap.emplace(pass.name, pass);
        if (!res.second) {
            LOGW("Pass info already exists: ", pass.name);
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
    Handle handle = LibraryLoader::GetInstance().LoadLibrary(libname);
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
    if (auto it = passInfoMap.find(name); it != passInfoMap.end() && config) {
        if (!it->second.builder) {
            if (!LoadPass(it->second.lib)) {
                return nullptr;
            }
        }
        passMap.emplace(name, passInfoMap[name].builder(*config));
        return passMap[name].get();
    }
    return nullptr;
}

void PassManager::RegPassBuilder(ConStr& name, const PassBuilder& builder)
{
    if (auto it = passInfoMap.find(name); it != passInfoMap.end()) {
        it->second.builder = builder;
    } else {
        LOGW("Pass info not found: ", name);
    }
}
