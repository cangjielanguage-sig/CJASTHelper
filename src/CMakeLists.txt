file(GLOB AST_HELPER_SRC "*.cpp")

add_executable(cjah ${AST_HELPER_SRC})



target_link_directories(cjah PUBLIC ${CANGJIE_TOOLS_LIB} ${LLVM_LIB})

target_link_libraries(cjah PUBLIC ${DEPEN_LIBS})

include_directories(${CANGJIE_HOME}/include)
if(CANGJIE_INCLUDE)
    message(STATUS "include: ${CANGJIE_INCLUDE}")
    include_directories(${CANGJIE_INCLUDE})
endif()

if (CMAKE_BUILD_TYPE MATCHES Release AND MINGW)
    target_link_options(cjah PRIVATE -static-libgcc -static-libstdc++ LINKER:-Bstatic,--whole-archive -lwinpthread LINKER:-Bdynamic,--no-whole-archive)
endif ()

install(TARGETS cjah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
