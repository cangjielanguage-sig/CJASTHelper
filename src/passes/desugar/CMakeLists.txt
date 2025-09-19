file(GLOB DESUGAR_SRC "*.cpp")

add_library(cjast_desugar_pass SHARED ${DESUGAR_SRC})

# 添加库依赖关系
target_link_libraries(cjast_desugar_pass PUBLIC cjast_helper_core cjast_wrapper)
