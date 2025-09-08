#pragma once

#include <string>
#include <vector>

using ConStr = const std::string;

// 辅助函数：将 std::vector<std::string> 转换为 char* 数组，模拟 argv
std::vector<char*> CreateArgv(const std::vector<std::string>& args);

// 辅助函数：从 vector<char*> 获取 argc (不包括最后的 nullptr)
int GetArgc(const std::vector<char*>& argv);

// 工具函数：读取文件内容为字符串
std::string ReadFileToString(ConStr& filename);

// Helper function to read from a FILE* into a string
std::string ExecCmd(const char* cmd);

void ExecDump(ConStr& cjahPath, ConStr& stage, ConStr& src, ConStr& out, bool desugar = false);

bool CheckExist(ConStr& file);
bool CheckExists(const std::vector<std::string>& files);

bool RemoveFiles(const std::vector<std::string>& files);

bool CompareFile(ConStr& actual, ConStr& expected);

std::string GetCJAH();

std::string GetFileNameWithoutSuffix(ConStr& fname);

std::vector<std::string> IterateCjah(ConStr& cjahPath, ConStr& src, ConStr& out);