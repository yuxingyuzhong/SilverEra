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
#     本层依赖 EngineFoundation，因此本层对外面 = 本层自有面 + EngineFoundation 对外面。
#     并入方式就是下面这一句 include：它把 EngineFoundation 的 BYJY_ENGINE_FOUNDATION_OUT_*
#     赋值进来，本层再在自己的列表里直接引用。纯数据赋值，重复 include 无副作用。
#
# 包含目录
#     本层自有部分只有 Sol2、Lua 与 bullet3：
#       本层公共头 src/entity/Entity/实体.h 与 src/effect/Effect/效应.h 对外暴露
#       LuaState（= sol::state），common/external/Sol2/ 下的包装头也直接用 sol:: 类型，
#       所以「使用本层公共头文件」必然需要这两条路径。
#       实体/装配层若通过公共头暴露 btRigidBody / btCollisionShape 等类型，
#       上层也必须拿到 bullet3 的 include 根才能编译，故一并纳入对外面。
#     本层不再声明自身层根/src 两个条目：项目内部一律写「项目根相对全路径」
#       （Engine/EngineSystem/src/...），由 EngineFoundation 对外面首项「项目根」统一解析。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../EngineFoundation/cmake/对外接口.cmake")

get_filename_component(BYJY_ENGINE_SYSTEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_ENGINE_SYSTEM_OUT_LIB "EngineSystem")
set(BYJY_ENGINE_SYSTEM_OUT_LIBDIR "${BYJY_ENGINE_SYSTEM_ROOT}")

# ---------- bullet3 ---------- 碰撞检测库
# 位置：Engine/EngineSystem/external/bullet3/（本层 external/ 下）
# 注意：此处只声明 include 根，不写 target_* —— 本文件是纯数据声明，
#       真正落到目标上的动作由 CMakeLists.txt 的 target_include_directories 完成。
set(BULLET_ROOT "${BYJY_ENGINE_SYSTEM_ROOT}/external/bullet3/src")

set(BYJY_ENGINE_SYSTEM_OUT_INC
    "${BYJY_ENGINE_SYSTEM_ROOT}/external/Sol2/include"
    "${BYJY_ENGINE_SYSTEM_ROOT}/external/Lua"
    "${BULLET_ROOT}"
    ${BYJY_ENGINE_FOUNDATION_OUT_INC}
)

set(BYJY_ENGINE_SYSTEM_OUT_DEF  "${BYJY_ENGINE_FOUNDATION_OUT_DEF}")
# 注意：必须带引号。Foundation 的 OUT_LINK / OUT_SYS 为空串，若写成
# `set(X ${Y})`，CMake 在 Y 为空时会**取消定义** X，导致上层 byjy_jieru_xiaceng()
# 读不到契约变量（渲染层接入系统层对外面时即会踩到）。
set(BYJY_ENGINE_SYSTEM_OUT_LINK "${BYJY_ENGINE_FOUNDATION_OUT_LINK}")
# 本层不直接调用 Win32 窗口 / 图形 API；系统库随渲染层对外面传递，不在此重复声明
set(BYJY_ENGINE_SYSTEM_OUT_SYS "${BYJY_ENGINE_FOUNDATION_OUT_SYS}")

# 备注：本层 external/ 的现状
#     Dear_ImGui、glad、glfw、glm、stb 五份副本已确认无任何构建脚本与源码引用，
#     且与在用副本重复（ImGui 与 Test 层同源、glm / stb 与渲染层字节一致），已整体删除；
#     本层 external/ 现仅保留 bullet3、Lua、Sol2。
#     注意：glfw 不再由 EngineFoundation 对外面提供 —— EngineFoundation 已撤下全部 GLFW / OpenGL
#     条目；将来本层若需要窗口能力，须自行声明其包含根与 glfw3.lib。
#     另：external 下的第三方源码随 `external/*.{cpp,c}` 整目录编入 EngineSystem.lib ——
#     bullet3（LinearMath + BulletCollision）与 Lua（全部 .c）对内提供符号，对外仅暴露其包含根
#     （Sol2 为纯头库）。三者的包含根都必须进对外面：公共头 `实体.h` / `效应.h` 直接暴露
#     `LuaState`（= sol::state）。