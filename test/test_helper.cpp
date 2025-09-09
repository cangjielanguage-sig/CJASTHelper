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

// Helper function to read from a FILE* into a string
std::string ExecCmd(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void ExecDump(ConStr& cjahPath, ConStr& stage, ConStr& src, ConStr& out, bool desugar)
{
    // 构建命令行字符串以运行你的应用
    std::string command =
        cjahPath + " --dump-source=" + stage + " " + src + " -Woff unused --output-type=dylib --output-dir " + out;
    if (desugar) {
        command += " --enable-desugar=true";
    } else {
        command += " --enable-desugar=false";
    }
    // 使用 popen 执行命令并捕获输出
    ExecCmd(command.c_str());
}

bool CheckExist(ConStr& file)
{
    return fs::exists(file);
}

bool CheckExists(const std::vector<std::string>& files)
{
    return std::all_of(files.begin(), files.end(), [](ConStr& file) { return fs::exists(file); });
}

bool RemoveFiles(const std::vector<std::string>& files)
{
    return std::all_of(files.begin(), files.end(), [](ConStr& file) { return fs::remove(file); });
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

std::string GetCJAH()
{
    std::string cjahPath = "build/bin/cjah";
    const char* envPath = std::getenv("CJAH");
    if (envPath != nullptr) {
        cjahPath = std::string(envPath);
    }
    return cjahPath;
}

std::string GetCJHome()
{
    const char* cjHome = std::getenv("CANGJIE_HOME");
    return std::string(cjHome);
}

std::string GetFileNameWithoutExtension(ConStr& fname)
{
    return fs::path(fname).stem();
}

std::vector<std::string> IterateCjah(ConStr& cjahPath, ConStr& src, ConStr& out)
{
    std::string fileName = GetFileNameWithoutExtension(src);
    std::string outPre = out + "/" + fileName;
    std::string suffix = "_source";
    std::string outFile0 = outPre + suffix + ".cj";
    std::string outFile1 = outPre + suffix + suffix + ".cj";
    std::string expectedFile = outPre + suffix + suffix + suffix + ".cj";
    const std::string stage = "desugared-sema";
    // cjah 测试源文件 经过 3 次迭代 源码维持不变
    ExecDump(cjahPath, stage, src, out, true);
    ExecDump(cjahPath, stage, outFile0, out, true);
    ExecDump(cjahPath, stage, outFile1, out, true);
    return {outFile0, outFile1, expectedFile};
}

void RemoveFiles(ConStr& dir, const std::function<bool(const fs::path&)>& pred)
{
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (pred(entry.path())) {
            fs::remove(entry.path());
        }
    }
}

void RemoveFiles(ConStr& dir, ConStr& ext)
{
    RemoveFiles(dir, [&ext](auto& path) { return path.extension() == ext; });
}
