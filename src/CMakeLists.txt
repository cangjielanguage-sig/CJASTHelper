file(GLOB AST_HELPER_SRC "AstHelper.cpp")
file(GLOB UTILS_SRC "utils/*.cpp")
file(GLOB WRAPPER_SRC "wrapper/*.cpp")
file(GLOB VISITOR_SRC "visitor/*.cpp")
file(GLOB PASS_SRC "pass/*.cpp")

add_library(ast_helper STATIC ${AST_HELPER_SRC})
add_library(utils STATIC ${UTILS_SRC})
add_library(wrapper STATIC ${WRAPPER_SRC})
add_library(visitor STATIC ${VISITOR_SRC})
add_library(pass STATIC ${PASS_SRC})

set(CJAH_LIBS ast_helper pass visitor wrapper utils)
set(DEPEND_LIBS ${CJAH_LIBS} ${DEPEND_LIBS} PARENT_SCOPE)
set(DEPEND_LIBS ${CJAH_LIBS} ${DEPEND_LIBS})

add_executable(cjah Main.cpp)

target_link_libraries(cjah PUBLIC ${DEPEND_LIBS})

if(CMAKE_BUILD_TYPE MATCHES Release)
    if(CJAH_STANDALONE)
        # macOS: 使用 @loader_path（推荐）
        # Linux: 使用 $ORIGIN
        if(APPLE)
            target_link_options(cjah PRIVATE -dead_strip -flto)
            # macOS: 使用 @loader_path 表示可执行文件所在目录
            set(CMAKE_INSTALL_RPATH "@loader_path/../lib")

            # 仅对某个 target 设置 RPATH
            set_target_properties(cjah PROPERTIES
                INSTALL_RPATH "@loader_path/../lib"
                BUILD_WITH_INSTALL_RPATH ON
            )
        elseif(UNIX AND NOT APPLE)
            # Linux: 使用 $ORIGIN 表示可执行文件所在目录
            set_target_properties(cjah PROPERTIES
                INSTALL_RPATH "\$ORIGIN/../lib"
                BUILD_WITH_INSTALL_RPATH ON
            )
        endif()
    endif()
endif()

# if (CMAKE_BUILD_TYPE MATCHES Release AND MINGW)
#     target_link_options(cjah PRIVATE -static-libgcc -static-libstdc++ LINKER:-Bstatic,--whole-archive -lwinpthread LINKER:-Bdynamic,--no-whole-archive)
# endif ()

if(CJAH_STANDALONE)
    message(STATUS "search path: ${CANGJIE_LIB}")

    set(LIB_SEARCH_PATHS ${CANGJIE_LIB})

    # 查找 libcangjie-fe.so
    find_library(LIBFE_LIB NAMES cangjie-fe libcangjie-fe.so libcangjie-fe.dylib PATHS ${LIB_SEARCH_PATHS})
    
    if(NOT LIBFE_LIB)
        message(FATAL_ERROR "libcangjie-fe.so not found")
    endif()

    # 安装依赖库 libcangjie-fe.so 到 lib/
    install(FILES ${LIBFE_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()

install(TARGETS cjah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
