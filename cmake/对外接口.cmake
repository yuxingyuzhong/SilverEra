# =============================================================================
# EngineFoundation —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Engine/EngineFoundation/cmake/对外接口.cmake
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
#     本层前缀 <P> = ENGINE_FOUNDATION
#
# 包含目录为什么首项是「项目根」而不是本层根
#     项目内部一律写「项目根相对全路径」：引擎头以 Engine/EngineFoundation/... 形式引用。
#     本层自身编译同样按此写法（私有包含根也是项目根），从而彻底消除
#     「直接 src/... 包含」带来的路径归属不明与文件重名。
#
# 其余条目
#     external/Json：本层公共头与源文件直接使用 <nlohmann/json.hpp>，故随对外面传递。
#     注：不再导出 external/ 伞根 —— 本层与上层都只写 <nlohmann/json.hpp>，靠 external/Json
#         即可解析；伞根只会把「Json/...」这种无人使用的包含形式也放进来，属残留条目。
#     注：本层源码不使用 GLFW / OpenGL（窗口与输入已整体迁往渲染层），因此不再导出
#         external/glfw 包含根、GLFW_STATIC 定义与 glfw3.lib；bullet3 已整体迁至
#         EngineSystem（Engine/EngineSystem/external/bullet3），原 glm 条目亦随目录删除而移除。
# =============================================================================

get_filename_component(BYJY_ENGINE_FOUNDATION_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# 项目根：Engine/EngineFoundation 上溯两级
get_filename_component(BYJY_ENGINE_FOUNDATION_PROJECT_ROOT "${BYJY_ENGINE_FOUNDATION_ROOT}/../.." ABSOLUTE)

set(BYJY_ENGINE_FOUNDATION_OUT_LIB "EngineFoundation")
set(BYJY_ENGINE_FOUNDATION_OUT_LIBDIR "${BYJY_ENGINE_FOUNDATION_ROOT}")

set(BYJY_ENGINE_FOUNDATION_OUT_INC
    "${BYJY_ENGINE_FOUNDATION_PROJECT_ROOT}"
    "${BYJY_ENGINE_FOUNDATION_ROOT}/external/Json"
)

# 本层不再需要任何编译定义（原 GLFW_STATIC 已随 GLFW 一并撤下）
set(BYJY_ENGINE_FOUNDATION_OUT_DEF "")

# 本层不再对外传递第三方静态库（原 glfw3.lib 已撤下）
set(BYJY_ENGINE_FOUNDATION_OUT_LINK "")

# 本层不直接调用 Win32 窗口 / 图形 API（仅 kernel32 默认链接），故不再对外传递系统库；
# 窗口与图形所需的系统库由渲染层（编译 SDL3 的层）自行声明并对外提供。
set(BYJY_ENGINE_FOUNDATION_OUT_SYS "")