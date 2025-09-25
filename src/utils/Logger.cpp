/**
 * @file
 *
 * This file implements the Logger.
 */
#ifdef NDEBUG
#else
#include "utils/Logger.h"
#include <stdexcept>

UniquePtr<Logger> Logger::instance = nullptr;

Logger& Logger::Get()
{
    // 注意: 当前实现不是线程安全的
    if (!instance) {
        const char* logPath = std::getenv("LOG_PATH");
        instance = UniquePtr<Logger>(new Logger(logPath ? logPath : "log.txt"));
    }
    return *instance;
}

Logger::~Logger()
{
    if (fs.is_open()) {
        fs.close();
    }
}

Logger::Logger(ConStr& path) : p(fs, 0)
{
    fs.open(path, std::ios::out);
    if (!fs.is_open()) {
        throw std::logic_error("Logger: Failed to open logger file " + path);
    }
}
#endif
