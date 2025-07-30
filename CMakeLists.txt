cmake_minimum_required(VERSION 3.16.5)
project(CJASTHelper)

option(CMAKE_ENABLE_ASSERT "Enable the assert and checking" OFF)
option(CMAKE_ENABLE_TEST "Enable testing" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# For gcc tool chain
if(GCC_TOOLCHAIN_FLAG)
    add_definitions(--gcc-toolchain="${GCC_TOOLCHAIN_FLAG}")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

# Output build directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add headers
if(NOT CMAKE_INCLUDE_SYSTEM_FLAG_CXX)
    set(CXX_SYSTEM_INCLUDE_CONFIGURATION_FLAG /experimental:external /external:W0)
    add_compile_options(${CXX_SYSTEM_INCLUDE_CONFIGURATION_FLAG})
    set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX /external:I)
endif()

include_directories(include)

# Add definitions and compiler flags by build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, default to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)" FORCE)
endif()

message(STATUS "Build type for the current project: ${CMAKE_BUILD_TYPE}")
if(NOT (CMAKE_BUILD_TYPE MATCHES Debug))
    add_compile_definitions(NDEBUG)
endif()

# For release version
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all -ftrapv -fPIE")
    set(CMAKE_CXX_FLAGS_RELEASE "-D_FORTIFY_SOURCE=2 -O2")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s -pie")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -pie")
    add_compile_definitions(RELEASE)
endif()

if(NOT APPLE AND NOT WIN32 AND NOT CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -Wl,-Bsymbolic -rdynamic -Wl,--no-undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -Wl,-Bsymbolic -rdynamic -Wl,--no-undefined")
endif()

# For windows platform
if (WIN32)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-insert-timestamp")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-insert-timestamp")
    set(LIB_SEC ssp)
endif()

# Depend libraries in Cangjie Tools
set(DEPEN_LIBS ${LIB_SEC} cangjie-frontend cangjie-lsp)

if(DEFINED ENV{CANGJIE_HOME})
    set(CANGJIE_TOOLS_LIB $ENV{CANGJIE_HOME}/tools/lib)
    set(LLVM_LIB $ENV{CANGJIE_HOME}/third_party/llvm/lib)
else()
    message(FATAL_ERROR "Error: CANGJIE_HOME environment variable is not set.")
endif()

# Only cjnative
add_compile_definitions(CANGJIE_CODEGEN_CJNATIVE_BACKEND)

if(CMAKE_ENABLE_ASSERT)
    add_compile_definitions(CMAKE_ENABLE_ASSERT)
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" TARGET_OS)
add_compile_definitions(__${TARGET_OS}__)

# Add source
add_subdirectory(src)
