#include "utils/FileHelper.h"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <limits.h>
#include <mach-o/dyld.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <system_error>

namespace fs = std::filesystem;

[[nodiscard]] fs::path getExecutablePath()
{
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    if (GetModuleFileNameW(nullptr, buffer, MAX_PATH) == 0) {
        throw std::system_error(
            std::error_code(GetLastError(), std::system_category()), "Failed to get executable path");
    }
    return fs::path(buffer);

#elif __APPLE__
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        throw std::runtime_error("Buffer too small for executable path");
    }
    return fs::path(buffer);

#elif __linux__
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        throw std::system_error(std::error_code(errno, std::generic_category()), "readlink failed");
    }
    buffer[len] = '\0';
    return fs::path(buffer);

#else
    static_assert(false, "Unsupported platform");
#endif
}

bool CheckExist(ConStr& file)
{
    return fs::exists(file);
}

Str FileName(ConStr& filePath)
{
    fs::path base = fs::path(filePath);
    while (!base.extension().empty()) {
        base = base.stem();
    }
    return base.string();
}

void CreateDirIfNotExists(ConStr& path)
{
    if (fs::exists(path)) {
        return;
    }
    // 创建目录（包括父目录）
    if (!fs::create_directories(path)) {
        throw std::logic_error("Try to create directory: " + path + " failed!");
    }
}

namespace {
/**
 * @brief 进程内单调唯一的后缀（时间戳 + 自增计数），避免依赖平台专有的 pid 接口
 */
Str UniqueSuffix()
{
    static std::atomic<unsigned long long> counter{0};
    auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::to_string(static_cast<long long>(tick)) + "-" + std::to_string(counter.fetch_add(1));
}
} // namespace

bool IsDirWritable(ConStr& dir)
{
    std::error_code ec;
    auto probe = fs::path(dir) / ("cjah-write-probe-" + UniqueSuffix());
    {
        std::ofstream ofs(probe, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) {
            return false;
        }
    }
    fs::remove(probe, ec);
    return true;
}

Str CreateTempDir(ConStr& prefix)
{
    std::error_code ec;
    auto base = fs::temp_directory_path(ec);
    if (ec) {
        return "";
    }
    static Vec<fs::path> created;
    static bool cleanupRegistered = false;
    if (!cleanupRegistered) {
        cleanupRegistered = true;
        std::atexit([]() {
            std::error_code ec2;
            for (auto& dir : created) {
                fs::remove_all(dir, ec2);
            }
        });
    }
    auto dir = base / (prefix + "-" + UniqueSuffix());
    fs::create_directories(dir, ec);
    if (ec || !fs::is_directory(dir, ec)) {
        return "";
    }
    created.push_back(dir);
    return dir.string();
}

Str FindPath(ConStr& name, ConStrVec& paths)
{
    // path is empty, find default path
    // 用 filesystem::path 拼接, 自动适配平台分隔符(Windows '\', POSIX '/')
    auto exeDir = getExecutablePath().parent_path();
    auto filePath = (exeDir / name).string();
    if (CheckExist(filePath)) {
        return filePath;
    }
    for (auto& p : paths) {
        filePath = (exeDir / p / name).string();
        if (CheckExist(filePath)) {
            return filePath;
        }
    }
    throw std::invalid_argument("The file is not found: " + name);
}

std::map<Str, StrVec> GroupCjFilesByDir(ConStr& dir)
{
    std::map<Str, StrVec> groups;
    std::error_code ec;
    fs::recursive_directory_iterator iter(dir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; iter != end; iter.increment(ec)) {
        if (ec) {
            // 遍历出错(权限/路径无效等)时中断, 返回已收集的部分
            break;
        }
        const auto& entry = *iter;
        if (entry.is_directory()) {
            // 跳过隐藏目录(.git/.idea 等)
            if (entry.path().filename().string().starts_with(".")) {
                iter.disable_recursion_pending();
            }
            continue;
        }
        if (entry.path().extension() == ".cj") {
            groups[entry.path().parent_path().string()].emplace_back(entry.path().string());
        }
    }
    for (auto& [pkgDir, files] : groups) {
        std::sort(files.begin(), files.end());
    }
    return groups;
}

// Json 配置文件解析
ConfigParser::ConfigParser(ConStr& name) : path(FindPath(name, searchPaths)), fs(path)
{
    if (!fs.is_open()) {
        throw std::logic_error("Try to open config file: " + name + " failed!");
    }
}
