/**
 * @file
 *
 * This file declares the Logger.
 */
#ifndef LOGGER_H
#define LOGGER_H
#include "Printer.h"
#include <exception>
#include <fstream>
#include <memory>
#include <vector>

// 定义自定义异常类
class LoggerException : public std::exception {
private:
    std::string message;

public:
    // 构造函数
    explicit LoggerException(const std::string& msg) noexcept : message(msg)
    {
    }

    // 重写 what() 方法，返回异常描述
    const char* what() const noexcept override
    {
        return message.c_str();
    }
};

class Logger {
public:
    enum class Level { DEBUG = 0, INFO, WARN, ERROR };
    enum class Mode { STD = 0, FILE, ALL, NO };

    void setLevel(Level level)
    {
        this->level = level;
    }

    template <typename... Args> inline void debug(const std::string& domain, Args&&... args)
    {
        log(Level::DEBUG, domain, std::forward<Args>(args)...);
    }

    template <typename... Args> inline void info(const std::string& domain, Args&&... args)
    {
        log(Level::INFO, domain, std::forward<Args>(args)...);
    }

    template <typename... Args> inline void warn(const std::string& domain, Args&&... args)
    {
        log(Level::WARN, domain, std::forward<Args>(args)...);
    }

    template <typename... Args> void log(Level level, const std::string& domain, Args&&... args)
    {
        if (this->level > level) {
            return;
        }
        plevel();
        pdomain(domain);
        p.pvals(std::forward<Args>(args)...);
        p.pnl();
    }

    // 获取合适的日志流示例
    static Logger& get(Mode m = Mode::STD);
    // 清空所有日志流
    static void close();

private:
    Logger();
    Logger(const std::string& path);

    void plevel();
    void pdomain(const std::string& domain);

private:
    Printer p;
    Level level = Level::DEBUG;
    static inline std::fstream fs;
    static std::vector<std::unique_ptr<Logger>> instances;
};

#endif // LOGGER_H