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
