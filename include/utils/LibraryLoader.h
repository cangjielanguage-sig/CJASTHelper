#pragma once

#include "utils/types/StrTypes.h"

#ifdef _WIN32
using Handle = HMODULE;
#else
using Handle = void*;
#endif

// Library Loader
class LibraryLoader {
public:
    static LibraryLoader& GetInstance();

    Handle LoadLibrary(ConStr& lib);
};
