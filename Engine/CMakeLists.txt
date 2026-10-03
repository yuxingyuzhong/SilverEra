# =============================================================================
# Engine —— 聚合层构建脚本
# =============================================================================
# 位置：白银纪元/Engine
# 角色：聚合层。不编源码、不产出静态库，只把 EngineSystem 与 EngineCore 两层
#       合并成「一个 Engine 面」（见 cmake/对外接口.cmake），让 Application 各层只接一层。
# 依赖：EngineSystem（其对外面已并入 EngineCore）—— 一律链接「已经构建好的」静态库
# 产物：无（不产出 Engine.lib；EngineSystem.lib 与 EngineCore.lib 由两个子层各自产出）
# 用法（本层独立配置，作为聚合面的自检）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先依次构建 Engine/EngineCore 与 Engine/EngineSystem。
# 说明：本层不导出头快照 —— 两个子层各自导出自己的头快照，Application 各层分别改指。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(Engine LANGUAGES C CXX)

# 强制使用 C++20 标准（与子层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录（项目根的下级）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# =============================================================================
# 1. 接入聚合面 —— 校验 EngineSystem.lib 与 EngineCore.lib 齐备
# =============================================================================
# 本层没有源码要编译，接入动作的作用是「自检」：读本层对外接口（它已并入
# EngineSystem 与 EngineCore 两层对外面），按 OUT_LIB 列表逐库定位并建立导入目标。
# 任一库缺失即 FATAL_ERROR，从而在配置期就暴露「子层没建好」。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE STRING
    "聚合层静态库路径（分号列表，顺序同 EngineSystem;EngineCore）；留空则自动探测")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/cmake/对外接口.cmake"
    "BYJY_ENGINE"
    "BYJY_ENGINE_LIB_PATH"
)

# =============================================================================
# 2. 聚合面就绪标记
# =============================================================================
# Application 各层直接读 cmake/对外接口.cmake，并不依赖本构建树；
# 该目标仅用于让本层构建有一个可见产物，便于确认聚合面可用。
add_custom_target(EngineAggregateCheck ALL
    COMMAND "${CMAKE_COMMAND}" -E echo "Engine 聚合面就绪：EngineSystem.lib + EngineCore.lib"
    COMMENT "Engine 聚合层自检"
    VERBATIM
)