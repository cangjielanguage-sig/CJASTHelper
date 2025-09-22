#pragma once
#include "utils/TypeAlias.h"
#include <filesystem>

std::filesystem::path getExecutablePath();

bool CheckExist(ConStr& file);
