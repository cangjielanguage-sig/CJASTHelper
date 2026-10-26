# 每个 pass 子目录把自身 target 追加到 CJAH_PASS_TARGETS（PARENT_SCOPE），
# 由 src/CMakeLists.txt 统一设置 install / RPATH。
# ★ 新增 pass 时只需在自己的 CMakeLists.txt 里登记，避免「库建了但没装」。
set(CJAH_PASS_TARGETS "")
add_subdirectory(desugar)
add_subdirectory(to_source)
add_subdirectory(sema_serialize)
add_subdirectory(ast_tree_print)
set(CJAH_PASS_TARGETS ${CJAH_PASS_TARGETS} PARENT_SCOPE)

