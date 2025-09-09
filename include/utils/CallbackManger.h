/**
 * @file
 *
 * This file declares the CallBackManager.
 */
#pragma once

#include <functional>
#include <tuple>
#include <type_traits>
#include <unordered_map>

// 检查类型包是否有重复
template <typename... Ts> struct has_duplicates : std::bool_constant<false> {};

template <typename T, typename... Rest>
struct has_duplicates<T, Rest...>
    : std::bool_constant<(std::is_same_v<T, Rest> || ...) || has_duplicates<Rest...>::value> {};

template <typename... Ts> inline constexpr bool has_duplicates_v = has_duplicates<Ts...>::value;

// 检查是否为模板特化
template <typename, template <typename...> typename> inline constexpr bool is_specialization_v = false;

template <template <typename...> typename Template, typename... Args>
inline constexpr bool is_specialization_v<Template<Args...>, Template> = true;

// Concept：检查是否可调用
template <typename T>
concept is_functional = requires(T t) { std::function{t}; };

// ✅ 独立的 consteval 函数：检查 tuple 是否满足元素类型可调用
template <typename T> consteval bool is_callable_tuple()
{
    constexpr size_t N = std::tuple_size_v<T>;
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return (is_functional<std::tuple_element_t<I, T>> && ...);
    }(std::make_index_sequence<N>{});
}

// ✅ 独立的 consteval 函数：检查 tuple 是否满足元素类型不重复
template <typename T> consteval bool is_unique_tuple()
{
    constexpr size_t N = std::tuple_size_v<T>;
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return !has_duplicates_v<std::tuple_element_t<I, T>...>;
    }(std::make_index_sequence<N>{});
}

// 回调函数类型无冲突的 tuple
template <typename T>
concept unique_callable_tuple = is_specialization_v<T, std::tuple> && is_unique_tuple<T>() && is_callable_tuple<T>();

// Concept：enum 类型
template <typename T>
concept enum_type = std::is_enum_v<T>;

// 通用回调管理器
template <enum_type Key, unique_callable_tuple Callbacks> class CallbackManager {
private:
    std::unordered_map<Key, Callbacks> callbacks;

public:
    template <is_functional Callback> CallbackManager& Reg(Key key, Callback&& cb)
    {
        std::get<Callback>(callbacks[key]) = std::forward<Callback>(cb);
        return *this;
    }

    template <is_functional Callback> std::optional<std::reference_wrapper<Callback>> TryGet(Key key)
    {
        // 获取对应位置的回调函数指针
        if (auto it = callbacks.find(key); it != callbacks.end())
            if (auto& fn = std::get<Callback>(it->second); fn) {
                return std::ref(fn);
            }
        return std::nullopt;
    }

    bool Has(Key key) const
    {
        return callbacks.contains(key);
    }
};
