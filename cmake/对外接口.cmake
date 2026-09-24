# =============================================================================
# 系统层 —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：系统层/cmake/对外接口.cmake
#
# 作用
#     声明「本层向上层提供什么」。上层通过本文件读取本层对外面，据此建立
#     IMPORTED 静态库目标（不使用 add_subdirectory 回退编源码）。
#
# 契约（变量名与含义是层间契约的一部分，改名必须同步上层）
#     BYJY_<P>_OUT_LIB    本层静态库文件名（不含扩展名）；叶层无库时留空
#     BYJY_<P>_OUT_INC    使用本层公共头文件所需的包含目录
#     BYJY_<P>_OUT_DEF    使用本层公共头文件所需的编译定义
#     BYJY_<P>_OUT_LINK   本层对外传递的第三方库（文件路径）
#     BYJY_<P>_OUT_SYS    本层对外传递的系统库
#     本层前缀 <P> = SYSTEM
#
# 并入下层
#     系统层依赖引擎层，因此本层对外面 = 本层自有面 + 引擎层对外面。
#     并入方式就是下面这一句 include：它把引擎层的 BYJY_ENGINE_OUT_* 赋值进来，
#     本层再在自己的列表里直接引用。纯数据赋值，被重复 include 无副作用。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../引擎层/cmake/对外接口.cmake")

get_filename_component(BYJY_SYSTEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_SYSTEM_OUT_LIB "SystemCore")

# 本层自有包含目录
#     根目录：层内以 "entity/..." 形式互相包含
#     src/ ：宿主与上层以 "entity/Entity/实体.h" 形式引用
# 之后接引擎层对外面：本层公共头文件会引用
#     "common/前置头文件包含.h"、"src/core/..."、"src/tools/..." 等引擎层头文件
set(BYJY_SYSTEM_OUT_INC
    "${BYJY_SYSTEM_ROOT}"
    "${BYJY_SYSTEM_ROOT}/src"
    "${BYJY_SYSTEM_ROOT}/external/Sol2/include"
    "${BYJY_SYSTEM_ROOT}/external/Lua"
    ${BYJY_ENGINE_OUT_INC}
)

set(BYJY_SYSTEM_OUT_DEF  ${BYJY_ENGINE_OUT_DEF})
set(BYJY_SYSTEM_OUT_LINK ${BYJY_ENGINE_OUT_LINK})
set(BYJY_SYSTEM_OUT_SYS  ${BYJY_ENGINE_OUT_SYS})

# 备注一：Sol2 / Lua 为什么必须在对外包含目录里
#       本层的公共头文件 src/entity/Entity/实体.h 与 src/effect/Effect/效应.h
#       对外暴露 LuaState（= sol::state），common/external/Sol2/ 下的包装头文件
#       也直接用 sol:: 类型，所以「使用本层公共头文件」必然需要这两条路径。
#       SystemCore 自身编译同样依赖它们，用的是同一份声明。
#
# 备注二：未列入的第三方目录
#       系统层/external/ 下的 Dear_ImGui、glad、glfw、glm、stb 目前不被 SystemCore
#       的源文件直接引用（唯一直接引用 stb_image.h 的 src/gui/Config_Editor/ 含
#       独立 main()，已排除在静态库之外；glfw / glm 由引擎层对外面提供同一份能力）。
#       将来把配置编辑器接进来时需要一起补上它的包含路径。
