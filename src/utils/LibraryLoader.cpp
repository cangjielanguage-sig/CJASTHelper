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
    Str libname = lib;
#ifdef _WIN32
    libname = libname + ".dll";
    libname = FindPath(libname, searchPaths);
    Handle handle = LoadLibrary(libname.c_str());
#else
#if defined(__linux__)
    libname = libname + ".so";
#else
    libname = libname + ".dylib";
#endif
    libname = FindPath(libname, searchPaths);
    Handle handle = dlopen(libname.c_str(), RTLD_LAZY);
#endif
    return handle;
}
