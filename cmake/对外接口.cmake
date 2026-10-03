# =============================================================================
# EngineSystem —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Engine/EngineSystem/cmake/对外接口.cmake
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
#     本层前缀 <P> = ENGINE_SYSTEM
#
# 并入下层
#     本层依赖 EngineCore，因此本层对外面 = 本层自有面 + EngineCore 对外面。
#     并入方式就是下面这一句 include：它把 EngineCore 的 BYJY_ENGINE_CORE_OUT_*
#     赋值进来，本层再在自己的列表里直接引用。纯数据赋值，重复 include 无副作用。
#
# 包含目录
#     本层自有部分只有 Sol2 与 Lua：
#       本层公共头 src/entity/Entity/实体.h 与 src/effect/Effect/效应.h 对外暴露
#       LuaState（= sol::state），common/external/Sol2/ 下的包装头也直接用 sol:: 类型，
#       所以「使用本层公共头文件」必然需要这两条路径。
#     本层不再声明自身层根/src 两个条目：项目内部一律写「项目根相对全路径」
#       （Engine/EngineSystem/src/...），由 EngineCore 对外面首项「项目根」统一解析。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../EngineCore/cmake/对外接口.cmake")

get_filename_component(BYJY_ENGINE_SYSTEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_ENGINE_SYSTEM_OUT_LIB "EngineSystem")
set(BYJY_ENGINE_SYSTEM_OUT_LIBDIR "${BYJY_ENGINE_SYSTEM_ROOT}")

set(BYJY_ENGINE_SYSTEM_OUT_INC
    "${BYJY_ENGINE_SYSTEM_ROOT}/external/Sol2/include"
    "${BYJY_ENGINE_SYSTEM_ROOT}/external/Lua"
    ${BYJY_ENGINE_CORE_OUT_INC}
)

set(BYJY_ENGINE_SYSTEM_OUT_DEF  ${BYJY_ENGINE_CORE_OUT_DEF})
set(BYJY_ENGINE_SYSTEM_OUT_LINK ${BYJY_ENGINE_CORE_OUT_LINK})
set(BYJY_ENGINE_SYSTEM_OUT_SYS  ${BYJY_ENGINE_CORE_OUT_SYS})

# 备注：未列入的第三方目录
#     EngineSystem/external/ 下的 Dear_ImGui、glad、glfw、glm、stb 目前不被本层
#     SystemCore 的源文件直接引用（唯一直接引用 stb_image.h 的 src/gui/Config_Editor/
#     含独立 main()，已排除在静态库之外；glfw / glm 由 EngineCore 对外面提供同一份能力）。
#     将来把配置编辑器接进来时需要一起补上它的包含路径。