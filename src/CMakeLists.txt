add_subdirectory(utils)
add_subdirectory(wrapper)
add_subdirectory(core)
add_subdirectory(passes)

# 由 src/passes/* 子目录自注册（新增 pass 无需再改本文件）
set(PASSES ${CJAH_PASS_TARGETS})

set(CJAH_LIBS cjast_helper_core cjast_wrapper utils)
# DEPEND_LIBS from parent scope contains Cangjie libraries, add our libs to it
list(APPEND DEPEND_LIBS ${CJAH_LIBS})
set(DEPEND_LIBS ${DEPEND_LIBS} PARENT_SCOPE)

add_executable(cjah Main.cpp)

target_link_libraries(cjah PUBLIC ${CJAH_LIBS} ${DEPEND_LIBS})

# Windows-specific settings
if(WIN32)
    if(USING_CLANG_CL)
        # Use lld-link on Windows with clang
        target_link_options(cjah PRIVATE -fuse-ld=lld)
        target_link_options(${CJAH_LIBS} PRIVATE -fuse-ld=lld)
        target_link_options(${PASSES} PRIVATE -fuse-ld=lld)
    endif()
    # Windows uses DLL search path, no RPATH needed
    # Set DLL output name prefix
    set_target_properties(${CJAH_LIBS} PROPERTIES PREFIX "")
    set_target_properties(${PASSES} PROPERTIES PREFIX "")
    # Set import library suffix
    set_target_properties(${CJAH_LIBS} PROPERTIES IMPORT_PREFIX "")
    set_target_properties(${PASSES} PROPERTIES IMPORT_PREFIX "")
endif()

# 设置RPATH以便运行时能找到共享库
# 安装产物统一使用相对 RPATH（Linux: $ORIGIN / macOS: @loader_path），
# 配合 build.sh|build.ps1 的 alone 安装模式（把 cangjie-lsp 拷进安装目录）实现依赖闭环；
# 这里只设置 INSTALL_RPATH（不设 BUILD_WITH_INSTALL_RPATH），构建树仍保留 CMake 自动生成的
# 链接目录 RPATH，因此构建目录内直接 `-r` 运行不受影响。
if(CMAKE_BUILD_TYPE MATCHES Release)
    if(APPLE)
        target_link_options(cjah PRIVATE -dead_strip -flto)
    endif()
endif()

if(APPLE)
    # macOS: 使用 @loader_path 表示可执行文件/库所在目录
    set_target_properties(cjah PROPERTIES
        INSTALL_RPATH "@loader_path/../lib"
    )
    set_target_properties(${CJAH_LIBS} ${PASSES} PROPERTIES
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    # Linux: 使用 $ORIGIN 表示可执行文件/库所在目录
    set_target_properties(cjah PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib"
    )
    set_target_properties(${CJAH_LIBS} ${PASSES} PROPERTIES
        INSTALL_RPATH "\$ORIGIN"
    )
endif()

# 安装共享库
# 注意：DESTINATION 必须写相对路径，否则 configure 期就被绝对化，
# `cmake --install --prefix <dir>` / DESTDIR 都无法改写到别的安装目录。
install(TARGETS ${CJAH_LIBS}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS ${PASSES}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../config DESTINATION .)

install(TARGETS cjah 
    RUNTIME DESTINATION bin
)