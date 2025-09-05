/**
 * @file
 *
 * This file declares the CallBackManager.
 */
#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <variant>

// 工具：判断是否是模板特化
template <typename T, template <typename...> typename Template> struct is_specialization : std::false_type {};

template <template <typename...> typename Template, typename... Args>
struct is_specialization<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> typename Template>
inline constexpr bool is_specialization_v = is_specialization<T, Template>::value;

// Concept：Key 必须是 enum 类型
template <typename T>
concept EnumType = std::is_enum_v<T>;

// Concept：必须是 std::variant<...>
template <typename T>
concept VariantType = is_specialization_v<T, std::variant>;

// 通用回调管理器
template <EnumType Key, VariantType CallbackVariant> class CallBackManager {
private:
    std::unordered_map<Key, CallbackVariant> callbacks;

public:
    template <typename Callback> void reg(Key key, Callback&& cb)
    {
        callbacks[key] = std::forward<Callback>(cb);
    }

    template <typename Callback> std::optional<std::reference_wrapper<Callback>> get(Key key)
    {
        auto it = callbacks.find(key);
        if (it == callbacks.end())
            return std::nullopt;
        if (auto* ptr = std::get_if<Callback>(&it->second)) {
            return std::ref(*ptr);
        }
        return std::nullopt;
    }

    bool has(Key key) const
    {
        return callbacks.contains(key);
    }
};
