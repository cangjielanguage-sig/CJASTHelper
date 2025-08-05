/**
 * @file
 *
 * This file implements the Logger.
 */
#include "Logger.h"
#include <cstdlib>

std::vector<std::unique_ptr<Logger>> Logger::instances(2);

LoggerException::LoggerException(const std::string& msg) noexcept : message(msg)
{
}

const char* LoggerException::what() const noexcept
{
    return message.c_str();
}

Logger& Logger::Get(Mode m)
{
    // 注意: 当前实现不是线程安全的
    if (m == Mode::FILE) {
        if (!instances.at(1)) {
            const char* logPath = std::getenv("LOG_PATH");
            instances[1] = std::unique_ptr<Logger>(new Logger(logPath ? logPath : "log.txt"));
        }
        return *instances[1];
    }
    // Default
    if (!instances.at(0)) {
        instances[0] = std::unique_ptr<Logger>(new Logger());
    }
    return *instances.at(0);
}

void Logger::Close()
{
    if (instances[1]->fs.is_open()) {
        instances[1]->fs.close();
    }
    instances[1] = nullptr;
}

Logger::Logger() : p(std::cout, 0)
{
}

Logger::Logger(const std::string& path) : p(fs, 0)
{
    fs.open(path, std::ios::out);
    if (!fs.is_open()) {
        throw LoggerException("Failed to open logger file " + path);
    }
}

void Logger::PLevel()
{
    switch (level) {
        case Level::DEBUG:
            p.PVal("[DEBUG]");
            break;
        case Level::INFO:
            p.PVal("[INFO]");
            break;
        case Level::WARN:
            p.PVal("[WARN]");
            break;
        case Level::ERROR:
            p.PVal("[ERROR]");
            break;
        default:
            break;
    }
}

void Logger::PDomain(const std::string& domain)
{
    p.PVal(" <" + domain + "> ");
}
