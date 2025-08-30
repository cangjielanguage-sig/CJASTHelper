/**
 * @file
 *
 * This file declares the cast utils.
 */
#pragma once

#include <concepts>
#include <type_traits>

// Concept: 类型是引用
template <typename T>
concept Reference = std::is_reference_v<T>;

// Concept: 类型是指针
template <typename T>
concept Pointer = std::is_pointer_v<T>;

// Concept: 类型有 .get() 方法
template <typename T>
concept HasGetMethod = requires(T t) { t.get(); };

// 简化 PointerLike 概念的实现
template <typename T>
concept PointerLike = HasGetMethod<T>;

// 主要的 cast 函数模板
template <typename Dst, typename Src> constexpr Dst Cast(Src&& src)
{
    // 情况1: 源类型有 .get() 方法
    if constexpr (PointerLike<std::remove_cvref_t<Src>>) {
        auto&& inner_value = src.get();
        return Cast<Dst>(std::forward<decltype(inner_value)>(inner_value));
    }
    // 情况2: 目标是指针，源是引用
    else if constexpr (Pointer<Dst> && Reference<Src> && !Pointer<std::remove_reference_t<Src>>) {
        return Cast<Dst>(std::addressof(src));
    }
    // 情况3: 目标是引用，源是指针
    else if constexpr (Reference<Dst> && Pointer<std::remove_reference_t<Src>>) {
        return Cast<Dst>(*src);
    }
    // 情况4: 源和目标都是指针
    else if constexpr (Pointer<Dst> && Pointer<std::remove_reference_t<Src>>) {
        return static_cast<Dst>(src);
    }
    // 情况5: 源和目标都是引用
    else if constexpr (Reference<Dst> && Reference<std::remove_reference_t<Src>>) {
        return static_cast<Dst>(src);
    }
    // 情况6: 从值类型到引用类型
    else if constexpr (Reference<Dst> && !Reference<Src> && !Pointer<Src>) {
        return static_cast<Dst>(src);
    }
    // 情况7: 基本静态转换
    else {
        return static_cast<Dst>(std::forward<Src>(src));
    }
}