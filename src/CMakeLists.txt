
add_subdirectory(utils)
add_subdirectory(wrapper)
add_subdirectory(core)
add_subdirectory(passes)

set(PASSES cjast_desugar_pass cjast_to_source_pass)

set(CJAH_LIBS cjast_helper_core cjast_wrapper utils)
set(DEPEND_LIBS ${CJAH_LIBS} PARENT_SCOPE)

add_executable(cjah Main.cpp)

target_link_libraries(cjah PUBLIC ${CJAH_LIBS})

# 设置RPATH以便运行时能找到共享库
if(CMAKE_BUILD_TYPE MATCHES Release)
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
        
        # 为共享库设置RPATH
        set_target_properties(cjast_helper_core cjast_wrapper utils PROPERTIES
            INSTALL_RPATH "@loader_path"
            BUILD_WITH_INSTALL_RPATH ON
        )
    elseif(UNIX AND NOT APPLE)
        # Linux: 使用 $ORIGIN 表示可执行文件所在目录
        set_target_properties(cjah PROPERTIES
            INSTALL_RPATH "\$ORIGIN/../lib"
            BUILD_WITH_INSTALL_RPATH ON
        )
        
        # 为共享库设置RPATH
        set_target_properties(cjast_helper_core cjast_wrapper utils PROPERTIES
            INSTALL_RPATH "\$ORIGIN"
            BUILD_WITH_INSTALL_RPATH ON
        )
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

# 安装共享库
install(TARGETS ${CJAH_LIBS}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(TARGETS ${PASSES}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../config DESTINATION ${CMAKE_INSTALL_PREFIX})

install(TARGETS cjah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)