file(GLOB ALL_TEST_SRC "*.cpp")

# 添加测试可执行文件
add_executable(runner ${ALL_TEST_SRC})

# 链接 gtest 主程序和库
target_link_libraries(runner PRIVATE gtest_main gtest pthread ast_helper ${DEPEN_LIBS})
target_link_directories(runner PRIVATE ${CANGJIE_TOOLS_LIB} ${LLVM_LIB})

# 注册测试用例到 CTest
add_test(NAME ExampleTest COMMAND runner)