/**
 * @file
 *
 * This file declares the general Printer.
 */

#ifndef PRINTER_H
#define PRINTER_H

#include <functional>
#include <iostream>
#include <string>
#include <type_traits>

// 辅助结构体用于计算解引用次数
template <typename U, typename = void> struct count_deref {
    static constexpr int value = 0;
};

template <typename U> using deref_t = decltype(*std::declval<U>());

template <typename U> struct count_deref<U, std::void_t<decltype(*std::declval<U>())>> {
    static constexpr int value = 1 + count_deref<deref_t<U>>::value;
};

template <typename U> inline constexpr int count_deref_v = count_deref<U>::value;

// 判断 U 是否可以解引用 N 次得到 T 类型
template <typename U, typename T, int N, typename = void> struct can_deref_to : std::false_type {};

// 基础情况：当 N == 0，直接判断 U 是否为 T&
template <typename U, typename T>
struct can_deref_to<U, T, 0, std::void_t<U>> : std::is_convertible<std::remove_cv_t<std::remove_reference_t<U>>&, T&> {
};

// 递归情况：U 可以解引用一次，然后继续判断 *U 是否可以解引用 N-1 次得到 T&
template <typename U, typename T, int N>
struct can_deref_to<U, T, N, std::void_t<decltype(*std::declval<U>())>> : can_deref_to<deref_t<U>, T, N - 1> {};

template <typename U, typename T> inline constexpr bool can_deref_to_v = can_deref_to<U, T, count_deref_v<U>>::value;

template <typename C> inline constexpr int count_deref_vv = count_deref<typename C::value_type>::value;
template <typename C, typename T>
inline constexpr bool can_deref_vv = can_deref_to<typename C::value_type, T, count_deref_vv<C>>::value;

// 这里的N是递归终止的开关
template <int N, typename U> inline decltype(auto) deref_impl(U&& u, std::integral_constant<int, 0>)
{
    return std::forward<U>(u);
}

template <int N, typename U> inline decltype(auto) deref_impl(U&& u, std::integral_constant<int, N>)
{
    if constexpr (N == 1) {
        return *std::forward<U>(u);
    } else if constexpr (N == 2) {
        return **std::forward<U>(u);
    } else {
        return deref_impl<N - 1>(*std::forward<U>(u), std::integral_constant<int, N - 1>{});
    }
}

/**
 * @brief 通用容器打印函数，支持解引用的容器元素（如 `std::vector<std::shared_ptr<T>>`）。
 * 通过回调函数 `cb` 将每个元素转换为字符串格式，并输出到指定流中。
 *
 * 该函数利用 SFINAE（`std::enable_if_t`）确保容器 `C` 中的元素可以安全解引用（通过 `can_deref_vv<C, T>` 检查），
 * 用于处理智能指针、裸指针或其他支持解引用的容器类型。
 *
 * @tparam T        容器中元素的最终类型（例如 `int`、`Person` 等）。
 * @tparam C        容器类型（例如 `std::vector<std::shared_ptr<T>>`）。
 * @tparam CB       回调函数类型，接受 `T` 并返回 `std::string`（或可转换为 `std::string` 的类型）。
 *
 * @param os        输出流（如 `std::cout`、`std::ofstream`）。
 * @param con       容器实例（类型为 `C`，其元素可安全解引用为 `T`）。
 * @param cb        回调函数，将每个 `T` 转换为字符串格式。
 *                  示例：`[](const T& t) -> std::string { return t.toString(); }`
 * @param sep       元素之间的分隔符（默认为空字符串 ""）。
 * @param pre       输出的前置字符串（默认为空 ""）。
 * @param suf       输出的后置字符串（默认为空 ""）。
 * @param b         是否强制输出 pre, suf (默认false，根据 `con` 是否为空决策)。
 */
template <typename T, typename C, typename CB>
std::enable_if_t<can_deref_vv<C, T>, void> printcc(std::ostream& os, const C& con, const CB& cb,
    const std::string& sep = "", const std::string& pre = "", const std::string& suf = "", bool b = false)
{
    auto it = con.cbegin();
    if (it == con.cend()) {
        if (b) {
            os << pre << suf;
        }
        return;
    }
    os << pre;
    do {
        constexpr int DN = count_deref_vv<C> + 1;
        if constexpr (std::is_invocable_r_v<std::string, CB, const T&>) {
            os << cb(deref_impl<DN>(it, std::integral_constant<int, DN>{}));
        } else {
            cb(deref_impl<DN>(it, std::integral_constant<int, DN>{}));
        }
        it++;
        if (it != con.cend()) {
            os << sep;
        }
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
    inline std::enable_if_t<can_deref_vv<C, T>, Printer&> PVec(const C& con, const CB& cb, const std::string& sep = "",
        const std::string& pre = "", const std::string& suf = "", bool b = false)
    {
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
    inline std::enable_if_t<can_deref_to_v<T, U> && (count_deref_v<T> == 1), Printer&> PPtr(
        const T& t, const std::function<void(const U&)>& cb, const std::string& pre = "", const std::string& suf = "")
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
    inline Printer& PWI(const std::function<void()>& cb, const std::string& pre = "", const std::string& suf = "")
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

#endif // PRINTER_H