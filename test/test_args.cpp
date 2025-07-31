#include "AstHelper.h"
#include <gtest/gtest.h>

// 辅助函数：将 std::vector<std::string> 转换为 char* 数组，模拟 argv
std::vector<char*> CreateArgv(const std::vector<std::string>& args)
{
    std::vector<char*> argv;
    // 程序名，通常为 argv[0]，这里用 "program" 模拟
    // argv.push_back(const_cast<char*>("program"));
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

// 测试正常情况
TEST(AstHelperTest, ParsesValidArguments)
{
    std::vector<std::string> args = {"--dump-source=parse"};
    auto argv = CreateArgv(args);
    int argc = GetArgc(argv);

    EXPECT_NO_THROW({
        auto args = ParseArgs(argc, argv.data());
        EXPECT_EQ(args.size(), 1);
    });
}

TEST(AstHelperTest, ParsesValidEnv)
{
    std::vector<std::string> args = {
        "PATH=c:\\a\\b\\c;D:\\dev\\cangjie",
        "LD_LIBRARY_PATH=yyy",
        "CANGJIE_HOME=D:\\dev\\cangjie",
    };
    auto argv = CreateArgv(args);

    EXPECT_NO_THROW({
        auto env = ParseEnv(argv.data(), {"CANGJIE_HOME", "PATH"});
        EXPECT_EQ(env.size(), 2);
    });
}

TEST(AstHelperTest, HandleArgs)
{
    AstHelper ah({}, {});
    EXPECT_EQ(ah.GetStage(), AstHelper::SourceStage::DEFAULT); // 检查正数相加是否正确
}
