#include "core/AstHelper.h"
#include "test_helper.h"
#include <gtest/gtest.h>

class AstHelperTest : public ::testing::Test {
protected:
    std::vector<std::string> args{"--dump-source=parse"};
    std::vector<char*> rawArgs;
    ArgHelper argHelper;
    AstHelper astHelper;

    // 构造函数：用于初始化成员变量
    AstHelperTest()
        : rawArgs(CreateArgv(args)), astHelper(argHelper.ParseArgs(GetArgc(rawArgs), rawArgs.data(), nullptr))
    {
    }

    void SetUp() override
    {
        // 在每个测试开始前运行的设置代码
    }

    void TearDown() override
    {
        // 在每个测试结束后运行的清理代码
    }
};

TEST_F(AstHelperTest, ParseArgs01)
{
    std::vector<std::string> args = {"--dump-source=parse"};
    auto argv = CreateArgv(args);
    int argc = GetArgc(argv);

    std::vector<std::string> env = {
        "PATH=c:\\a\\b\\c;D:\\dev\\cangjie",
        "LD_LIBRARY_PATH=yyy",
        "CANGJIE_HOME=D:\\dev\\cangjie",
    };
    auto envp = CreateArgv(env);

    EXPECT_NO_THROW({
        auto options = argHelper.ParseArgs(argc, argv.data(), envp.data());
        EXPECT_EQ(options.stage, SourceStage::PARSE);
        EXPECT_EQ(options.passes.size(), 1);
        EXPECT_EQ(options.passes[0], "to-source");
    });
}

TEST_F(AstHelperTest, ParseArgs02)
{
    std::vector<std::string> args = {"--help --dump-source=parse"};
    auto argv = CreateArgv(args);
    int argc = GetArgc(argv);

    EXPECT_NO_THROW({
        auto options = argHelper.ParseArgs(argc, argv.data(), nullptr);
        EXPECT_EQ(options.stage, SourceStage::DEFAULT);
        argHelper.ShowHelperInfo();
    });
}
