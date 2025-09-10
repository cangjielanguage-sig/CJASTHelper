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
#include <exception>
#include <fstream>
#include <memory>
#include <source_location> // C++20 起支持

// Logger Exception
class LoggerException : public std::exception {
private:
    std::string message;

public:
    explicit LoggerException(const std::string& msg) noexcept;
    const char* what() const noexcept override;
};

// 映射表（保持顺序一致！）
constexpr int LEVEL_SIZE = 4;
constexpr std::array<const char*, LEVEL_SIZE> level_names = {"DEBUG", "INFO", "WARN", "ERROR"};

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
    template <Level level = Level::DEBUG, typename... Args> void Log(const std::source_location& loc, Args&&... args)
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
    Logger(const std::string& path);

private:
    Printer p;                               // 打印器对象
    Level level = Level::DEBUG;              // 当前日志级别
    static inline std::fstream fs;           // 文件流
    static std::unique_ptr<Logger> instance; // 日志实例集合
};

#define LOG(level, ...) Logger::Get().Log<Logger::Level::level>(std::source_location::current(), ##__VA_ARGS__)
#endif

#define DEBUG(...) LOG(DEBUG, ##__VA_ARGS__)
#define INFO(...) LOG(INFO, ##__VA_ARGS__)
#define WARN(...) LOG(WARN, ##__VA_ARGS__)
#define ERROR(...) LOG(ERROR, ##__VA_ARGS__)