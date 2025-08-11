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
    /**
     * @brief 日志级别枚举。
     */
    enum class Level { DEBUG = 0, INFO, WARN, ERROR };

    /**
     * @brief 日志模式枚举。
     */
    enum class Mode {
        STD = 0, // 输出到标准输出
        FILE,    // 输出到文件
        ALL,     // 同时输出到标准输出和文件
        NO       // 不输出任何日志
    };

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
     * @brief 记录调试级别的日志。
     *
     * @tparam Args 参数包中的类型。
     * @param domain 日志域。
     * @param args 要记录的日志内容。
     */
    template <typename... Args> inline void Debug(const std::string& domain, Args&&... args)
    {
        Log(Level::DEBUG, domain, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录信息级别的日志。
     *
     * @tparam Args 参数包中的类型。
     * @param domain 日志域。
     * @param args 要记录的日志内容。
     */
    template <typename... Args> inline void Info(const std::string& domain, Args&&... args)
    {
        Log(Level::INFO, domain, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录警告级别的日志。
     *
     * @tparam Args 参数包中的类型。
     * @param domain 日志域。
     * @param args 要记录的日志内容。
     */
    template <typename... Args> inline void Warn(const std::string& domain, Args&&... args)
    {
        Log(Level::WARN, domain, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录错误级别的日志。
     *
     * @tparam Args 参数包中的类型。
     * @param domain 日志域。
     * @param args 要记录的日志内容。
     */
    template <typename... Args> inline void Error(const std::string& domain, Args&&... args)
    {
        Log(Level::ERROR, domain, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录指定级别的日志。
     *
     * @tparam Args 参数包中的类型。
     * @param level 日志级别。
     * @param domain 日志域。
     * @param args 要记录的日志内容。
     */
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

    /**
     * @brief 获取合适的日志实例。
     *
     * @param m 日志模式，默认为文件模式。
     * @return 日志实例的引用。
     */
    static Logger& Get(Mode m = Mode::FILE);

    /**
     * @brief 清空所有日志流。
     */
    static void Close();

private:
    /**
     * @brief 私有构造函数，防止外部实例化。
     */
    Logger();

    /**
     * @brief 私有构造函数，用于初始化文件日志流。
     *
     * @param path 文件路径。
     */
    Logger(const std::string& path);

    /**
     * @brief 打印日志级别。
     *
     * @param level 日志级别。
     */
    void PLevel(Level level);

    /**
     * @brief 打印日志域。
     *
     * @param domain 日志域。
     */
    void PDomain(const std::string& domain);

private:
    Printer p;                                             // 打印器对象
    Level level = Level::DEBUG;                            // 当前日志级别
    static inline std::fstream fs;                         // 文件流
    static std::vector<std::unique_ptr<Logger>> instances; // 日志实例集合
};

#endif // LOGGER_H