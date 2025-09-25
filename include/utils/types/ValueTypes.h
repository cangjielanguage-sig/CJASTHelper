#pragma once

#include "StrTypes.h"

#include <cstdint>
#include <optional>
#include <variant>

// ====== 数值类型 ======
using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

// ====== 可选值与多态值（C++17）======
template <typename T> using Opt = std::optional<T>;

template <typename... Ts> using Var = std::variant<Ts...>;

using None = std::nullopt_t;

using StrOpt = Opt<Str>; // C++17
