# =============================================================================
# Engine —— 聚合层构建脚本
# =============================================================================
# 位置：白银纪元/Engine
# 角色：聚合层。不编源码、不产出静态库，只把 EngineSystem 与 EngineFoundation 两层
#       合并成「一个 Engine 面」（见 cmake/对外接口.cmake），让 Application 各层只接一层。
#       渲染层（EngineRenderer）**不并入聚合面**：它依赖 EngineSystem，由应用层在聚合面
#       之外单独链接（Test / Game 均显式列出 EngineRenderer.lib）。
# 依赖：EngineSystem（其对外面已并入 EngineFoundation）—— 一律链接「已经构建好的」静态库
# 产物：无（不产出 Engine.lib；EngineSystem.lib 与 EngineFoundation.lib 由两个子层各自产出）
# 用法（本层独立配置，作为聚合面的自检；Debug / Release 用两棵独立构建树）：
#     Debug  ：cmake -S <本目录> -B <本目录>/out/build/x64-Debug   -G Ninja -DCMAKE_BUILD_TYPE=Debug
#     Release：cmake -S <本目录> -B <本目录>/out/build/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
#     随后  ：cmake --build <对应构建树目录>
# 前置条件：先依次构建 Engine/EngineFoundation 与 Engine/EngineSystem。
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
# 1. 接入聚合面 —— 校验 EngineSystem.lib 与 EngineFoundation.lib 齐备
# =============================================================================
# 本层没有源码要编译，接入动作的作用是「自检」：读本层对外接口（它已并入
# EngineSystem 与 EngineFoundation 两层对外面），按 OUT_LIB 列表逐库定位并建立导入目标。
# 任一库缺失即 FATAL_ERROR，从而在配置期就暴露「子层没建好」。
#
# 版本解析与其余层同口径（cmake/版本工具.cmake）：
#   锁空 → 跟随各层最新构建树；锁非空 → 取 Engine/版本缓存 里对应的冻结版本。
# 说明：渲染层不在聚合面内，故这里只解析 System / Foundation 两层。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")
include("${PROJECT_ROOT_DIR}/EngineFoundation/cmake/版本工具.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE STRING
    "聚合层静态库路径（分号列表，顺序同 EngineSystem;EngineFoundation）；留空则按版本锁解析")

# 双配置解析（聚合面的两个库）：Debug / Release 各算一份。
if("${BYJY_ENGINE_LIB_PATH}" STREQUAL "")
    byjy_banben_jiexi_shuang(EngineSystem engine_system
        "${PROJECT_ROOT_DIR}/EngineSystem"
        BYJY_ENGINE_SYSTEM_LIB BYJY_ENGINE_SYSTEM_SNAPSHOT BYJY_ENGINE_SYSTEM_VERSION)
    byjy_banben_jiexi_shuang(EngineFoundation engine_foundation
        "${PROJECT_ROOT_DIR}/EngineFoundation"
        BYJY_ENGINE_FOUNDATION_LIB BYJY_ENGINE_FOUNDATION_SNAPSHOT BYJY_ENGINE_FOUNDATION_VERSION)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
        set(BYJY_ENGINE_LIB_PATH "${BYJY_ENGINE_SYSTEM_LIB_Release};${BYJY_ENGINE_FOUNDATION_LIB_Release}")
    else()
        set(BYJY_ENGINE_LIB_PATH "${BYJY_ENGINE_SYSTEM_LIB_Debug};${BYJY_ENGINE_FOUNDATION_LIB_Debug}")
    endif()
else()
    # 手工覆盖：两配置共用同一份库（列表顺序同 OUT_LIB）
    list(LENGTH BYJY_ENGINE_LIB_PATH _byjy_override_count)
    if(_byjy_override_count GREATER 1)
        list(GET BYJY_ENGINE_LIB_PATH 0 _byjy_sys_lib)
        list(GET BYJY_ENGINE_LIB_PATH 1 _byjy_core_lib)
        set(BYJY_ENGINE_SYSTEM_LIB_Debug "${_byjy_sys_lib}")
        set(BYJY_ENGINE_SYSTEM_LIB_Release "${_byjy_sys_lib}")
        set(BYJY_ENGINE_FOUNDATION_LIB_Debug "${_byjy_core_lib}")
        set(BYJY_ENGINE_FOUNDATION_LIB_Release "${_byjy_core_lib}")
    endif()
endif()

# 给导入函数提供分配置库路径（与 OUT_LIB 同构的分号列表）：锚点头按 $<CONFIG> 分别注入
set(BYJY_ENGINE_LIB_PATH_DEBUG
    "${BYJY_ENGINE_SYSTEM_LIB_Debug};${BYJY_ENGINE_FOUNDATION_LIB_Debug}")
set(BYJY_ENGINE_LIB_PATH_RELEASE
    "${BYJY_ENGINE_SYSTEM_LIB_Release};${BYJY_ENGINE_FOUNDATION_LIB_Release}")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/cmake/对外接口.cmake"
    "BYJY_ENGINE"
    "BYJY_ENGINE_LIB_PATH"
)

# 多配置：逐配置设置导入库位置（单配置生成器忽略这些属性）
foreach(_byjy_pair IN ITEMS "EngineSystem|BYJY_ENGINE_SYSTEM_LIB" "EngineFoundation|BYJY_ENGINE_FOUNDATION_LIB")
    string(REPLACE "|" ";" _byjy_parts "${_byjy_pair}")
    list(GET _byjy_parts 0 _byjy_target)
    list(GET _byjy_parts 1 _byjy_base)
    foreach(_byjy_cfg IN ITEMS DEBUG RELEASE)
        set(_byjy_cfg_lib "${${_byjy_base}_${_byjy_cfg}}")
        if(NOT "${_byjy_cfg_lib}" STREQUAL "" AND EXISTS "${_byjy_cfg_lib}")
            set_target_properties(${_byjy_target} PROPERTIES
                IMPORTED_LOCATION_${_byjy_cfg} "${_byjy_cfg_lib}")
        endif()
    endforeach()
    if(EXISTS "${${_byjy_base}_Release}")
        set_target_properties(${_byjy_target} PROPERTIES
            IMPORTED_LOCATION_RELWITHDEBINFO "${${_byjy_base}_Release}"
            IMPORTED_LOCATION_MINSIZEREL "${${_byjy_base}_Release}")
    endif()
endforeach()

# 全链一致性（逐配置）：System 构建时用的 Foundation 版本必须与本次解析一致
foreach(_byjy_cfg IN ITEMS Debug Release)
    byjy_banben_hedui("Engine" EngineSystem "${_byjy_cfg}")
endforeach()

# =============================================================================
# 2. 聚合面就绪标记
# =============================================================================
# Application 各层直接读 cmake/对外接口.cmake，并不依赖本构建树；
# 该目标仅用于让本层构建有一个可见产物，便于确认聚合面可用。
add_custom_target(EngineAggregateCheck ALL
    COMMAND "${CMAKE_COMMAND}" -E echo "Engine 聚合面就绪：EngineSystem.lib + EngineFoundation.lib"
    COMMENT "Engine 聚合层自检"
    VERBATIM
)