add_subdirectory(utils)
add_subdirectory(wrapper)
add_subdirectory(core)
add_subdirectory(passes)

set(PASSES cjast_desugar_pass cjast_to_source_pass)

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
if(CMAKE_BUILD_TYPE MATCHES Release)
    # macOS: 使用 @loader_path（推荐）
    # Linux: 使用 $ORIGIN
    if(APPLE)
        target_link_options(cjah PRIVATE -dead_strip -flto)
        # macOS: 使用 @loader_path 表示可执行文件所在目录
        # 仅对某个 target 设置 RPATH
        set_target_properties(cjah PROPERTIES
            INSTALL_RPATH "@loader_path/../lib"
            BUILD_WITH_INSTALL_RPATH ON
        )
        
        # 为共享库设置RPATH
        set_target_properties(${CJAH_LIBS} PROPERTIES
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

if(CJAH_STANDALONE)
    message(STATUS "search path: ${CANGJIE_LIB}")

    # cangjie-fe library already found in parent CMakeLists.txt
    if(NOT CANGJIE_FE_LIB)
        message(FATAL_ERROR "cangjie-fe library not found")
    endif()

    # 安装依赖库 cangjie-fe 到 lib/
    install(FILES ${CANGJIE_FE_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()

# 安装共享库
install(TARGETS ${CJAH_LIBS}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

install(TARGETS ${PASSES}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../config DESTINATION ${CMAKE_INSTALL_PREFIX})

install(TARGETS cjah 
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)