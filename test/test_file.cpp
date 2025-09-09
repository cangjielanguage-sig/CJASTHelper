#include "test_helper.h"
#include <gtest/gtest.h>

TEST(CjahTest, GetFileNameWithoutSuffix)
{
    EXPECT_EQ(GetFileNameWithoutExtension("test/main.cj"), "main");
}

// Continuous Integration Tests
TEST(CjahTest, Integration01)
{
    std::string cjahPath = GetCJAH();
    std::string out = ".";
    std::string src = "test/data/inputs/main.cj";
    auto tmpFiles = IterateCjah(cjahPath, src, out);
    // 检查输出文件是否存在
    EXPECT_EQ(tmpFiles.size(), 3);
    EXPECT_TRUE(CheckExists(tmpFiles)) << "Output file not found.";
    EXPECT_TRUE(CompareFile(tmpFiles[1], tmpFiles[2])) << "Actual output does not match expected.";
    EXPECT_TRUE(RemoveFiles(tmpFiles)) << "Remove tmpFiles Failed!.";
}

TEST(CjahTest, Integration02)
{
    std::string cjahPath = GetCJAH();
    std::string out = ".";
    std::string src = "test/data/inputs/desugar.cj";
    std::string tmpFile = out + "/desugar_source.cj";
    ExecDump(cjahPath, "parse", src, out);
    // 检查输出文件是否存在
    EXPECT_TRUE(CheckExist(tmpFile)) << "Output file of parse not found.";
    EXPECT_TRUE(CompareFile(tmpFile, src)) << "Actual output does not match expected.";
    std::vector<std::string> stages{"desugared-parse", "sema", "desugared-sema"};
    std::string expected = "test/data/expected/desugar/";
    for (auto& stage : stages) {
        ExecDump(cjahPath, stage, src, out);
        EXPECT_TRUE(CheckExist(tmpFile)) << "Output file of " + stage + " not found.";
        EXPECT_TRUE(CheckExist(expected + stage + "_false.cj")) << "Expected file of " + stage + " not found.";
        EXPECT_TRUE(CompareFile(tmpFile, expected + stage + "_false.cj"))
            << "Actual output of " + stage + " does not match expected.";
    }

    expected = "test/data/expected/desugar/";
    for (auto& stage : stages) {
        ExecDump(cjahPath, stage, src, out, true);
        EXPECT_TRUE(CheckExist(tmpFile)) << "Output file of " + stage + " not found.";
        EXPECT_TRUE(CheckExist(expected + stage + "_true.cj")) << "Expected file of " + stage + " not found.";
        EXPECT_TRUE(CompareFile(tmpFile, expected + stage + "_true.cj"))
            << "Actual output of " + stage + " does not match expected.";
    }

    EXPECT_TRUE(RemoveFiles({tmpFile})) << "Remove tmpFiles Failed!.";
}
