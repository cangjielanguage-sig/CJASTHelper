/**
 * @file
 *
 * This file declares the general Printer.
 */

#pragma once

#include "utils/types/TypeAlias.h"
#include <concepts>
#include <iostream>
#include <type_traits>

// 概念：U 可被解引用
template <typename U>
concept dereferenceable = requires(U&& u) {
    {
        *std::forward<U>(u)
    };
};

// 工具：获取解引用后的类型
template <typename U> using deref_t = decltype(*std::declval<U>());

// consteval：计算类型最多可解引用多少次
template <typename U> consteval int count_deref_levels()
{
    if constexpr (dereferenceable<decltype(std::declval<std::remove_cvref_t<U>>())>) {
        return 1 + count_deref_levels<deref_t<U>>();
    } else {
        return 0;
    }
}

template <typename U> inline constexpr int count_deref_v = count_deref_levels<U>();

// 递归的 consteval 函数（允许自调用）
template <typename U, typename T, int N> consteval bool deref_to_n_impl()
{
    if constexpr (N == 0) {
        return std::derived_from<std::remove_cvref_t<U>, T>;
    } else {
        if constexpr (dereferenceable<U>) {
            return deref_to_n_impl<deref_t<U>, T, N - 1>();
        } else {
            return false;
        }
    }
}

// 现在 concept 可以安全使用这个函数
template <typename U, typename T, int N>
concept deref_to_n = deref_to_n_impl<U, T, N>();

// 概念：U 完全解引用后得到 T（即解引用 count_deref_v<U> 次）
template <typename U, typename T>
concept deref_to = deref_to_n<U, T, count_deref_v<U>>;

// 容器 value_type 可解引用到 T
template <typename C, typename T>
concept container_deref_to = requires { typename C::value_type; } && deref_to<typename C::value_type, T>;

// 指针类型解引用一次得到 T
template <typename Ptr, typename T>
concept ptr_deref_to = deref_to<Ptr, T> && (count_deref_v<Ptr> == 1);

// 递归解引用 N 次
constexpr auto& deref_n(auto&& value, std::integral_constant<int, 0>)
{
    return std::forward<decltype(value)>(value);
}

template <int N> constexpr auto& deref_n(auto&& value, std::integral_constant<int, N>)
{
    static_assert(N > 0, "N must be positive");
    if constexpr (N == 1) {
        return *std::forward<decltype(value)>(value);
    } else if constexpr (N == 2) {
        return **std::forward<decltype(value)>(value);
    } else {
        return deref_n(*std::forward<decltype(value)>(value), std::integral_constant<int, N - 1>{});
    }
}

// 完全解引用
constexpr auto& fully_deref(auto&& value)
{
    return deref_n(std::forward<decltype(value)>(value),
        std::integral_constant<int, count_deref_v<std::decay_t<decltype(value)>>>{});
}

/**
 * @brief 打印容器，支持智能指针、裸指针等可解引用元素。
 *
 * @tparam T  元素最终类型
 * @tparam C  容器类型（其 value_type 可解引用为 T）
 * @tparam CB 回调函数类型，接受 const T&
 */
template <typename T, typename C, typename CB>
    requires container_deref_to<C, T>
void printcc(
    std::ostream& os, const C& con, const CB& cb, ConStr& sep = "", ConStr& pre = "", ConStr& suf = "", bool b = false)
{
    auto it = con.cbegin();
    if (it == con.cend()) {
        if (b)
            os << pre << suf;
        return;
    }
    os << pre;
    do {
        // 完全解引用 *it 得到 T&
        if constexpr (std::is_invocable_r_v<Str, CB, const T&>) {
            os << cb(fully_deref(*it));
        } else {
            cb(fully_deref(*it));
        }
        ++it;
        if (it != con.cend())
            os << sep;
    } while (it != con.cend());
    os << suf;
}

class Printer {
public:
    /**
     * @brief 构造函数，初始化输出流和缩进级别。
     *
     * @param os 输出流，默认为标准输出。
     * @param indent 每级缩进的空格数，默认为2。
     */
    explicit Printer(std::ostream& os, int indent = 2);
    /**
     * @brief 增加当前缩进级别。
     */
    void Indent();
    /**
     * @brief 输出换行并重置下一行前导空格。
     *
     * @param n 要输出的换行符数量，默认为1。
     * @return 当前对象的引用，支持链式调用。
     */
    Printer& PNL(int n = 1);
    /**
     * @brief 减少当前缩进级别。
     */
    void Unindent();

    void Flush()
    {
        os_.flush();
    }

    /**
     * @brief 输出单个值。
     *
     * @tparam T 值的类型。
     * @param value 要输出的值。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename T> inline Printer& PVal(const T& value)
    {
        EnsureIndent();
        os_ << value;
        return *this;
    }

    /**
     * @brief 重载运算符<<，输出单个值。
     *
     * @tparam T 值的类型。
     * @param value 要输出的值。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename T> Printer& operator<<(const T& value)
    {
        return this->PVal(value);
    }

    /**
     * @brief 输出单个值并换行。
     *
     * @tparam T 值的类型。
     * @param value 要输出的值。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename T> inline Printer& PValNL(const T& value)
    {
        return PVal(value).PNL();
    }

    /**
     * @brief 输出多个值。
     *
     * @tparam Args 参数包中的类型。
     * @param args 要输出的值。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename... Args> inline Printer& PVals(Args&&... args)
    {
        EnsureIndent();
        // 使用折叠表达式展开参数包，对每个参数执行 os_ << arg
        // 表达式 ((std::cout << args), ...) 会从左到右展开
        // 例如：print(a, b, c) 展开为 (std::cout << a), (std::cout << b), (std::cout << c)
        ((os_ << args), ...);
        return *this;
    }

    /**
     * @brief 按指定分隔符输出多个值。
     *
     * @tparam Sep 分隔符的类型。
     * @tparam Args 参数包中的类型。
     * @param sep 分隔符。
     * @param args 要输出的值。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename Sep, typename... Args> inline Printer& PSVals(Sep&& sep, Args&&... args)
    {
        EnsureIndent();
        if constexpr (sizeof...(args) > 0) {
            std::size_t n{0};
            ((os_ << (n++ ? sep : "") << args), ...);
        }
        return *this;
    }

    /**
     * @brief 按指定分隔符输出容器。
     *
     * 注意: 容器为指针时， 要保证没有空指针。
     *
     * @tparam T 容器元素的类型。
     * @tparam C 容器的类型。
     * @tparam CB 回调函数的类型。
     * @param con 容器。
     * @param cb 回调函数，用于处理每个元素。
     * @param sep 分隔符。
     * @param pre 前缀字符串。
     * @param suf 后缀字符串。
     * @param b 是否强制打印 pre, suf。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename T, typename C, typename CB>
        requires container_deref_to<C, T>
    inline Printer& PVec(
        const C& con, const CB& cb, ConStr& sep = "", ConStr& pre = "", ConStr& suf = "", bool b = false)
    {
        if (b) {
            EnsureIndent();
        }
        printcc<T>(os_, con, cb, sep, pre, suf, b);
        return *this;
    }

    /**
     * @brief 输出指针指向的值。
     *
     * @tparam U 指针所指向的类型。
     * @tparam T 指针的类型。
     * @param t 指针。
     * @param cb 回调函数，用于处理指针所指向的值。
     * @param pre 前缀字符串。
     * @param suf 后缀字符串。
     * @return 当前对象的引用，支持链式调用。
     */
    template <typename U, typename T>
        requires ptr_deref_to<U, T>
    inline Printer& PPtr(const T& t, const Function<void(const U&)>& cb, ConStr& pre = "", ConStr& suf = "")
    {
        if (t) {
            PVal(pre);
            cb(*t);
            PVal(suf);
        }
        return *this;
    }

    /**
     * @brief 打印带有缩进的回调内容。
     *
     * @param cb 回调函数，包含要打印的内容。
     * @param pre 可选的前缀字符串，默认为空字符串。
     * @param suf 可选的后缀字符串，默认为空字符串。
     * @return 当前对象的引用，支持链式调用。
     */
    inline Printer& PWI(const Function<void()>& cb, ConStr& pre = "", ConStr& suf = "")
    {
        PValNL(pre);
        Indent();
        cb();
        Unindent();
        PVal(suf);
        return *this;
    }

private:
    void EnsureIndent();

    std::ostream& os_;  // 输出流
    int indent_;        // 每级缩进的空格数
    int currentIndent_; // 当前缩进级别
    bool needIndent_;   // 标记是否需要进行缩进
};