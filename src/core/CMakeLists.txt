
file(GLOB VISITOR_SRC "visitor/*.cpp")
file(GLOB PASS_SRC "pass/*.cpp")

add_library(cjast_helper_core STATIC ${VISITOR_SRC} ${PASS_SRC} AstHelper.cpp)

