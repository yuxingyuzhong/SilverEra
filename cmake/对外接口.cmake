# =============================================================================
# Game —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Application/Game/cmake/对外接口.cmake
#
# 作用
#     声明「本层向上层提供什么」。Game 目前是拓扑最上层，还没有使用方；
#     本文件先把边界写清楚，将来有层要接 GameCore 时不必再补。
#
# 契约（变量名与含义是层间契约的一部分，改名必须同步上层）
#     BYJY_<P>_OUT_LIB    本层静态库名（多个用分号分隔）；不产出库时留空
#     BYJY_<P>_OUT_LIBDIR 与 OUT_LIB 一一对应的「该库所在层目录」
#     BYJY_<P>_OUT_INC    使用本层公共头文件所需的包含目录
#     BYJY_<P>_OUT_DEF    使用本层公共头文件所需的编译定义
#     BYJY_<P>_OUT_LINK   本层对外传递的第三方库（文件路径）
#     BYJY_<P>_OUT_SYS    本层对外传递的系统库
#     本层前缀 <P> = GAME
#
# 并入下层
#     本层依赖 Engine 聚合层，因此本层对外面 = 本层自有面 + Engine 对外面。
#
# 备注：GameCore 只在 src/ 下有源文件时才产出（当前尚未产出库）；
#       本层不产出可执行文件，可执行文件只在 Test 层产生（EngineTests.exe）。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../../Engine/cmake/对外接口.cmake")

get_filename_component(BYJY_GAME_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_GAME_OUT_LIB "GameCore")
set(BYJY_GAME_OUT_LIBDIR "${BYJY_GAME_ROOT}")

# 本层自有包含目录由「项目根」统一承担（Application/Game/... 全路径），
# 它已在 Engine 对外面的首项里，故此处只承接即可。
set(BYJY_GAME_OUT_INC
    ${BYJY_ENGINE_OUT_INC}
)

set(BYJY_GAME_OUT_DEF  ${BYJY_ENGINE_OUT_DEF})
set(BYJY_GAME_OUT_LINK ${BYJY_ENGINE_OUT_LINK})
set(BYJY_GAME_OUT_SYS  ${BYJY_ENGINE_OUT_SYS})