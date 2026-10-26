/**
 * @file
 *
 * This file implements the Logger.
 */
#ifdef NDEBUG
#else
#include "utils/Logger.h"
#include "utils/FileHelper.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>

UniquePtr<Logger> Logger::instance = nullptr;

namespace {
/**
 * @brief 日志文件候选路径（按优先级）
 *
 * 日志只是调试辅助：发布包常在不可写的当前目录下运行（CI 容器、系统目录等），
 * 因此依次尝试 LOG_PATH 环境变量 -> 当前目录 -> 可执行文件目录 -> 系统临时目录；
 * 全部失败时降级为「不写日志文件」，而不是让工具启动失败。
 */
Vec<Str> LogPathCandidates()
{
    Vec<Str> paths;
    if (const char* envPath = std::getenv("LOG_PATH"); envPath && *envPath) {
        paths.emplace_back(envPath);
    }
    paths.emplace_back("log.txt");
    try {
        auto exeDir = getExecutablePath().parent_path();
        paths.emplace_back((exeDir / "log.txt").string());
    } catch (...) {
        // 取不到自身可执行文件路径时跳过该候选
    }
    std::error_code ec;
    auto tmpDir = std::filesystem::temp_directory_path(ec);
    if (!ec) {
        paths.emplace_back((tmpDir / "cjah_log.txt").string());
    }
    return paths;
}
} // namespace

Logger& Logger::Get()
{
    // 注意: 当前实现不是线程安全的
    if (!instance) {
        auto paths = LogPathCandidates();
        instance = UniquePtr<Logger>(new Logger(paths));
    }
    return *instance;
}

Logger::~Logger()
{
    if (fs.is_open()) {
        fs.close();
    }
}

Logger::Logger(Vec<Str>& paths) : p(fs, 0)
{
    for (auto& path : paths) {
        fs.open(path, std::ios::out);
        if (fs.is_open()) {
            enabled = true;
            return;
        }
        fs.clear();
    }
    // 全部候选路径都打不开：禁用文件日志，仅在 stderr 提示一次（不影响工具运行）
    std::cerr << "warning: cannot open log file (tried LOG_PATH, cwd, exe dir, temp dir); "
                 "file logging disabled"
              << std::endl;
}
#endif
