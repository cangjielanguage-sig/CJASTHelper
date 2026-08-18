#include "core/ArgHelper.h"
#include "test_helper.h"
#include "utils/FileHelper.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace {
// Windows 路径分隔符归一化, 便于断言
Str NormPath(ConStr& p)
{
    Str r = p;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
}
} // namespace

// 按包分组: 每个直接包含 .cj 文件的目录一个包, 遍历递归
TEST(CheckSyntaxDirTest, GroupCjFilesByDirMultiPackage)
{
    auto groups = GroupCjFilesByDir("test/data");
    // 期望至少两个包: inputs/ 与 expected/desugar/
    ASSERT_GE(groups.size(), 2);
    // 键为平台分隔符, 归一化后查找
    auto findPkg = [&groups](ConStr& name) {
        for (auto& [pkgDir, files] : groups) {
            if (NormPath(pkgDir) == name) {
                return &files;
            }
        }
        return (StrVec*)nullptr;
    };
    auto* inputs = findPkg("test/data/inputs");
    ASSERT_NE(inputs, nullptr) << "inputs 包缺失";
    for (auto& f : *inputs) {
        EXPECT_TRUE(f.ends_with(".cj"));
        EXPECT_TRUE(NormPath(f).find("inputs") != Str::npos);
    }
    auto* desugar = findPkg("test/data/expected/desugar");
    ASSERT_NE(desugar, nullptr) << "expected/desugar 包缺失";
    EXPECT_EQ(desugar->size(), 6); // 4 stages x 2 desugar 开关的 golden
    // 每个包内文件按字典序排序
    for (auto& [pkgDir, files] : groups) {
        EXPECT_TRUE(std::is_sorted(files.begin(), files.end()));
    }
}

TEST(CheckSyntaxDirTest, GroupCjFilesByDirMissingDir)
{
    auto groups = GroupCjFilesByDir("test/data/not_exist_dir");
    EXPECT_TRUE(groups.empty());
}

// check-syntax + 目录(多包): 每个包一个 Options, 各自只含本包文件
TEST(CheckSyntaxDirTest, CheckSyntaxSplitsPackages)
{
    ArgHelper argh;
    StrVec args{"cjah", "--check-syntax=true", "test/data"};
    StrVec envp{};
    auto argv = CreateArgv(args);
    auto envv = CreateArgv(envp);
    auto opts = argh.ParseArgs(GetArgc(argv), argv.data(), envv.data());
    ASSERT_GE(opts.size(), 2);
    for (auto& o : opts) {
        ASSERT_TRUE(o.checkSyntax);
        ASSERT_EQ(o.stage, SourceStage::PARSE);
        ASSERT_EQ(o.args[0], "cjah"); // argv0 保留
        for (size_t i = 1; i < o.args.size(); ++i) {
            EXPECT_TRUE(o.args[i].ends_with(".cj"));
        }
    }
    // bad_syntax.cj 只应出现在其中一个包(inputs 包)中
    int withBad = 0;
    for (auto& o : opts) {
        for (size_t i = 1; i < o.args.size(); ++i) {
            if (o.args[i].find("bad_syntax.cj") != Str::npos) {
                ++withBad;
            }
        }
    }
    EXPECT_EQ(withBad, 1);
    // 同一包内所有文件父目录一致(即每个包的文件来自同一目录)
    for (auto& o : opts) {
        Str parent = NormPath(o.args[1]).substr(0, NormPath(o.args[1]).rfind('/'));
        for (size_t i = 2; i < o.args.size(); ++i) {
            auto p2 = NormPath(o.args[i]);
            EXPECT_EQ(parent, p2.substr(0, p2.rfind('/'))) << "跨包文件混入: " << o.args[i];
        }
    }
}

// 透传参数(cjc 选项)出现在每个包的 args 中
TEST(CheckSyntaxDirTest, PassthroughArgsCopiedToEveryPackage)
{
    ArgHelper argh;
    StrVec args{"cjah", "--check-syntax=true", "test/data", "--diagnostic-format=json"};
    StrVec envp{};
    auto argv = CreateArgv(args);
    auto envv = CreateArgv(envp);
    auto opts = argh.ParseArgs(GetArgc(argv), argv.data(), envv.data());
    ASSERT_GE(opts.size(), 2);
    for (auto& o : opts) {
        EXPECT_TRUE(std::any_of(
            o.args.begin(), o.args.end(), [](ConStr& a) { return a == "--diagnostic-format=json"; }));
    }
}

// 回归: check-syntax + 单文件, 参数原样保留
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

// 同目录多个显式文件: 归入同一包(单次 Options, 行为与旧版一致)
TEST(CheckSyntaxDirTest, CheckSyntaxSameDirFilesOnePackage)
{
    ArgHelper argh;
    StrVec args{"cjah", "--check-syntax=true", "test/data/inputs/main.cj", "test/data/inputs/desugar.cj"};
    StrVec envp{};
    auto argv = CreateArgv(args);
    auto envv = CreateArgv(envp);
    auto opts = argh.ParseArgs(GetArgc(argv), argv.data(), envv.data());
    ASSERT_EQ(opts.size(), 1);
    ASSERT_EQ(opts[0].args.size(), 3);
    // 包内文件按字典序: desugar.cj 在 main.cj 之前
    EXPECT_TRUE(opts[0].args[1].ends_with("desugar.cj"));
    EXPECT_TRUE(opts[0].args[2].ends_with("main.cj"));
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