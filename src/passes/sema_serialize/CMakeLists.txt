file(GLOB SEMA_SERIALIZE_SRC "*.cpp")

add_library(cjast_sema_pass SHARED ${SEMA_SERIALIZE_SRC})

# 添加库依赖关系
target_link_libraries(cjast_sema_pass PUBLIC cjast_helper_core cjast_wrapper)

# 注册到父目录，由 src/CMakeLists.txt 统一 install / RPATH
set(CJAH_PASS_TARGETS ${CJAH_PASS_TARGETS} cjast_sema_pass PARENT_SCOPE)
