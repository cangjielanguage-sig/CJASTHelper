file(GLOB WRAPPER_SRC "*.cpp")
add_library(cjast_wrapper SHARED ${WRAPPER_SRC})

# 添加库依赖关系
target_link_libraries(cjast_wrapper PUBLIC utils ${DEPEND_LIBS})