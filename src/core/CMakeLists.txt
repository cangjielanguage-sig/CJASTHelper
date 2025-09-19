file(GLOB VISITOR_SRC "visitor/*.cpp")
file(GLOB PASS_SRC "pass/*.cpp")

add_library(cjast_helper_core SHARED ${VISITOR_SRC} ${PASS_SRC} AstHelper.cpp)

# 添加库依赖关系
target_link_libraries(cjast_helper_core PUBLIC cjast_wrapper)