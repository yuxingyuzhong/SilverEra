# =============================================================================
# Engine —— 对外接口（聚合层自描述）
# -----------------------------------------------------------------------------
# 位置：Engine/cmake/对外接口.cmake
#
# 作用
#     Engine 是聚合层：它不编源码、不产出静态库，只把 EngineSystem 与 EngineCore
#     两层的能力合并成「一个 Engine 面」，让 Application 各层只接 Engine 一层。
#     上层通过本文件读取聚合后的对外面，据此建立 IMPORTED 静态库目标。
#
# 契约（变量名与含义是层间契约的一部分，改名必须同步上层）
#     BYJY_ENGINE_OUT_LIB    聚合转发出去的静态库名列表 = EngineSystem;EngineCore
#     BYJY_ENGINE_OUT_LIBDIR 与 OUT_LIB 一一对应的「该库所在层目录」（供自动探测）
#     BYJY_ENGINE_OUT_INC    聚合后的包含目录
#     BYJY_ENGINE_OUT_DEF    聚合后的编译定义
#     BYJY_ENGINE_OUT_LINK   聚合后的第三方库（文件路径）
#     BYJY_ENGINE_OUT_SYS    聚合后的系统库
#
# 并入方式
#     先 include EngineSystem 的对外接口（它又 include 了 EngineCore 的），
#     于是 BYJY_ENGINE_SYSTEM_* 与 BYJY_ENGINE_CORE_* 都已赋值；本文件再把它们
#     摊平成 BYJY_ENGINE_*。纯数据赋值，重复 include 无副作用。
#
# 为什么 OUT_LIB 是两个库
#     聚合层只做「面」的合并，不合并二进制产物：EngineSystem.lib 与 EngineCore.lib
#     仍各自独立产出，由上层一并链接（因此不生成 Engine.lib）。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../EngineSystem/cmake/对外接口.cmake")

get_filename_component(BYJY_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_ENGINE_OUT_LIB
    "${BYJY_ENGINE_SYSTEM_OUT_LIB};${BYJY_ENGINE_CORE_OUT_LIB}"
)
set(BYJY_ENGINE_OUT_LIBDIR
    "${BYJY_ENGINE_SYSTEM_ROOT};${BYJY_ENGINE_CORE_ROOT}"
)

set(BYJY_ENGINE_OUT_INC  ${BYJY_ENGINE_SYSTEM_OUT_INC})
set(BYJY_ENGINE_OUT_DEF  ${BYJY_ENGINE_SYSTEM_OUT_DEF})
set(BYJY_ENGINE_OUT_LINK ${BYJY_ENGINE_SYSTEM_OUT_LINK})
set(BYJY_ENGINE_OUT_SYS  ${BYJY_ENGINE_SYSTEM_OUT_SYS})

# 备注：聚合层本体不产出静态库，故 BYJY_ENGINE_ROOT 不进入包含目录列表；
#       Engine/ 下只有 EngineCore/ 与 EngineSystem/ 两个子层与 cmake/ 目录。