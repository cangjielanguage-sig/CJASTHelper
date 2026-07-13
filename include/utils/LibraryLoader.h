#pragma once

#include "utils/types/TypeAlias.h"

#ifdef _WIN32
#ifndef __MINGW32__
#include <windows.h>
using Handle = HMODULE;
#else
using Handle = void*;
#endif
#else
using Handle = void*;
#endif

// Library Loader
class LibraryLoader {
public:
    static LibraryLoader& GetInstance();

    Handle LoadLib(ConStr& lib);

private:
    static inline ConStrVec searchPaths{"lib", "bin", "../lib", "../../lib"};
};
