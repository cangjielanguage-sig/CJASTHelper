file(GLOB AST_TREE_PRINT_SRC "*.cpp")

add_library(cjast_ast_tree_print_pass SHARED ${AST_TREE_PRINT_SRC})

# 添加库依赖关系
target_link_libraries(cjast_ast_tree_print_pass PUBLIC cjast_helper_core cjast_wrapper)
