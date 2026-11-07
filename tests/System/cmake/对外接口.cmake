# =============================================================================
# tests/System —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Application/Test/tests/System/cmake/对外接口.cmake
#
# 契约（与各引擎层的对外接口同名同义）
#     BYJY_TEST_SYSTEM_OUT_LIB     TestSystem
#     BYJY_TEST_SYSTEM_OUT_LIBDIR  本子工程根
#     BYJY_TEST_SYSTEM_OUT_INC     Test 层根
#     BYJY_TEST_SYSTEM_OUT_DEF / _LINK / _SYS  空
#
# 说明：静态库编译期的 EngineSystem / EngineFoundation / gtest 依赖由子工程自身
#       构建脚本闭合；主层只需整体收编本库（WHOLE_ARCHIVE）。
# =============================================================================

get_filename_component(BYJY_TEST_SYSTEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_TEST_SYSTEM_OUT_LIB "TestSystem")
set(BYJY_TEST_SYSTEM_OUT_LIBDIR "${BYJY_TEST_SYSTEM_ROOT}")
set(BYJY_TEST_SYSTEM_OUT_INC "${BYJY_TEST_SYSTEM_ROOT}/../..")
set(BYJY_TEST_SYSTEM_OUT_DEF "")
set(BYJY_TEST_SYSTEM_OUT_LINK "")
set(BYJY_TEST_SYSTEM_OUT_SYS "")
