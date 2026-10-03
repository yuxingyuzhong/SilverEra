# =============================================================================
# EngineCore —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Engine/EngineCore/cmake/对外接口.cmake
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
#     本层前缀 <P> = ENGINE_CORE
#
# 包含目录为什么首项是「项目根」而不是本层根
#     项目内部一律写「项目根相对全路径」：引擎头以 Engine/EngineCore/... 形式引用。
#     本层自身编译同样按此写法（私有包含根也是项目根），从而彻底消除
#     「直接 src/... 包含」带来的路径归属不明与文件重名。
#
# 其余条目
#     external/Json、external/glfw、external/bullet3/src：本层预编译头
#     common/前置头文件包含.h 内部 include 了 <nlohmann/json.hpp> 与 <GLFW/glfw3.h>，
#     本层公共头（碰撞相关）引用了 bullet3 头，故这些路径必须随对外面传递。
#     原 glm 条目已移除（external/glm 目录已不存在且无引用）。
# =============================================================================

get_filename_component(BYJY_ENGINE_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# 项目根：Engine/EngineCore 上溯两级
get_filename_component(BYJY_ENGINE_CORE_PROJECT_ROOT "${BYJY_ENGINE_CORE_ROOT}/../.." ABSOLUTE)

set(BYJY_ENGINE_CORE_OUT_LIB "EngineCore")
set(BYJY_ENGINE_CORE_OUT_LIBDIR "${BYJY_ENGINE_CORE_ROOT}")

set(BYJY_ENGINE_CORE_OUT_INC
    "${BYJY_ENGINE_CORE_PROJECT_ROOT}"
    "${BYJY_ENGINE_CORE_ROOT}/external/Json"
    "${BYJY_ENGINE_CORE_ROOT}/external/glfw"
    "${BYJY_ENGINE_CORE_ROOT}/external"
    "${BYJY_ENGINE_CORE_ROOT}/external/bullet3/src"
)

set(BYJY_ENGINE_CORE_OUT_DEF
    GLFW_STATIC
)

set(BYJY_ENGINE_CORE_OUT_LINK
    "${BYJY_ENGINE_CORE_ROOT}/external/glfw/glfw3.lib"
)

set(BYJY_ENGINE_CORE_OUT_SYS
    opengl32
    user32
    gdi32
    shell32
)