#include "AstHelper.h"
#include <gtest/gtest.h>

class AstHelperClassTest : public ::testing::Test {
protected:
    std::vector<std::string> args;
    AstHelper ah;

    // 构造函数：用于初始化成员变量
    AstHelperClassTest() : args({"--dump-source=parse"}), ah(args, {})
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

TEST_F(AstHelperClassTest, GetStage)
{
    EXPECT_EQ(ah.GetStage(), AstHelper::SourceStage::PARSE);
}
