# =============================================================================
# tests/Renderer —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Application/Test/tests/Renderer/cmake/对外接口.cmake
#
# 契约（与各引擎层的对外接口同名同义）
#     BYJY_TEST_RENDERER_OUT_LIB     TestRenderer
#     BYJY_TEST_RENDERER_OUT_LIBDIR  本子工程根
#     BYJY_TEST_RENDERER_OUT_INC     Test 层根
#     BYJY_TEST_RENDERER_OUT_DEF / _LINK / _SYS  空
#
# 说明：静态库编译期的 Renderer / System / Foundation / gtest 依赖由子工程自身
#       构建脚本闭合；主层只需整体收编本库（WHOLE_ARCHIVE）。
# =============================================================================

get_filename_component(BYJY_TEST_RENDERER_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_TEST_RENDERER_OUT_LIB "TestRenderer")
set(BYJY_TEST_RENDERER_OUT_LIBDIR "${BYJY_TEST_RENDERER_ROOT}")
set(BYJY_TEST_RENDERER_OUT_INC "${BYJY_TEST_RENDERER_ROOT}/../..")
set(BYJY_TEST_RENDERER_OUT_DEF "")
set(BYJY_TEST_RENDERER_OUT_LINK "")
set(BYJY_TEST_RENDERER_OUT_SYS "")
