#pragma once
#include "utils/types/TypeAlias.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

std::filesystem::path getExecutablePath();

bool CheckExist(ConStr& file);
Str FileName(ConStr& filePath);
void CreateDirIfNotExists(ConStr& path);

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
    Str ValidatePath(ConStr& path);
    Str path;
    std::ifstream fs;
    static inline ConStrVec searchPaths{"config", "../config", "../../config"};
};
