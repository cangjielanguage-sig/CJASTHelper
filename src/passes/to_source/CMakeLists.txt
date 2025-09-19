file(GLOB TO_SOURCE_SRC "*.cpp")

add_library(cjast_to_source_pass SHARED ${TO_SOURCE_SRC})

# 添加库依赖关系
target_link_libraries(cjast_to_source_pass PUBLIC cjast_helper_core cjast_wrapper)
