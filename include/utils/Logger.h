/**
 * @file
 *
 * This file declares the Logger, 只有DEBUG模式下才enable
 */
#pragma once

#ifdef NDEBUG
#define LOG(level, ...)
#else
#include "utils/Printer.h"
#include <fstream>

// source_location fallback for older toolchains (Clang < 16, GCC < 11)
#if __has_include(<source_location>) && (__cplusplus >= 202002L)
#include <source_location>
namespace cjah {
using std::source_location;
}
#else
namespace cjah {
struct source_location {
    const char* file_name() const noexcept { return file_; }
    const char* function_name() const noexcept { return func_; }
    uint32_t line() const noexcept { return line_; }
    uint32_t column() const noexcept { return col_; }

    static constexpr source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t line = __builtin_LINE(),
        uint32_t col = 0) noexcept {
        source_location loc;
        loc.file_ = file;
        loc.func_ = func;
        loc.line_ = line;
        loc.col_ = col;
        return loc;
    }

private:
    const char* file_ = "";
    const char* func_ = "";
    uint32_t line_ = 0;
    uint32_t col_ = 0;
};
}
#endif

// 映射表（保持顺序一致！）
constexpr int LEVEL_SIZE = 4;
constexpr Array<const char*, LEVEL_SIZE> level_names = {"DEBUG", "INFO", "WARN", "ERROR"};

class Logger {
public:
    /**
     * @brief 日志级别枚举。
     */
    enum class Level { DEBUG = 0, INFO, WARN, ERROR };

    constexpr const char* LevelName(Level level)
    {
        auto idx = static_cast<size_t>(level);
        return idx < level_names.size() ? level_names[idx] : "Unknown";
    }

    /**
     * @brief 设置日志级别。
     *
     * @param level 要设置的日志级别。
     */
    void SetLevel(Level level)
    {
        this->level = level;
    }

    /**
     * @brief 记录指定级别的日志。
     *
     * @tparam Args 参数包中的类型。
     * @param level 日志级别。
     * @param loc 日志位置。
     * @param args 要记录的日志内容。
     */
    template <Level level = Level::DEBUG, typename... Args> void Log(const cjah::source_location& loc, Args&&... args)
    {

        if (this->level > level) {
            return;
        }
        p.PVals("[", LevelName(level), "]");
        p.PVals(" <", loc.file_name(), ":", loc.line(), " ", loc.function_name());
        p.PSVals(" ", ">", std::forward<Args>(args)...).PNL();
    }

    /**
     * @brief 获取日志实例。
     */
    static Logger& Get();

    virtual ~Logger();

private:
    /**
     * @brief 私有构造函数，用于初始化文件日志流。
     *
     * @param path 文件路径。
     */
    Logger(ConStr& path);

private:
    Printer p;                         // 打印器对象
    Level level = Level::DEBUG;        // 当前日志级别
    static inline std::fstream fs;     // 文件流
    static UniquePtr<Logger> instance; // 日志实例集合
};

// MinGW/GCC compatible variadic macro - use C++20 __VA_OPT__ for zero-arg handling
#define LOG(level, ...) Logger::Get().Log<Logger::Level::level>(cjah::source_location::current() __VA_OPT__(, ) __VA_ARGS__)
#endif

#define LOGD(...) LOG(DEBUG, ##__VA_ARGS__)
#define LOGI(...) LOG(INFO, ##__VA_ARGS__)
#define LOGW(...) LOG(WARN, ##__VA_ARGS__)
#define LOGE(...) LOG(ERROR, ##__VA_ARGS__)