#pragma once

#include <string>
#include <vector>

using ConStr = const std::string;

// 辅助函数：将 std::vector<std::string> 转换为 char* 数组，模拟 argv
std::vector<char*> CreateArgv(const std::vector<std::string>& args);

// 辅助函数：从 vector<char*> 获取 argc (不包括最后的 nullptr)
int GetArgc(const std::vector<char*>& argv);

std::string FileName(ConStr& filePath);
bool CheckExist(ConStr& file);
bool MoveFile(ConStr& src, ConStr& dst);
void RemoveFiles(ConStr& dir, ConStr& ext);
bool CompareFile(ConStr& actual, ConStr& expected);

std::string GetEnv(ConStr& key, ConStr& defaultValue);
