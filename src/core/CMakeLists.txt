file(GLOB VISITOR_SRC "visitor/*.cpp")
file(GLOB PASS_SRC "pass/*.cpp")
file(GLOB HELPER_SRC "*.cpp")

add_library(cjast_helper_core SHARED ${VISITOR_SRC} ${PASS_SRC} ${HELPER_SRC})

# 添加库依赖关系
target_link_libraries(cjast_helper_core PUBLIC utils cjast_wrapper)