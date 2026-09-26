# =============================================================================
# 引擎层 —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：引擎层/cmake/对外接口.cmake
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
#     本层前缀 <P> = ENGINE
#
# 说明
#     引擎层是拓扑最底层，没有下层可并入，因此本文件只描述本层自身。
#
#     包含目录里除引擎层根目录外，还必须带上 external/Json 与 external/glfw：
#     引擎层的预编译头 common/前置头文件包含.h 内部 include 了
#     <nlohmann/json.hpp> 与 <GLFW/glfw3.h>，上层只要 include 这个预编译头
#     就会用到这两条路径。
#
#     glm 要把 external/glm 本身带上：src/core/spatial/common/几何体类型.h
#     里写的是 <glm.hpp>，只给 external 这一层是解析不到的（引擎层自身构建
#     时同样把 GLM_ROOT 加进了包含路径）。空间划分与碰撞相关的公共头都会
#     带进这个头，上层只要 include 四叉树一类的头文件就会用到。
#
#     external 目录本身也保留在列表里（原有声明，未改动）。
#
#     bullet3 的头文件由引擎层 src/core/spatial/collision 下的公共头引用，
#     上层若包含这些头同样需要，故一并声明。
#
#     引擎层/CMakeLists.txt 里这些包含路径是 PRIVATE 的，本文件是按「上层
#     实际需要什么」重新声明的对外面，两侧出现差异时以本文件为准。
# =============================================================================

get_filename_component(BYJY_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_ENGINE_OUT_LIB "EngineCore")

set(BYJY_ENGINE_OUT_INC
    "${BYJY_ENGINE_ROOT}"
    "${BYJY_ENGINE_ROOT}/external/Json"
    "${BYJY_ENGINE_ROOT}/external/glfw"
    "${BYJY_ENGINE_ROOT}/external/glm"
    "${BYJY_ENGINE_ROOT}/external"
    "${BYJY_ENGINE_ROOT}/external/bullet3/src"
)

set(BYJY_ENGINE_OUT_DEF
    GLFW_STATIC
)

set(BYJY_ENGINE_OUT_LINK
    "${BYJY_ENGINE_ROOT}/external/glfw/glfw3.lib"
)

set(BYJY_ENGINE_OUT_SYS
    opengl32
    user32
    gdi32
    shell32
)
