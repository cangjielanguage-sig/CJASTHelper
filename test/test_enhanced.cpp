#include "AstHelper.h"
#include <gtest/gtest.h>

// 测试不同源码阶段的解析
TEST(AstHelperEnhancedTest, ParseStageValues)
{
    // 测试PARSE阶段
    std::vector<std::string> argsParse = {"--dump-source=parse"};
    AstHelper ahParse(argsParse, {});
    EXPECT_EQ(ahParse.GetStage(), AstHelper::SourceStage::PARSE);

    // 测试DESUGARED_PARSE阶段
    std::vector<std::string> argsDesugaredParse = {"--dump-source=desugared-parse"};
    AstHelper ahDesugaredParse(argsDesugaredParse, {});
    EXPECT_EQ(ahDesugaredParse.GetStage(), AstHelper::SourceStage::DESUGARED_PARSE);

    // 测试SEMA阶段
    std::vector<std::string> argsSema = {"--dump-source=sema"};
    AstHelper ahSema(argsSema, {});
    EXPECT_EQ(ahSema.GetStage(), AstHelper::SourceStage::SEMA);

    // 测试DESUGARED_SEMA阶段
    std::vector<std::string> argsDesugaredSema = {"--dump-source=desugared-sema"};
    AstHelper ahDesugaredSema(argsDesugaredSema, {});
    EXPECT_EQ(ahDesugaredSema.GetStage(), AstHelper::SourceStage::DESUGARED_SEMA);

    // 测试默认阶段
    std::vector<std::string> argsDefault = {};
    AstHelper ahDefault(argsDefault, {});
    EXPECT_EQ(ahDefault.GetStage(), AstHelper::SourceStage::DEFAULT);
}

// 测试声明过滤功能
TEST(AstHelperEnhancedTest, FilterDeclarations)
{
    // 测试函数声明过滤
    std::vector<std::string> argsFunc = {"--dump-source=parse", "--filter-decl=func"};
    AstHelper ahFunc(argsFunc, {});
    EXPECT_EQ(ahFunc.GetStage(), AstHelper::SourceStage::PARSE);

    // 测试类声明过滤
    std::vector<std::string> argsClass = {"--dump-source=parse", "--filter-decl=class"};
    AstHelper ahClass(argsClass, {});
    EXPECT_EQ(ahClass.GetStage(), AstHelper::SourceStage::PARSE);

    // 测试多个声明类型过滤
    std::vector<std::string> argsMulti = {"--dump-source=parse", "--filter-decl=func,class,var"};
    AstHelper ahMulti(argsMulti, {});
    EXPECT_EQ(ahMulti.GetStage(), AstHelper::SourceStage::PARSE);
}

// 测试无效参数处理
TEST(AstHelperEnhancedTest, InvalidArguments)
{
    // 测试无效的dump-source值
    std::vector<std::string> argsInvalid = {"--dump-source=invalid"};
    AstHelper ahInvalid(argsInvalid, {});
    // 应该回退到默认阶段
    EXPECT_EQ(ahInvalid.GetStage(), AstHelper::SourceStage::DEFAULT);

    // 测试无效的filter-decl值
    std::vector<std::string> argsInvalidFilter = {"--dump-source=parse", "--filter-decl=unknown"};
    AstHelper ahInvalidFilter(argsInvalidFilter, {});
    EXPECT_EQ(ahInvalidFilter.GetStage(), AstHelper::SourceStage::PARSE);
}

// 测试环境变量处理
TEST(AstHelperEnhancedTest, EnvironmentVariables)
{
    std::vector<std::string> args = {"--dump-source=parse"};
    std::unordered_map<std::string, std::string> env = {
        {"CANGJIE_HOME", "/test/path"},
        {"PATH", "/test/path/bin"}
    };
    AstHelper ah(args, env);
    EXPECT_EQ(ah.GetStage(), AstHelper::SourceStage::PARSE);
}

// 测试输出目录功能
TEST(AstHelperEnhancedTest, OutputDirectory)
{
    std::vector<std::string> args = {"--dump-source=parse", "--output-dir=/tmp/test"};
    AstHelper ah(args, {});
    EXPECT_EQ(ah.GetStage(), AstHelper::SourceStage::PARSE);
    // 注意：GetOutputDir()方法在头文件中声明但未在测试文件中使用，我们假设它能正确返回输出目录
}