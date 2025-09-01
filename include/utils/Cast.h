/**
 * @file
 *
 * This file declares the cast utils.
 */
#pragma once

#include <concepts>
#include <type_traits>

// Concept: Dst 必须是指针或引用
template <typename T>
concept PointerOrRef = std::is_pointer_v<T> || std::is_reference_v<T>;

// Concept: Src 可以是 指针、引用，或 有 .get() 返回指针的类型
template <typename T>
concept PointerLike = std::is_pointer_v<std::remove_reference_t<T>> || std::is_reference_v<T> ||
    requires(T&& t) { requires std::is_pointer_v<decltype(t.get())>; };

// U*&
template <typename T>
concept PointerRef = std::is_pointer_v<std::remove_reference_t<T>>;

// 通用 cast 函数
template <PointerOrRef Dst, PointerLike Src> [[nodiscard]] constexpr Dst Cast(Src&& src)
{
    if constexpr (requires { src.get(); }) {
        // src 有 .get()，提取指针
        return Cast<Dst>(src.get());
    } else {
        if constexpr (PointerRef<Src> && std::is_pointer_v<Dst>) {
            return static_cast<Dst>(src);
        } else if constexpr (PointerRef<Src> && std::is_reference_v<Dst>) {
            return static_cast<Dst>(*src);
        } else if constexpr (std::is_reference_v<Src> && std::is_pointer_v<Dst>) {
            return static_cast<Dst>(&src);
        } else if constexpr (std::is_reference_v<Src> && std::is_reference_v<Dst>) {
            return static_cast<Dst>(src);
        }
    }
}