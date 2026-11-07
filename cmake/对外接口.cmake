# =============================================================================
# Test —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Application/Test/cmake/对外接口.cmake
#
# 作用
#     声明「本层向上层提供什么」。上层通过本文件读取本层对外面，据此建立
#     IMPORTED 静态库目标（不使用 add_subdirectory 回退编源码）。
#
# 契约（变量名与含义是层间契约的一部分，改名必须同步上层）
#     BYJY_<P>_OUT_LIB    本层静态库名（多个用分号分隔）；不产出库时留空
#     BYJY_<P>_OUT_LIBDIR 与 OUT_LIB 一一对应的「该库所在层目录」
#     BYJY_<P>_OUT_INC    使用本层公共头文件所需的包含目录
#     BYJY_<P>_OUT_DEF    使用本层公共头文件所需的编译定义
#     BYJY_<P>_OUT_LINK   本层对外传递的第三方库（文件路径）
#     BYJY_<P>_OUT_SYS    本层对外传递的系统库
#     本层前缀 <P> = TEST
#
# 并入下层
#     本层依赖 Engine 聚合层，因此本层对外面 = 本层自有面 + Engine 对外面
#     （后者又已并入 EngineSystem 与 EngineFoundation 对外面）。
#     注意：渲染层是**本层另外单独接入**的（与聚合面并列），不在本对外面内 ——
#     它的窗口 / 图形依赖不应强加给只做逻辑的消费方。
#
# 备注：本层是拓扑最上层，当前**没有使用方**，本文件仅作自描述。
#       TestCore 是条件目标（src/ 根下暂无源文件时不会产出），故 OUT_LIB 留空；
#       可执行产物 EngineTests.exe 也从不写进对外面。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../../Engine/cmake/对外接口.cmake")

get_filename_component(BYJY_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# 本层当前不产出可被链接的静态库
set(BYJY_TEST_OUT_LIB "")
set(BYJY_TEST_OUT_LIBDIR "")

# 本层自有包含目录由「项目根」统一承担（Application/Test/... 全路径），
# 它已在 Engine 对外面的首项里，故此处只承接即可。
set(BYJY_TEST_OUT_INC
    ${BYJY_ENGINE_OUT_INC}
)

# 注意：必须带引号。Engine 的 OUT_DEF / OUT_LINK / OUT_SYS 可能为空串，
# 写成 set(X ${Y}) 时 CMake 会在 Y 为空时**取消定义** X（上层读不到契约变量）。
set(BYJY_TEST_OUT_DEF  "${BYJY_ENGINE_OUT_DEF}")
set(BYJY_TEST_OUT_LINK "${BYJY_ENGINE_OUT_LINK}")
set(BYJY_TEST_OUT_SYS  "${BYJY_ENGINE_OUT_SYS}")