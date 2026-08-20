#include "utils/LibraryLoader.h"
#include "utils/FileHelper.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

LibraryLoader& LibraryLoader::GetInstance()
{
    static LibraryLoader instance;
    return instance;
}

Handle LibraryLoader::LoadLib(ConStr& lib)
{
    // passes.json 中的 lib 是目标名(不带平台前缀/后缀), 这里按平台拼出真实库文件名:
    //   Windows: cjast_desugar_pass.dll (CMake 对共享库设置了 PREFIX "")
    //   Linux:   libcjast_desugar_pass.so
    //   macOS:   libcjast_desugar_pass.dylib
    Str libname = lib;
#if defined(_WIN32)
    libname += ".dll";
#elif defined(__APPLE__)
    libname = "lib" + libname + ".dylib";
#elif defined(__linux__)
    libname = "lib" + libname + ".so";
#else
#error "Unsupported platform"
#endif
    libname = FindPath(libname, searchPaths);
#ifdef _WIN32
    Handle handle = LoadLibrary(libname.c_str());
#else
    Handle handle = dlopen(libname.c_str(), RTLD_LAZY);
#endif
    return handle;
}
