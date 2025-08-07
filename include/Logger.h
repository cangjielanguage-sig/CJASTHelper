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

// Logger Exception
class LoggerException : public std::exception {
private:
    std::string message;

public:
    explicit LoggerException(const std::string& msg) noexcept;
    const char* what() const noexcept override;
};

class Logger {
public:
    enum class Level { DEBUG = 0, INFO, WARN, ERROR };
    enum class Mode { STD = 0, FILE, ALL, NO };

    void SetLevel(Level level)
    {
        this->level = level;
    }

    template <typename... Args> inline void Debug(const std::string& domain, Args&&... args)
    {
        Log(Level::DEBUG, domain, std::forward<Args>(args)...);
    }

    template <typename... Args> inline void Info(const std::string& domain, Args&&... args)
    {
        Log(Level::INFO, domain, std::forward<Args>(args)...);
    }

    template <typename... Args> inline void Warn(const std::string& domain, Args&&... args)
    {
        Log(Level::WARN, domain, std::forward<Args>(args)...);
    }

    template <typename... Args> void Log(Level level, const std::string& domain, Args&&... args)
    {
#ifdef NDEBUG
#else
        if (this->level > level) {
            return;
        }
        PLevel(level);
        PDomain(domain);
        p.PVals(std::forward<Args>(args)...);
        p.PNL();
#endif
    }

    // 获取合适的日志流示例
    static Logger& Get(Mode m = Mode::FILE);
    // 清空所有日志流
    static void Close();

private:
    Logger();
    Logger(const std::string& path);

    void PLevel(Level level);
    void PDomain(const std::string& domain);

private:
    Printer p;
    Level level = Level::DEBUG;
    static inline std::fstream fs;
    static std::vector<std::unique_ptr<Logger>> instances;
};

#endif // LOGGER_H