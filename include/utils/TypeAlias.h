/**
 * @file
 *
 * This file declares simple type alias.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using StrVec = std::vector<std::string>;
using ConStrVec = const std::vector<std::string>;
using StrSet = std::unordered_set<std::string>;
using StrMap = std::unordered_map<std::string, std::string>;
using StrPair = std::pair<std::string, std::string>;
using StrPairVec = std::vector<StrPair>;
using Str = std::string;
using ConStr = const std::string;
