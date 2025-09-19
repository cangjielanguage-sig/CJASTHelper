/**
 * @file
 *
 * This file implements the ToSourcePass.
 */
#include "core/pass/Pass.h"
#include "utils/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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

ToSourcePassConfig& ToSourcePassConfig::Output(const std::string& out)
{
    this->out = out;
    return *this;
}

ToSourcePassConfig& ToSourcePassConfig::Suffix(const std::string& suffix)
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
ToSourcePassConfig& ToSourcePassConfig::FocusAnnotationAttrs(const std::vector<std::string>& attrs)
{
    this->focusAnnotationAttrs.insert(attrs.begin(), attrs.end());
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

ToSourcePassConfig& ToSourcePassConfig::Focus(const std::unordered_set<std::string>& kinds)
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
ToSourcePassConfig& ToSourcePassConfig::FocusModifierAttrs(
    const std::vector<std::string>& attrs, const std::vector<std::string>& kinds)
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
ToSourcePassConfig& ToSourcePassConfig::IgnoreDecls(const std::unordered_set<std::string>& decls)
{
    this->ignoreDecls = decls;
    return *this;
}

/**
 * @brief 设置忽略的注解。
 * @param annos 忽略的注解名称列表。
 */
ToSourcePassConfig& ToSourcePassConfig::IgnoreAnnotations(const std::unordered_set<std::string>& annos)
{
    this->ignoreAnnotations.insert(annos.begin(), annos.end());
    return *this;
}

using json = nlohmann::json;

void from_json(const json& j, PassInfo& p)
{
    j.at("name").get_to(p.name);
    j.at("path").get_to(p.path);
    j.at("description").get_to(p.desc);
    j.at("version").get_to(p.version);
    j.at("dependencies").get_to(p.depends);
}

void PassManager::Init(ConStr& path)
{
    DEBUG("Init pass manager...", path);
    std::fstream fs(path);
    // 打开 JSON 文件
    if (!fs.is_open()) {
        ERROR("Failed to open file: ", path);
        throw std::logic_error("Failed to open file: " + path);
    }

    // 读取整个文件到 json 对象
    json j;
    try {
        fs >> j;
    } catch (const json::parse_error& e) {
        throw std::logic_error("Parse json file error: " + path);
    }

    std::vector<PassInfo> passes = j.get<std::vector<PassInfo>>();
    for (auto& pass : passes) {
        DEBUG("Reg Info for", pass.name);
        auto res = PassManager::passInfoMap.emplace(pass.name, pass);
        if (!res.second) {
            WARN("Pass info already exists: ", pass.name);
        }
    }
}

void PassManager::Run(AstNode& node, const std::vector<std::string>& passes)
{
    for (auto& name : passes) {
        if (auto pass = TryGetPass(name)) {
            pass->Run(node);
        }
    }
}

bool PassManager::LoadPass(const std::string& path)
{
    DEBUG("Load pass: ", path);
#ifdef _WIN32
    HMODULE handle = LoadLibrary(path.c_str());
#else
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
#endif
    if (!handle) {
        ERROR("Failed to load pass: ", path);
        return false;
    }
    INFO("Load pass: ", path, " successfully!");
    return true;
}

Pass* PassManager::TryGetPass(const std::string& name)
{
    if (auto it = passMap.find(name); it != passMap.end()) {
        return it->second.get();
    }
    if (auto it = passInfoMap.find(name); it != passInfoMap.end() && config) {
        if (!it->second.builder) {
            if (!LoadPass(it->second.path)) {
                return nullptr;
            }
        }
        passMap.emplace(name, passInfoMap[name].builder(*config));
        return passMap[name].get();
    }
    return nullptr;
}

void PassManager::RegPassBuilder(const std::string& name, const PassBuilder& builder)
{
    if (auto it = passInfoMap.find(name); it != passInfoMap.end()) {
        it->second.builder = builder;
    } else {
        WARN("Pass info not found: ", name);
    }
}
