#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
// 工具函数：读取文件内容为字符串
std::string ReadFileToString(const std::string& filename)
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

void ExecDumpDesugaredSema(const std::string& cjahPath, const std::string& src, const std::string& out)
{
    // 构建命令行字符串以运行你的应用
    std::string command =
        cjahPath + " --dump-source=desugared-sema " + src + " -Woff unused --output-type=dylib --output-dir " + out;
    // 使用 popen 执行命令并捕获输出
    ExecCmd(command.c_str());
}

inline bool CheckExists(const std::vector<std::string>& files)
{
    return std::all_of(files.begin(), files.end(), [](const std::string& file) { return fs::exists(file); });
}

inline bool RemoveFiles(const std::vector<std::string>& files)
{
    return std::all_of(files.begin(), files.end(), [](const std::string& file) { return fs::remove(file); });
}

inline bool CompareFile(const std::string& actual, const std::string& expected)
{
    std::string content0 = ReadFileToString(actual);
    std::string content1 = ReadFileToString(expected);
    return content0 == content1;
}

inline std::string GetCJAH()
{
    std::string cjahPath = "build/bin/cjah";
    const char* envPath = std::getenv("CJAH");
    if (envPath != nullptr) {
        cjahPath = std::string(envPath);
    }
    return cjahPath;
}

inline std::string GetFileNameWithoutSuffix(const std::string& fname)
{
    size_t start = fname.find_last_of('/');
    size_t end = fname.find_last_of('.');
    if (start != std::string::npos) {
        if (end != std::string::npos) {
            return fname.substr(start + 1, end - start - 1);
        }
        return fname.substr(0, end);
    }
    return fname;
}

inline std::vector<std::string> IterateCjah(const std::string& cjahPath, const std::string& src, const std::string& out)
{
    std::string fileName = GetFileNameWithoutSuffix(src);
    std::string outPre = out + "/" + fileName;
    std::string suffix = "_source";
    std::string outFile0 = outPre + suffix + ".cj";
    std::string outFile1 = outPre + suffix + suffix + ".cj";
    std::string expectedFile = outPre + suffix + suffix + suffix + ".cj";
    // cjah 测试源文件 经过 3 次迭代 源码维持不变
    ExecDumpDesugaredSema(cjahPath, src, out);
    ExecDumpDesugaredSema(cjahPath, outFile0, out);
    ExecDumpDesugaredSema(cjahPath, outFile1, out);
    return {outFile0, outFile1, expectedFile};
}
} // namespace

TEST(CJAHTest, GetFileNameWithoutSuffix)
{
    EXPECT_EQ(GetFileNameWithoutSuffix("test/main.cj"), "main");
}

// Continuous Integration Tests
TEST(CJAHTest, Integration01)
{
    std::string cjahPath = GetCJAH();
    std::string out = ".";
    std::string src = "test/main.cj";
    auto tmpFiles = IterateCjah(cjahPath, src, out);
    // 检查输出文件是否存在
    EXPECT_EQ(tmpFiles.size(), 3);
    EXPECT_TRUE(CheckExists(tmpFiles)) << "Output file not found.";
    EXPECT_TRUE(CompareFile(tmpFiles[1], tmpFiles[2])) << "Actual output does not match expected.";
    EXPECT_TRUE(RemoveFiles(tmpFiles)) << "Remove tmpFiles Failed!.";
}
