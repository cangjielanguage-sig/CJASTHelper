#pragma once
#include "utils/TypeAlias.h"
#include <filesystem>

std::filesystem::path getExecutablePath();

bool CheckExist(ConStr& file);
Str FileName(ConStr& filePath);
void CreateDirIfNotExists(ConStr& path);