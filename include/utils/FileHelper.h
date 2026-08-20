#pragma once
#include "utils/types/TypeAlias.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>

std::filesystem::path getExecutablePath();

bool CheckExist(ConStr& file);
Str FileName(ConStr& filePath);
void CreateDirIfNotExists(ConStr& path);
Str FindPath(ConStr& name, ConStrVec& paths);

/**
 * @brief 递归遍历目录, 按包分组: 每个直接包含 .cj 文件的目录视为一个包
 * 返回 包目录 -> 该目录下直接包含的 .cj 文件列表(已按字典序排序);
 * 键为目录绝对/相对路径(与输入写法一致的分隔符), 按 std::map 键序(包序确定).
 * @param dir 根目录
 * @return 包分组; 目录不存在或没有 .cj 文件时为空 map
 */
std::map<Str, StrVec> GroupCjFilesByDir(ConStr& dir);

class ConfigParser {
public:
    ConfigParser(ConStr& name);
    virtual ~ConfigParser() = default;

    /**
     * 从文件加载 JSON 并解析为指定类型 T
     * @tparam T 目标类型（需支持 from_json）
     * @return 解析后的对象
     * @throws std::runtime_error 如果文件不存在或解析失败
     */
    template <typename T> T Parse()
    {
        using nlohmann::json;
        json j;
        try {
            fs >> j;
        } catch (const json::parse_error& e) {
            throw std::runtime_error("JSON parse error in " + path + ": " + e.what());
        }
        try {
            return j.get<T>();
        } catch (const json::type_error& e) {
            throw std::runtime_error("Type conversion error in " + path + ": " + e.what());
        }
    }

private:
    Str path;
    std::ifstream fs;
    static inline ConStrVec searchPaths{"config", "../config", "../../config"};
};
