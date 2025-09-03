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

target_link_directories(cjah PRIVATE ${CANGJIE_TOOLS_LIB})

target_link_libraries(cjah PUBLIC ${DEPEND_LIBS})

if (CMAKE_BUILD_TYPE MATCHES Release AND MINGW)
    target_link_options(cjah PRIVATE -static-libgcc -static-libstdc++ LINKER:-Bstatic,--whole-archive -lwinpthread LINKER:-Bdynamic,--no-whole-archive)
endif ()

install(TARGETS cjah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)