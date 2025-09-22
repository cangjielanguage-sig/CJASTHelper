#pragma once

#include "utils/FileHelper.h"

// 辅助函数：将 std::vector<std::string> 转换为 char* 数组，模拟 argv
std::vector<char*> CreateArgv(ConStrVec& args);

// 辅助函数：从 vector<char*> 获取 argc (不包括最后的 nullptr)
int GetArgc(const std::vector<char*>& argv);

bool MoveFile(ConStr& src, ConStr& dst);
void RemoveFiles(ConStr& dir, ConStr& ext);
bool CompareFile(ConStr& actual, ConStr& expected);

Str GetEnv(ConStr& key, ConStr& defaultValue);
