#include "test_helper.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace fs = std::filesystem;

// 辅助函数：将 std::vector<std::string> 转换为 char* 数组，模拟 argv
std::vector<char*> CreateArgv(const std::vector<std::string>& args)
{
    std::vector<char*> argv;
    // 程序名，通常为 argv[0]，这里用 "program" 模拟
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    // argv 必须以 nullptr 结尾
    argv.push_back(nullptr);
    return argv;
}

// 辅助函数：从 vector<char*> 获取 argc (不包括最后的 nullptr)
int GetArgc(const std::vector<char*>& argv)
{
    return static_cast<int>(argv.size()) - 1; // 减去最后的 nullptr
}

std::string FileName(ConStr& filePath)
{
    return fs::path(filePath).stem();
}

bool CheckExist(ConStr& file)
{
    return fs::exists(file);
}

namespace {
// 工具函数：读取文件内容为字符串
std::string ReadFileToString(ConStr& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        return "";
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void RemoveFiles(ConStr& dir, const std::function<bool(const fs::path&)>& pred)
{
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (pred(entry.path())) {
            fs::remove(entry.path());
        }
    }
}
} // namespace

void RemoveFiles(ConStr& dir, ConStr& ext)
{
    RemoveFiles(dir, [&ext](auto& path) { return path.extension() == ext; });
}

bool CompareFile(ConStr& actual, ConStr& expected)
{
    std::string content0 = ReadFileToString(actual);
    std::string content1 = ReadFileToString(expected);

    // 忽略空行的比较
    auto removeEmptyLines = [](ConStr& content) {
        std::istringstream iss(content);
        std::ostringstream oss;
        std::string line;
        while (std::getline(iss, line)) {
            // 检查是否为空行（只包含空白字符）
            if (line.find_first_not_of(" \t\r\n") != std::string::npos) {
                oss << line << '\n';
            }
        }
        return oss.str();
    };

    return removeEmptyLines(content0) == removeEmptyLines(content1);
}

std::string GetEnv(ConStr& key, ConStr& defaultValue)
{
    std::string cjahPath = defaultValue;
    const char* envPath = std::getenv(key.c_str());
    if (envPath != nullptr) {
        cjahPath = std::string(envPath);
    }
    return cjahPath;
}
