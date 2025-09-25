#include "core/AstHelper.h"
#include "test_helper.h"
#include <gtest/gtest.h>

struct TestConfig {
    std::string name;
    StrVec args;
    StrVec envp;
    std::optional<std::string> expectedPath;
};

class CJAHTest : public ::testing::TestWithParam<TestConfig> {
protected:
    std::unique_ptr<AstHelper> ah;
    TestConfig cfg;
    ArgHelper argh;

    // 构造函数：用于初始化成员变量
    CJAHTest() : ah(nullptr)
    {
    }

    void SetUp() override
    {
        // 在每个测试开始前运行的设置代码
        cfg = GetParam();
        ah = std::make_unique<AstHelper>(
            argh.ParseArgs(GetArgc(CreateArgv(cfg.args)), CreateArgv(cfg.args).data(), CreateArgv(cfg.envp).data()));
    }

    void TearDown() override
    {
        // 在每个测试结束后运行的清理代码
    }

    static void TearDownTestSuite()
    {
        RemoveFiles("test/data/output", ".cj");
    }
};

static std::unordered_map<SourceStage, std::string> stageMap{{SourceStage::DEFAULT, "default"},
    {SourceStage::DESUGARED_PARSE, "desugared-parse"}, {SourceStage::DESUGARED_SEMA, "desugared-sema"},
    {SourceStage::IMPORT, "import"}, {SourceStage::PARSE, "parse"}, {SourceStage::SEMA, "sema"}};

// ✅ 定义 PrintTo 函数（必须在全局命名空间）
void PrintTo(const TestConfig& data, ::std::ostream* os)
{
    *os << "TestConfig{"
        << "name: " << data.name << ", args: " << data.args[0] << ", " << data.args[1];
    if (data.expectedPath) {
        *os << ", expectedPath: " << *data.expectedPath;
    }
    *os << "}";
}

// Continuous Integration Tests

TEST_P(CJAHTest, CI001)
{
    auto expected = cfg.expectedPath;
    EXPECT_TRUE(ah);
    ah->Run();
    std::string outFile = "test/data/output/" + cfg.name + "_source.cj";
    EXPECT_TRUE(CheckExist(outFile)) << "Output file not found.";
    if (expected) {
        EXPECT_TRUE(CheckExist(*expected)) << "Expected file not found.";
        EXPECT_TRUE(CompareFile(outFile, *expected)) << "Output file is not equal to expected file.";
    }
}

TEST_P(CJAHTest, GenGolden)
{
    auto expected = cfg.expectedPath;
    EXPECT_TRUE(ah);
    ah->Run();
    std::string outFile = "test/data/output/" + cfg.name + "_source.cj";

    if (expected) {
        EXPECT_TRUE(CheckExist(*expected)) << "Expected file not found.";
        if (!CompareFile(outFile, *expected)) {
            // 注意： 覆盖golden数据 确保结果正确
            EXPECT_TRUE(MoveFile(outFile, *expected)) << "Failed to generate golden data.";
        }
    }
}

TestConfig MKCfg(ConStr& src, ConStr& stage, ConStr& enableDesugar = "false", bool checked = true)
{
    std::string name = FileName(src);
    std::string out = "test/data/output";
    std::optional<std::string> expected{};
    if (checked) {
        expected = stage == "parse" ? src : "test/data/expected/" + name + "/" + stage + "_" + enableDesugar + ".cj";
    }
    return {name,
        {"cjah", "--dump-source=" + stage, "--enable-desugar=" + enableDesugar, "--output-type=dylib", "--output-dir",
            out, "-Woff", "unused", "-Woff", "parser", src},
        {"CANGJIE_HOME=" + GetEnv("CANGJIE_HOME", "")}, expected};
}

/**
 * Generate all stage configs
 *
 * 生成 demo 的所有可能阶段和选项的配置
 * 相当于组合不同选项测试
 * cjah --dump-source=[parse, desugared-parse, sema, desugared-sema] --enable-desugar=[true, false] demo.cj
 * 每个组合校验预期结果
 */
std::vector<TestConfig> GenerateAllStageCfgs(ConStr& demo)
{
    std::vector<TestConfig> cfgs;
    std::vector<std::string> stages = {"parse", "desugared-parse", "sema", "desugared-sema"};
    std::vector<std::string> enableDesugars = {"false", "true"};
    for (const auto& stage : stages) {
        for (const auto& enable : enableDesugars) {
            cfgs.push_back(MKCfg(demo, stage, enable));
        }
    }
    // 打开注释测试单个场景
    // cfgs.push_back(MKCfg(demo, "desugared-parse", "true"));
    return cfgs;
}

/**
 * Generate FP test configs
 *
 * 生成 demo 的迭代测试数据
 * 迭代执行 desugared-sema 3 次
 * 相当于
 * # 生成 demo_source.cj
 * cjah --dump-source=desugared-sema demo.cj --output-dir out
 * # 迭代执行 demo_source.cj
 * cjah --dump-source=desugared-sema out/demo_source.cj --output-dir out
 * # 迭代执行 demo_source_source.cj
 * cjah --dump-source=desugared-sema out/demo_source_source.cj --output-dir out
 * 迭代到不动点，校验 demo_source_source_source.cj 和 demo_source_source.cj
 */
std::vector<TestConfig> GenerateFPCfgs(ConStr& demo)
{
    std::vector<TestConfig> cfgs;
    std::string name = FileName(demo);
    auto out = "test/data/output/";
    auto suffix = "_source.cj";
    cfgs.push_back(MKCfg(demo, "desugared-sema", "true", false));
    cfgs.push_back(MKCfg(out + name + suffix, "desugared-sema", "true", false));
    name += "_source";
    std::string expected = out + name + suffix;
    // 最后一次迭代校验结果
    auto lastCfg = MKCfg(out + name + suffix, "desugared-sema", "true", false);
    lastCfg.expectedPath = expected;
    cfgs.push_back(lastCfg);
    return cfgs;
}

// IterateAllStages/CJAHTest.CI001/*
INSTANTIATE_TEST_SUITE_P(IterateAllStages, CJAHTest,
    ::testing::ValuesIn(GenerateAllStageCfgs("test/data/inputs/desugar.cj")),
    [](const ::testing::TestParamInfo<CJAHTest::ParamType>& info) {
        return info.param.name + std::to_string(info.index);
    });

// IterateFP/CJAHTest.CI001/*
INSTANTIATE_TEST_SUITE_P(IterateFP, CJAHTest, ::testing::ValuesIn(GenerateFPCfgs("test/data/inputs/main.cj")),
    [](const ::testing::TestParamInfo<CJAHTest::ParamType>& info) {
        return info.param.name + std::to_string(info.index);
    });