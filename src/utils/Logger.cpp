/**
 * @file
 *
 * This file implements the Logger.
 */
#ifdef NDEBUG
#else
#include "utils/Logger.h"
#include <cstdlib>

LoggerException::LoggerException(const std::string& msg) noexcept : message(msg)
{
}

const char* LoggerException::what() const noexcept
{
    return message.c_str();
}

std::unique_ptr<Logger> Logger::instance = nullptr;

Logger& Logger::Get()
{
    // 注意: 当前实现不是线程安全的
    if (!instance) {
        const char* logPath = std::getenv("LOG_PATH");
        instance = std::unique_ptr<Logger>(new Logger(logPath ? logPath : "log.txt"));
    }
    return *instance;
}

Logger::~Logger()
{
    if (fs.is_open()) {
        fs.close();
    }
}

Logger::Logger(const std::string& path) : p(fs, 0)
{
    fs.open(path, std::ios::out);
    if (!fs.is_open()) {
        throw LoggerException("Failed to open logger file " + path);
    }
}
#endif
