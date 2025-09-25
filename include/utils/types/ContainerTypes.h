#pragma once

#include "StrTypes.h"

#include <array>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ====== 常用容器（通用）======
template <typename T> using Vec = std::vector<T>;

template <typename T> using List = std::list<T>;

template <typename T> using Deque = std::deque<T>;

template <typename T> using Set = std::set<T>;

template <typename T> using UnorderedSet = std::unordered_set<T>;

template <typename K, typename V> using Map = std::map<K, V>;

template <typename K, typename V> using UnorderedMap = std::unordered_map<K, V>;

template <typename T, size_t N> using Array = std::array<T, N>;

using Size = std::size_t;
using Diff = std::ptrdiff_t;

// ====== 常用数值容器 ======
using IntVec = Vec<int>;
using IntSet = UnorderedSet<int>;
using IntMap = UnorderedMap<int, int>;
using DoubleVec = Vec<double>;

// ====== 字符串容器类型 ======
using StrVec = Vec<Str>;
using ConStrVec = const StrVec;
using StrSet = UnorderedSet<Str>;
using ConStrSet = const StrSet;

using StrPair = std::pair<Str, Str>;
using StrPairVec = Vec<StrPair>;
using ConStrPairVec = const StrPairVec;
using StrList = List<Str>;
using StrDeque = Deque<Str>;

template <typename T> using StrMap = UnorderedMap<Str, T>;
template <typename T> using ConStrMap = const StrMap<T>;
