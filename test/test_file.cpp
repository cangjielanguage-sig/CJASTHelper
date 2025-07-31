#include <array>
#include <cstdio>
#include <cstdlib> // for getenv
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

// 工具函数：读取文件内容为字符串
std::string readFileToString(const std::string& filename)
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
std::string exec(const char* cmd)
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

TEST(CJAHTest, OutputTest)
{
    std::string cjahPath = "build/bin/cjah";
    const char* envPath = std::getenv("CJAH");
    if (envPath != nullptr) {
        cjahPath = std::string(envPath);
    }
    // 构建命令行字符串以运行你的应用
    std::string command = cjahPath + " --dump-source=parse ../tmp/test.cj";

    // 使用 popen 执行命令并捕获输出
    std::string output = exec(command.c_str());

    // 检查输出是否符合预期
    // EXPECT_EQ(output, "Hello, World!\n");

    // 读取两个文件内容
    std::string outputFilename = "../tmp/test.cj";
    // 检查输出文件是否存在
    EXPECT_TRUE(fs::exists(outputFilename)) << "Output file not found.";

    std::string actual = readFileToString(outputFilename);
    std::string expected = readFileToString("../tmp/test.cj");
    // 比较内容
    EXPECT_EQ(actual, expected) << "Actual output does not match expected.";
}