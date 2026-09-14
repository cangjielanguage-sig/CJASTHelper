file(GLOB SEMA_SERIALIZE_SRC "*.cpp")

add_library(cjast_sema_pass SHARED ${SEMA_SERIALIZE_SRC})

# 添加库依赖关系
target_link_libraries(cjast_sema_pass PUBLIC cjast_helper_core cjast_wrapper)
