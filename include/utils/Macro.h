/**
 * @file
 *
 * This file declares macro tools.
 */
#ifndef MACRO_H
#define MACRO_H
// 工具宏: 完成对只有一个变参函数或宏的调用
#ifdef NDEBUG
#define AH_ASSERT(f) static_cast<void>(f)
#else
#define AH_ASSERT(f) assert(f)
#endif
#define AH_CHECK_NULL(p) AH_ASSERT((p) != nullptr)

// f(t0, ...)
// f(t0, ...) f(t1, ...)

#define EXPAND1(f, t0, ...) f(t0, ##__VA_ARGS__)
#define EXPAND2(f, t0, t1, ...)                                                                                        \
    EXPAND1(f, t0, ##__VA_ARGS__);                                                                                     \
    EXPAND1(f, t1, ##__VA_ARGS__)
#define EXPAND3(f, t0, t1, t2, ...)                                                                                    \
    EXPAND2(f, t0, t1, ##__VA_ARGS__);                                                                                 \
    EXPAND1(f, t2, ##__VA_ARGS__)
#define EXPAND4(f, t0, t1, t2, t3, ...)                                                                                \
    EXPAND3(f, t0, t1, t2, ##__VA_ARGS__);                                                                             \
    EXPAND1(f, t3, ##__VA_ARGS__)
#endif // MACRO_H