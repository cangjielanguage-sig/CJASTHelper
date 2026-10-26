file(GLOB DESUGAR_SRC "*.cpp")

add_library(cjast_desugar_pass SHARED ${DESUGAR_SRC})

# 添加库依赖关系
target_link_libraries(cjast_desugar_pass PUBLIC cjast_helper_core cjast_wrapper)

# 注册到父目录，由 src/CMakeLists.txt 统一 install / RPATH
set(CJAH_PASS_TARGETS ${CJAH_PASS_TARGETS} cjast_desugar_pass PARENT_SCOPE)
