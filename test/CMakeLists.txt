file(GLOB ALL_TEST_SRC "*.cpp")

# 添加测试可执行文件
add_executable(cjah_test ${ALL_TEST_SRC})

# 链接 gtest 主程序和库
target_link_libraries(cjah_test PRIVATE gtest_main gtest pthread ast_helper utils ${DEPEN_LIBS})
target_link_directories(cjah_test PRIVATE ${GTEST_LIB_PATH} ${CANGJIE_TOOLS_LIB} ${LLVM_LIB})

target_include_directories(cjah_test PRIVATE ${GTEST_INCLUDE_PATH})

# 注册测试用例到 CTest
add_test(NAME CJAHTest COMMAND cjah_test)