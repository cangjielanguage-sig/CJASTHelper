cmake_minimum_required(VERSION 3.16.5)
project(CJASTHelper LANGUAGES CXX)

option(CJAH_STANDALONE "Build standalone version" OFF)
option(CMAKE_ENABLE_ASSERT "Enable the assert and checking" OFF)
option(CMAKE_ENABLE_TEST "Enable testing" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Cangjie SDK paths (from $CANGJIE_HOME set by scripts/mac_env.sh / linux_env.sh / win_env.ps1)
set(CANGJIE_HOME $ENV{CANGJIE_HOME})
if(NOT CANGJIE_HOME)
    message(FATAL_ERROR "Please set CANGJIE_HOME to the Cangjie SDK root")
endif()
set(CANGJIE_LIB_DIR "${CANGJIE_HOME}/tools/lib")
if(WIN32)
    set(CANGJIE_CJNATIVE_LIB_DIR "${CANGJIE_HOME}/lib/windows_x86_64_cjnative")
elseif(APPLE)
    set(CANGJIE_CJNATIVE_LIB_DIR "${CANGJIE_HOME}/lib/darwin_aarch64_cjnative")
else()
    set(CANGJIE_CJNATIVE_LIB_DIR "${CANGJIE_HOME}/lib/linux_x86_64_cjnative")
endif()

# For gcc tool chain
if(GCC_TOOLCHAIN_FLAG)
    add_definitions(--gcc-toolchain="${GCC_TOOLCHAIN_FLAG}")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# Output build directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(include)

# Link directories (only add existing dirs to avoid stale-path linker warnings)
if(EXISTS "${CANGJIE_LIB_DIR}")
    link_directories(${CANGJIE_LIB_DIR})
endif()
if(EXISTS "${CANGJIE_CJNATIVE_LIB_DIR}")
    link_directories(${CANGJIE_CJNATIVE_LIB_DIR})
endif()
if(CANGJIE_INCLUDE)
    if(EXISTS "${CANGJIE_INCLUDE}/cangjie")
        message(STATUS "include: ${CANGJIE_INCLUDE}")
        include_directories(${CANGJIE_INCLUDE})
    else()
        message(FATAL_ERROR "Please set correct CANGJIE_INCLUDE include cangjie headers")
    endif()
else()
    message(FATAL_ERROR "Please set correct CANGJIE_INCLUDE")
endif()
if(JSON_INCLUDE)
    include_directories(${JSON_INCLUDE})
else()
    message(FATAL_ERROR "Please set correct JSON_INCLUDE")
endif()
if(CANGJIE_LIB)
    if(EXISTS "${CANGJIE_LIB}")
        message(STATUS "lib: ${CANGJIE_LIB}")
        link_directories(${CANGJIE_LIB})
    else()
        message(FATAL_ERROR "Please set correct CANGJIE_LIB")
    endif()
endif()

# Add definitions and compiler flags by build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, default to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)" FORCE)
endif()

message(STATUS "Build type for the current project: ${CMAKE_BUILD_TYPE}")
if(NOT (CMAKE_BUILD_TYPE MATCHES Debug))
    add_compile_definitions(NDEBUG)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fstandalone-debug")
endif()

# For release version
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fPIC -fno-omit-frame-pointer -pipe -fno-common -fno-strict-aliasing -fstack-protector-all -ftrapv")
    if(CJAH_STANDALONE)
        set(CMAKE_CXX_FLAGS_RELEASE "-Os -ffunction-sections -fdata-sections")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-D_FORTIFY_SOURCE=2 -O2")
    endif()
    if(NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    endif()
    add_compile_definitions(RELEASE)
endif()

add_library(cangjie-lsp SHARED IMPORTED)

if(WIN32)
    set_target_properties(cangjie-lsp PROPERTIES
        IMPORTED_IMPLIB ${CANGJIE_HOME}/tools/lib/libcangjie-lsp.dll.a
    )
elseif(APPLE)
    set_target_properties(cangjie-lsp PROPERTIES
        IMPORTED_LOCATION ${CANGJIE_HOME}/tools/lib/libcangjie-lsp.dylib
    )
else()
    set_target_properties(cangjie-lsp PROPERTIES
        IMPORTED_LOCATION ${CANGJIE_HOME}/tools/lib/libcangjie-lsp.so
    )
endif()

# Security hardening flags for Linux
if(NOT APPLE AND NOT WIN32 AND NOT CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -Wl,-Bsymbolic -rdynamic -Wl,--no-undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -Wl,-Bsymbolic -rdynamic -Wl,--no-undefined")
endif()

# For windows platform
if (WIN32)
    set(CMAKE_C_COMPILER "${MINGW_BIN}/x86_64-w64-mingw32-clang.exe")
    set(CMAKE_CXX_COMPILER "${MINGW_BIN}/x86_64-w64-mingw32-clang++.exe")
    set(CMAKE_AR "${MINGW_BIN}/llvm-ar.exe")
    set(CMAKE_RANLIB "${MINGW_BIN}/llvm-ranlib.exe")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-insert-timestamp")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-insert-timestamp")
    set(LIB_SEC ssp)
endif()

# Depend libraries in Cangjie Tools
set(DEPEND_LIBS ${LIB_SEC})
if(CJAH_STANDALONE)
    list(APPEND DEPEND_LIBS cangjie-fe)
else()
    list(APPEND DEPEND_LIBS cangjie-lsp)
endif()
# Only cjnative
# other version
add_compile_definitions(CANGJIE_CODEGEN_CJNATIVE_BACKEND)
add_compile_definitions(NO_CHIR)
# LTS version
add_compile_definitions(CANGJIE_CODEGEN_LLVMGC_BACKEND)

if(CMAKE_ENABLE_ASSERT)
    add_compile_definitions(CMAKE_ENABLE_ASSERT)
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" TARGET_OS)
add_compile_definitions(__${TARGET_OS}__)
# Add source
add_subdirectory(src)

if(WIN32)
    set(LIBSSP_DLL ${MINGW_BIN}/../x86_64-w64-mingw32/bin/libssp-0.dll)
    message(STATUS "find libssp ${LIBSSP_DLL}")
    if(EXISTS "${LIBSSP_DLL}")
        install(FILES "${LIBSSP_DLL}" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
    endif()
endif()

# For Testing
if(CMAKE_ENABLE_TEST)
    # If local gtest built, configure GTEST_RELEASE_PATH google test install path
    if(DEFINED ENV{GTEST_RELEASE_PATH})
        set(GTEST_LIB_PATH $ENV{GTEST_RELEASE_PATH}/lib)
        set(GTEST_INCLUDE_PATH $ENV{GTEST_RELEASE_PATH}/include)
    else()
        # Depend on gtest source build (need to download source)
        set(GTEST_VERSION "v1.17.0")
        set(GTEST_URL "https://github.com/google/googletest/archive/tags/${GTEST_VERSION}.zip")

        # Set third party directory
        set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
        set(GTEST_SOURCE_DIR "${THIRD_PARTY_DIR}/googletest-${GTEST_VERSION}")

        # Check if third_party/gtest exists
        if(NOT EXISTS "${GTEST_SOURCE_DIR}")
            message(FATAL_ERROR "Please download gtest from ${GTEST_URL} firstly, and extract the source into ${GTEST_SOURCE_DIR}.")
        endif()
        # Add gtest source path
        add_subdirectory(${GTEST_SOURCE_DIR})
    endif()
    # Now can add gtest as executable or test target dependency
    enable_testing()
    add_subdirectory(test)
endif()