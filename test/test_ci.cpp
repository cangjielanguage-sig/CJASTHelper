#include "core/AstHelper.h"
#include "test_helper.h"
#include <gtest/gtest.h>
#include <filesystem>

struct TestConfig {
    std::string name;
    StrVec args;
    StrVec envp;
    std::optional<std::string> expectedPath;
    /** 独立输出目录(按配置拆分): 输入保持同一个源文件不变, 仅输出路径唯一 */
    bool isolate = false;
};

// 输出文件 = <--output-dir>/<输入基名>_source.cj(pass 侧约定 <input-basename>_source.cj)
static Str OutFile(const TestConfig& cfg)
{
    Str dir = "test/data/output";
    for (size_t i = 0; i + 1 < cfg.args.size(); ++i) {
        if (cfg.args[i] == "--output-dir") {
            dir = cfg.args[i + 1];
            break;
        }
    }
    return dir + "/" + FileName(cfg.args.back()) + "_source.cj";
}

// 与真实 cjah 一致: 向前端提供 ParseEnv 白名单内的环境变量(而非仅 CANGJIE_HOME)
static StrVec FrontendEnvp()
{
    StrVec v{"CANGJIE_HOME=" + GetEnv("CANGJIE_HOME", "")}; // 保留原行为: 始终含 CANGJIE_HOME(可能为空)
    for (auto key : {"CANGJIE_PATH", "LIBRARY_PATH", "LD_LIBRARY_PATH", "PATH", "SDKROOT"}) {
        Str val = GetEnv(key, "");
        if (!val.empty()) {
            v.emplace_back(Str(key) + "=" + val);
        }
    }
    return v;
}

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
        if (cfg.isolate) {
            // 每个配置使用独立输出目录(如 test/data/output/desugared-parse_false/):
            // 输入仍是同一个源文件(名字/内容不变, AST 中内嵌的文件名一致),
            // 输出(desugar_source.cj)落在各自目录中, 互不冲突;
            // 共享同名输出文件会让 Run() 未写盘时残留上一个配置的内容, 是 golden 误比对的根源
            Str subDir = "test/data/output/" + FileName(*cfg.expectedPath);
            std::error_code ec;
            std::filesystem::create_directories(subDir, ec);
            for (size_t i = 0; i + 1 < cfg.args.size(); ++i) {
                if (cfg.args[i] == "--output-dir") {
                    cfg.args[i + 1] = subDir;
                    break;
                }
            }
        }
        auto opts =
            argh.ParseArgs(GetArgc(CreateArgv(cfg.args)), CreateArgv(cfg.args).data(), CreateArgv(cfg.envp).data());
        ah = std::make_unique<AstHelper>(std::move(opts[0]));
    }

    void TearDown() override
    {
        // 在每个测试结束后运行的清理代码
    }

    static void TearDownTestSuite()
    {
        // 递归清理: 兼容隔离后每配置一个子目录的输出布局
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator("test/data/output", ec)) {
            if (std::filesystem::is_directory(e.path())) {
                std::filesystem::remove_all(e.path(), ec);
            } else if (e.path().extension() == ".cj") {
                std::filesystem::remove(e.path(), ec);
            }
        }
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
    std::string outFile = OutFile(cfg);
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
    std::string outFile = OutFile(cfg);

    if (expected) {
        EXPECT_TRUE(CheckExist(*expected)) << "Expected file not found.";
        if (CheckExist(outFile)) {
            // 仅在本测试确实写出输出时才刷新 golden, 防止把残留内容写入黄金文件
            if (!CompareFile(outFile, *expected)) {
                // 注意： 覆盖golden数据 确保结果正确
                EXPECT_TRUE(MoveFile(outFile, *expected)) << "Failed to generate golden data.";
            }
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
    TestConfig cfg{name,
        {"cjah", "--dump-source=" + stage, "--enable-desugar=" + enableDesugar, "--output-type=dylib", "--output-dir",
            out, "-Woff", "unused", "-Woff", "parser", src},
        FrontendEnvp(), expected};
    return cfg;
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
    // 注意: desugar.cj 故意包含未声明类型 UnknownType(sema 阶段会拒绝并报错),
    // sema/desugared-sema 阶段对该输入无法产出输出文件 —— 旧共享输出文件布局下
    // 这些配置靠比对上一个配置残留的文件"假通过"(sema 黄金文件实为 desugared-parse 内容的副本),
    // 独立输出目录后该缺陷暴露为 "Output file not found"。
    // desugared-sema 的完整覆盖由 IterateFP(合法输入 main.cj, 迭代到不动点)承担, 见 GenerateFPCfgs。
    std::vector<std::string> stages = {"parse", "desugared-parse"};
    std::vector<std::string> enableDesugars = {"false", "true"};
    for (const auto& stage : stages) {
        for (const auto& enable : enableDesugars) {
            cfgs.push_back(MKCfg(demo, stage, enable));
            cfgs.back().isolate = true; // 独立输出目录: 输出路径唯一, 消除跨配置串扰
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