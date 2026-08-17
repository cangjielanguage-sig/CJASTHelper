#include "core/ArgHelper.h"
#include "test_helper.h"
#include "utils/FileHelper.h"
#include <algorithm>
#include <gtest/gtest.h>

// --check-syntax 目录模式: 递归收集 .cj 文件
TEST(CheckSyntaxDirTest, CollectCjFilesRecursive)
{
    auto files = CollectCjFiles("test/data");
    ASSERT_GE(files.size(), 5);
    // 只收集 .cj 文件, 排除 .txt 等其他文件
    for (auto& f : files) {
        EXPECT_TRUE(f.ends_with(".cj")) << "non-cj file collected: " << f;
    }
    EXPECT_TRUE(std::none_of(files.begin(), files.end(), [](ConStr& f) { return f.find(".txt") != Str::npos; }));
    // 能收集到坏语法文件(用于验证检查确实覆盖到)
    EXPECT_TRUE(std::any_of(files.begin(), files.end(),
        [](ConStr& f) { return f.find("bad_syntax.cj") != Str::npos; }));
    // 结果按字典序排序
    EXPECT_TRUE(std::is_sorted(files.begin(), files.end()));
}

TEST(CheckSyntaxDirTest, CollectCjFilesMissingDir)
{
    auto files = CollectCjFiles("test/data/not_exist_dir");
    EXPECT_TRUE(files.empty());
}

// check-syntax + 目录: 目录被展开为 .cj 文件列表, argv0 保留在最前
TEST(CheckSyntaxDirTest, CheckSyntaxExpandsDir)
{
    ArgHelper argh;
    StrVec args{"cjah", "--check-syntax=true", "test/data/inputs"};
    StrVec envp{};
    auto argv = CreateArgv(args);
    auto envv = CreateArgv(envp);
    auto opts = argh.ParseArgs(GetArgc(argv), argv.data(), envv.data());
    ASSERT_EQ(opts.size(), 1);
    ASSERT_TRUE(opts[0].checkSyntax);
    ASSERT_EQ(opts[0].stage, SourceStage::PARSE);
    ASSERT_GE(opts[0].args.size(), 2);
    for (size_t i = 1; i < opts[0].args.size(); ++i) {
        EXPECT_TRUE(opts[0].args[i].ends_with(".cj")) << "unexpected arg: " << opts[0].args[i];
    }
}

// 回归: check-syntax + 单文件, 参数原样保留, 不做目录展开
TEST(CheckSyntaxDirTest, CheckSyntaxSingleFileUnchanged)
{
    ArgHelper argh;
    StrVec args{"cjah", "--check-syntax=true", "test/data/inputs/main.cj"};
    StrVec envp{};
    auto argv = CreateArgv(args);
    auto envv = CreateArgv(envp);
    auto opts = argh.ParseArgs(GetArgc(argv), argv.data(), envv.data());
    ASSERT_EQ(opts.size(), 1);
    ASSERT_TRUE(opts[0].checkSyntax);
    ASSERT_EQ(opts[0].args.size(), 2);
    EXPECT_EQ(opts[0].args[1], "test/data/inputs/main.cj");
}

// 回归: 非 check-syntax 模式不做目录展开
TEST(CheckSyntaxDirTest, NonCheckSyntaxKeepsDirArg)
{
    ArgHelper argh;
    StrVec args{"cjah", "--dump-source=parse", "test/data/inputs"};
    StrVec envp{};
    auto argv = CreateArgv(args);
    auto envv = CreateArgv(envp);
    auto opts = argh.ParseArgs(GetArgc(argv), argv.data(), envv.data());
    ASSERT_EQ(opts.size(), 1);
    ASSERT_EQ(opts[0].args.size(), 2);
    EXPECT_EQ(opts[0].args[1], "test/data/inputs");
}