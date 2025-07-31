file(GLOB AST_HELPER_SRC "*.cpp")

list(REMOVE_ITEM AST_HELPER_SRC "Main.cpp")

add_library(ast_helper STATIC ${AST_HELPER_SRC})

add_executable(cjah Main.cpp)

target_link_directories(cjah PRIVATE ${CANGJIE_TOOLS_LIB} ${LLVM_LIB})

target_link_libraries(cjah PUBLIC ast_helper ${DEPEN_LIBS})

if (CMAKE_BUILD_TYPE MATCHES Release AND MINGW)
    target_link_options(cjah PRIVATE -static-libgcc -static-libstdc++ LINKER:-Bstatic,--whole-archive -lwinpthread LINKER:-Bdynamic,--no-whole-archive)
endif ()

install(TARGETS cjah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
