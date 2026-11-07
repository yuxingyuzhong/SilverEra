# =============================================================================
# Test —— 构建脚本
# =============================================================================
# 位置：白银纪元/Application/Test
# 依赖：Engine 聚合层（对外转发 EngineSystem.lib + EngineFoundation.lib）—— 一律链接「已经构建好的」静态库
#       渲染层 EngineRenderer.lib（图形操作界面经其 UI 通道绘制；本层不引入 Vulkan/SDL 类型到用例）
# 产物：TestCore 静态库（src/ 下的功能测试器集合，当前为空）
#       TestLauncher 静态库（src/主调/ 下的测试选择模型、控制台菜单）
#       TestGui 静态库（external/Dear_ImGui 核心 + SDL3/Vulkan 后端，供图形操作界面使用）
#       EngineTests.exe（本层唯一可执行文件，main 由 src/主调/主程序.cpp 提供）
# 测试用例：按被测层拆到独立子工程（tests/Foundation、tests/System、tests/Renderer），
#           各自产出 Test<Layer>.lib（用例 + 注册锚）与 Test<Layer>.exe（薄入口）；
#           本层只整体收编三个静态库（WHOLE_ARCHIVE），不再直接编译用例。
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先按依赖顺序构建 Engine/EngineFoundation → Engine/EngineSystem → Engine/EngineRenderer，
#           再构建三个测试子工程（各层测试库缺失时本层配置期即报错并给出构建命令）。
#           本层不拉入任何下层源码，也没有源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 包含路径：项目内部一律写「项目根相对全路径」——
#           引擎头   Engine/EngineFoundation/...、Engine/EngineSystem/...、Engine/EngineRenderer/...
#           本层自身 Application/Test/...
#           编译包含根 = [下层头快照根, ..., 项目根]（快照在前，见第 1.5 节，次序不可颠倒）
# 层间契约：
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，EngineSystem / EngineRenderer / Engine / Test 逐字节相同
# 说明：
#     - 本层不再产出分层宿主（引擎宿主 / 系统宿主 / 全栈宿主）。宿主机制取消后，
#       可执行文件只在测试层产生：EngineTests.exe 既是单元测试入口，也是启动项。
#     - 本层自有的用例（main 调逻辑自测）留在 src/单元测试/Test/ 下，用 OBJECT 库收集：
#       静态库里未被引用的 .obj 会被链接器丢弃，TEST 宏的静态注册会随之消失。
#     - 分层用例（EngineFoundation / EngineSystem / Renderer）由独立子工程产出静态库，
#       本层用 WHOLE_ARCHIVE 整体收编 —— 原因同上，跨构建树也无法用 OBJECT 库。
# =============================================================================
cmake_minimum_required(VERSION 3.24)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(Test LANGUAGES C CXX)

# 强制使用 C++20 标准（与下层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录；项目根供 Application/Test/... 全路径解析
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${PROJECT_ROOT_DIR}/../.." ABSOLUTE)

# =============================================================================
# 0.5 链接响应文件 BOM 包装器（MSVC + Ninja 专用）
# =============================================================================
# 背景：Ninja 写出的链接响应文件无 BOM，link.exe 按 ANSI 解读中文对象名，
#       链接阶段会报 LNK1104（对象文件名乱码）。
# 做法：用 CMAKE_CXX_LINKER_LAUNCHER 前置一个脚本，给 @*.rsp 补 BOM 后再调用
#       真实链接器；仅 Ninja 生成器生效。
# =============================================================================
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja" AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    set(BYJY_LINK_WRAPPER "${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/link_rsp_bom.ps1")
    find_program(BYJY_POWERSHELL_EXECUTABLE NAMES powershell powershell.exe)
    if(EXISTS "${BYJY_LINK_WRAPPER}" AND BYJY_POWERSHELL_EXECUTABLE)
        set(CMAKE_CXX_LINKER_LAUNCHER
            "${BYJY_POWERSHELL_EXECUTABLE};-NoProfile;-ExecutionPolicy;Bypass;-File;${BYJY_LINK_WRAPPER}")
        message(STATUS "链接响应文件 BOM 包装器已启用：${BYJY_LINK_WRAPPER}")
    else()
        message(WARNING "未找到链接包装器或 PowerShell，中文对象名的链接可能报 LNK1104：${BYJY_LINK_WRAPPER}")
    endif()
endif()

# =============================================================================
# 1. 接入 Engine 聚合层 —— 版本解析（版本锁）＋ 链接已构建好的静态库
# =============================================================================
# 本层只消费下层「已经构建好的」静态库：下层源码不进入本次构建，也没有源码回退路径。
# Engine 聚合层的对外面转发 EngineSystem.lib + EngineFoundation.lib 两个库，故这里只接一次。
#
# 版本解析（cmake/版本工具.cmake）：
#   ① 覆盖变量 BYJY_ENGINE_LIB_PATH 非空（须与库数等长的分号列表）→ 用它
#   ② 否则读 Engine/版本锁.cmake，逐层解析：
#        锁空   → 跟随最新：<层>/out/build/x64-<当前配置>
#        锁非空 → 锁定版本：Engine/版本缓存/<层>/x64-<配置>/<版本哈希>
#   ③ 锁指定但缓存缺失 → FATAL_ERROR
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")
include("${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/版本工具.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE STRING
    "Engine 聚合层静态库路径（分号列表，顺序同 EngineSystem;EngineFoundation）；留空则按版本锁解析")

# 双配置解析（聚合面的两个库）：Debug / Release 各算一份。多配置生成器（VS）下由
# IMPORTED_LOCATION_<CONFIG> 与 $<CONFIG> 包含根分别选取；单配置生成器只用当前配置那份。
if("${BYJY_ENGINE_LIB_PATH}" STREQUAL "")
    byjy_banben_jiexi_shuang(EngineSystem engine_system
        "${BYJY_PROJECT_ROOT}/Engine/EngineSystem"
        BYJY_ENGINE_SYSTEM_LIB BYJY_ENGINE_SYSTEM_SNAPSHOT BYJY_ENGINE_SYSTEM_VERSION)
    byjy_banben_jiexi_shuang(EngineFoundation engine_foundation
        "${BYJY_PROJECT_ROOT}/Engine/EngineFoundation"
        BYJY_ENGINE_FOUNDATION_LIB BYJY_ENGINE_FOUNDATION_SNAPSHOT BYJY_ENGINE_FOUNDATION_VERSION)
    # 列表顺序必须与 Engine 对外面的 OUT_LIB 一致：EngineSystem;EngineFoundation
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
        set(BYJY_ENGINE_LIB_PATH "${BYJY_ENGINE_SYSTEM_LIB_Release};${BYJY_ENGINE_FOUNDATION_LIB_Release}")
    else()
        set(BYJY_ENGINE_LIB_PATH "${BYJY_ENGINE_SYSTEM_LIB_Debug};${BYJY_ENGINE_FOUNDATION_LIB_Debug}")
    endif()
else()
    # 手工覆盖：两配置共用同一份库（列表顺序同上）
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
# 分配置头快照根（同构列表）：VS 构建树的布局与 Ninja 不同，锚点不能靠库路径反推
set(BYJY_ENGINE_LIB_PATH_INCLUDE_DEBUG
    "${BYJY_ENGINE_SYSTEM_SNAPSHOT_Debug};${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug}")
set(BYJY_ENGINE_LIB_PATH_INCLUDE_RELEASE
    "${BYJY_ENGINE_SYSTEM_SNAPSHOT_Release};${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Release}")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../../Engine/cmake/对外接口.cmake"
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

# 手工覆盖 / 仅解析出单配置时：快照根由导入目标反推，并补齐成双配置同一份
foreach(_byjy_pair IN ITEMS "EngineFoundation|BYJY_ENGINE_FOUNDATION_SNAPSHOT" "EngineSystem|BYJY_ENGINE_SYSTEM_SNAPSHOT")
    string(REPLACE "|" ";" _byjy_parts "${_byjy_pair}")
    list(GET _byjy_parts 0 _byjy_target)
    list(GET _byjy_parts 1 _byjy_base)
    if(NOT DEFINED ${_byjy_base}_Debug OR "${${_byjy_base}_Debug}" STREQUAL "")
        byjy_tou_kuaizhao_gen(${_byjy_base}_Debug ${_byjy_target})
    endif()
    if(NOT DEFINED ${_byjy_base}_Release OR "${${_byjy_base}_Release}" STREQUAL "")
        set(${_byjy_base}_Release "${${_byjy_base}_Debug}")
    endif()
    set(${_byjy_base}
        "$<IF:$<CONFIG:Debug>,${${_byjy_base}_Debug},${${_byjy_base}_Release}>")
endforeach()

# 全链一致性（逐配置）：System 构建时用的 Foundation 版本必须与本次解析一致
foreach(_byjy_cfg IN ITEMS Debug Release)
    byjy_banben_hedui("Test" EngineSystem "${_byjy_cfg}")
endforeach()

# =============================================================================
# 1.2 接入渲染层 —— 版本解析（版本锁）＋ 链接已构建好的 EngineRenderer.lib
# =============================================================================
# 渲染层公共头不暴露 SDL / Vulkan / VMA 类型，本层按公共面（渲染器.h、网格数据.h）
# 编写用例即可，无需、也不应引入 Vulkan 头。
set(BYJY_RENDERER_LIB_PATH "" CACHE STRING
    "渲染层静态库路径（单库 EngineRenderer）；留空则按版本锁解析")

if("${BYJY_RENDERER_LIB_PATH}" STREQUAL "")
    byjy_banben_jiexi_shuang(EngineRenderer renderer
        "${BYJY_PROJECT_ROOT}/Engine/EngineRenderer"
        BYJY_RENDERER_LIB BYJY_RENDERER_SNAPSHOT BYJY_RENDERER_VERSION)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
        set(BYJY_RENDERER_LIB_PATH "${BYJY_RENDERER_LIB_Release}")
    else()
        set(BYJY_RENDERER_LIB_PATH "${BYJY_RENDERER_LIB_Debug}")
    endif()
else()
    # 手工覆盖：两配置共用同一份库
    set(BYJY_RENDERER_LIB_Debug "${BYJY_RENDERER_LIB_PATH}")
    set(BYJY_RENDERER_LIB_Release "${BYJY_RENDERER_LIB_PATH}")
endif()

set(BYJY_RENDERER_LIB_PATH_DEBUG   "${BYJY_RENDERER_LIB_Debug}")
set(BYJY_RENDERER_LIB_PATH_RELEASE "${BYJY_RENDERER_LIB_Release}")
# 分配置头快照根：VS 构建树布局与 Ninja 不同，锚点不能靠库路径反推
set(BYJY_RENDERER_LIB_PATH_INCLUDE_DEBUG   "${BYJY_RENDERER_SNAPSHOT_Debug}")
set(BYJY_RENDERER_LIB_PATH_INCLUDE_RELEASE "${BYJY_RENDERER_SNAPSHOT_Release}")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../../Engine/EngineRenderer/cmake/对外接口.cmake"
    "BYJY_RENDERER"
    "BYJY_RENDERER_LIB_PATH"
)

# 多配置：逐配置设置导入库位置（单配置生成器忽略这些属性）
foreach(_byjy_cfg IN ITEMS DEBUG RELEASE)
    set(_byjy_cfg_lib "${BYJY_RENDERER_LIB_${_byjy_cfg}}")
    if(NOT "${_byjy_cfg_lib}" STREQUAL "" AND EXISTS "${_byjy_cfg_lib}")
        set_target_properties(EngineRenderer PROPERTIES
            IMPORTED_LOCATION_${_byjy_cfg} "${_byjy_cfg_lib}")
    endif()
endforeach()
if(EXISTS "${BYJY_RENDERER_LIB_Release}")
    set_target_properties(EngineRenderer PROPERTIES
        IMPORTED_LOCATION_RELWITHDEBINFO "${BYJY_RENDERER_LIB_Release}"
        IMPORTED_LOCATION_MINSIZEREL "${BYJY_RENDERER_LIB_Release}")
endif()

# 手工覆盖 / 仅解析出单配置时：快照根由导入目标反推，并补齐成双配置同一份
if(NOT DEFINED BYJY_RENDERER_SNAPSHOT_Debug OR "${BYJY_RENDERER_SNAPSHOT_Debug}" STREQUAL "")
    byjy_tou_kuaizhao_gen(BYJY_RENDERER_SNAPSHOT_Debug EngineRenderer)
endif()
if(NOT DEFINED BYJY_RENDERER_SNAPSHOT_Release OR "${BYJY_RENDERER_SNAPSHOT_Release}" STREQUAL "")
    set(BYJY_RENDERER_SNAPSHOT_Release "${BYJY_RENDERER_SNAPSHOT_Debug}")
endif()
set(BYJY_RENDERER_SNAPSHOT
    "$<IF:$<CONFIG:Debug>,${BYJY_RENDERER_SNAPSHOT_Debug},${BYJY_RENDERER_SNAPSHOT_Release}>")

# 全链一致性（逐配置）：Renderer 构建时用的 Foundation / System 版本必须与本次解析一致
foreach(_byjy_cfg IN ITEMS Debug Release)
    byjy_banben_hedui("Test" EngineRenderer "${_byjy_cfg}")
endforeach()

# =============================================================================
# 1.5 下层头快照接入（本层专用）
# =============================================================================
# 本层按「与下层静态库同版次」的头文件编译：把三个子层的头快照根作为「本层自身的
# 包含目录」列出，并排在项目根之前 —— 目标自身的 -I 恒在导入目标传递来的 /external:I
# 之前，因此 Engine/... 必然先命中快照，不会落到源码树上的同名头。
# 快照根已由第 1 / 1.2 节的版本解析给出（锁空 = 构建树快照，锁非空 = 版本缓存快照）。
# 目录缺失即硬失败（不静默退回源码树）。
# =============================================================================
set(BYJY_CORE_SNAPSHOT   "${BYJY_ENGINE_FOUNDATION_SNAPSHOT}")
set(BYJY_SYSTEM_SNAPSHOT "${BYJY_ENGINE_SYSTEM_SNAPSHOT}")
# BYJY_RENDERER_SNAPSHOT 已在第 1.2 节设定（按 $<CONFIG> 选取）

# 双配置逐份校验：目录缺失即硬失败（多配置生成器）；单配置下缺哪棵下层树就跳过哪份。
# 同时做头完整性兜底：本层源码引用的下层头必须都在快照里。
foreach(_byjy_cfg IN ITEMS Debug Release)
    foreach(_byjy_pair IN ITEMS
            "EngineFoundation|BYJY_ENGINE_FOUNDATION_SNAPSHOT"
            "EngineSystem|BYJY_ENGINE_SYSTEM_SNAPSHOT"
            "EngineRenderer|BYJY_RENDERER_SNAPSHOT")
        string(REPLACE "|" ";" _byjy_parts "${_byjy_pair}")
        list(GET _byjy_parts 0 _byjy_layer)
        list(GET _byjy_parts 1 _byjy_base)
        set(_byjy_snap "${${_byjy_base}_${_byjy_cfg}}")
        if(NOT IS_DIRECTORY "${_byjy_snap}")
            if(CMAKE_CONFIGURATION_TYPES)
                message(FATAL_ERROR
                    "Test：${_byjy_layer} 的 ${_byjy_cfg} 头快照目录不存在：${_byjy_snap}\n"
                    "  多配置生成器要求三套快照齐备；请先构建 ${_byjy_layer} 的 ${_byjy_cfg} 树。")
            endif()
            continue()
        endif()
        byjy_jiaoyan_xiacengtou("Test(${_byjy_cfg})" "${_byjy_snap}" "${_byjy_layer}"
            "${PROJECT_ROOT_DIR}/src" "${PROJECT_ROOT_DIR}/common")
    endforeach()
endforeach()

# 本层各目标的包含根：下层快照在前、项目根在后（供「头文件」写 Application/Test/... 全路径）。
# 渲染层快照排在项目根之前，使 Engine/EngineRenderer/... 先命中快照、不落到源码树。
# 末位补本层根：本层实现文件（.cpp）的跨目录包含写「本层根相对」src/...，无需背项目根前缀。
set(BYJY_APP_INC
    "${BYJY_CORE_SNAPSHOT}"
    "${BYJY_SYSTEM_SNAPSHOT}"
    "${BYJY_RENDERER_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
    "${PROJECT_ROOT_DIR}"
)

# =============================================================================
# 1.6 渲染层着色器拷贝
# =============================================================================
# 渲染层以 SPIR-V 在运行期加载着色器：Renderer_Config::shader_dir 为空时按
# 「相对可执行文件目录的 shaders」解析。着色器与 EngineRenderer.lib 同版次，
# 故从其 IMPORTED_LOCATION 同源推导着色器目录，配置期拷到本层根目录 shaders/。
get_target_property(_BYJY_RENDERER_LIB_FILE EngineRenderer IMPORTED_LOCATION)
# Ninja 布局：<树>/lib/EngineRenderer.lib → 上溯两级；VS 布局：<树>/lib/<配置>/ → 上溯三级
get_filename_component(_byjy_shader_root_a "${_BYJY_RENDERER_LIB_FILE}/../.." ABSOLUTE)
get_filename_component(_byjy_shader_root_b "${_BYJY_RENDERER_LIB_FILE}/../../.." ABSOLUTE)
if(IS_DIRECTORY "${_byjy_shader_root_b}/shaders")
    set(BYJY_RENDERER_SHADERS "${_byjy_shader_root_b}/shaders")
else()
    set(BYJY_RENDERER_SHADERS "${_byjy_shader_root_a}/shaders")
endif()
if(IS_DIRECTORY "${BYJY_RENDERER_SHADERS}")
    file(COPY "${BYJY_RENDERER_SHADERS}" DESTINATION "${PROJECT_ROOT_DIR}")
    message(STATUS "渲染层着色器已拷贝：${BYJY_RENDERER_SHADERS} → ${PROJECT_ROOT_DIR}/shaders")
else()
    message(WARNING "未找到渲染层着色器目录：${BYJY_RENDERER_SHADERS}（渲染层用例需要它，请先构建渲染层）")
endif()

# =============================================================================
# 1.7 接入分层测试子工程（tests/Foundation、tests/System、tests/Renderer）
# =============================================================================
# 每个子工程都是独立的 CMake 工程（各自的 out/build/x64-<配置>），只读同一份
# Engine/版本锁.cmake。本层不编它们的源码，只按对外接口契约定位已构建好的静态库：
#     <子工程>/out/build/x64-<配置>/lib/Test<层>.lib
# 链接时用 WHOLE_ARCHIVE 整体收编：静态库里未被引用的 .obj 会被链接器丢弃，
# TEST 宏的静态注册随之消失（与 EngineTestObjects 用 OBJECT 库是同一个原因）。
foreach(_byjy_pair IN ITEMS
        "Foundation|BYJY_TEST_FOUNDATION"
        "System|BYJY_TEST_SYSTEM"
        "Renderer|BYJY_TEST_RENDERER")
    string(REPLACE "|" ";" _byjy_parts "${_byjy_pair}")
    list(GET _byjy_parts 0 _byjy_sub)
    list(GET _byjy_parts 1 _byjy_prefix)

    # 先在本作用域读入对外接口：byjy_jieru_xiaceng() 内部的 include 只作用于函数作用域，
    # 函数返回后 OUT_LIB / OUT_LIBDIR 就取不到了，故此处显式再 include 一次（纯数据赋值）。
    include("${PROJECT_ROOT_DIR}/tests/${_byjy_sub}/cmake/对外接口.cmake")

    byjy_jieru_xiaceng(
        "${PROJECT_ROOT_DIR}/tests/${_byjy_sub}/cmake/对外接口.cmake"
        "${_byjy_prefix}"
        "${_byjy_prefix}_LIB_PATH")

    # 逐配置覆盖导入库位置（多配置生成器需要 Debug / Release 两份都齐）
    set(_byjy_lib_name "${${_byjy_prefix}_OUT_LIB}")
    set(_byjy_lib_dir "${${_byjy_prefix}_OUT_LIBDIR}")
    if("${_byjy_lib_name}" STREQUAL "")
        message(FATAL_ERROR
            "Test：测试子工程 ${_byjy_sub} 的对外接口未声明 OUT_LIB（${_byjy_prefix}_OUT_LIB）")
    endif()
    set(_byjy_found_configs "")
    foreach(_byjy_cfg IN ITEMS DEBUG RELEASE)
        set(_byjy_lib "${_byjy_lib_dir}/out/build/x64-${_byjy_cfg}/lib/${_byjy_lib_name}.lib")
        if(EXISTS "${_byjy_lib}")
            set_target_properties(${_byjy_lib_name} PROPERTIES
                IMPORTED_LOCATION_${_byjy_cfg} "${_byjy_lib}")
            list(APPEND _byjy_found_configs "${_byjy_cfg}")
        elseif(CMAKE_CONFIGURATION_TYPES)
            message(FATAL_ERROR
                "Test：找不到测试子工程 ${_byjy_sub} 的 ${_byjy_cfg} 静态库：${_byjy_lib}\n"
                "  多配置生成器要求两套测试库齐备。请先构建：\n"
                "      cmake -S \"${_byjy_lib_dir}\" -B \"${_byjy_lib_dir}/out/build/x64-${_byjy_cfg}\" -G Ninja\n"
                "      cmake --build \"${_byjy_lib_dir}/out/build/x64-${_byjy_cfg}\"")
        endif()
    endforeach()
    # 声明导入目标实际提供的配置，多配置生成器才会按 $<CONFIG> 选中对应库
    if(_byjy_found_configs)
        set_target_properties(${_byjy_lib_name} PROPERTIES
            IMPORTED_CONFIGURATIONS "${_byjy_found_configs}")
    endif()
    if(EXISTS "${_byjy_lib_dir}/out/build/x64-Release/lib/${_byjy_lib_name}.lib")
        set_target_properties(${_byjy_lib_name} PROPERTIES
            IMPORTED_LOCATION_RELWITHDEBINFO "${_byjy_lib_dir}/out/build/x64-Release/lib/${_byjy_lib_name}.lib"
            IMPORTED_LOCATION_MINSIZEREL "${_byjy_lib_dir}/out/build/x64-Release/lib/${_byjy_lib_name}.lib")
    endif()

    # 三个测试库名收进列表，供 EngineTests 链接（顺序保持 Foundation → System → Renderer）
    list(APPEND BYJY_TEST_LIBS "${_byjy_lib_name}")
endforeach()

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 收录 src/ 下的功能测试代码
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
)

# 导出编译数据库（供 IDE / 语言工具链读取本层包含路径与宏）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译）
file(GLOB_RECURSE TEST_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.h"
    "${PROJECT_ROOT_DIR}/src/*.hpp"
)

# 主调头文件（TestLauncher 与 EngineTests 共用，单独收编以纳入索引）
file(GLOB_RECURSE MAIN_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/主调/*.h"
    "${PROJECT_ROOT_DIR}/src/主调/*.hpp"
)

# 本层各目标共用同一份生成脚本（与 EngineFoundation 共用，属构建工具而非层间契约）。
# 用途：把 GLOB 扫描结果落成显式清单（target_sources 形式），使 VS 文件视图的
#       改名 / 迁移操作有「可改写的引用」，从而触发 CMake 重新配置；原理与约束
#       见 Engine/EngineFoundation/cmake/生成源文件列表.cmake 头部说明。
# 说明：本层有多个目标，故按目标各一份清单文件 —— 目标多为条件创建，逐目标清单
#       可随目标所在的条件块按需 include，不会引用到不存在的目标。
include("${PROJECT_ROOT_DIR}/../../Engine/EngineFoundation/cmake/生成源文件列表.cmake")

# 占位空列表：供「只登记头文件、无源文件」的清单调用使用（同步函数要求传源 / 头两个变量名）
set(BYJY_NO_SOURCES "")

# 任务<1>：把被 GLOB 的目录登记为配置依赖 —— 目录内容变化（增删文件）时，
# 生成系统自动重跑配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
# 说明：GLOB_RECURSE 覆盖子目录，但 CMAKE_CONFIGURE_DEPENDS 要逐个目录登记；
#       故递归登记 src 下全部子目录（含 src/单元测试/EngineFoundation/工具、EngineFoundation/核心、Renderer、Test 等），
#       避免在深层目录新增文件时模型感知不到。
file(GLOB_RECURSE BYJY_SRC_DIRS LIST_DIRECTORIES true "${PROJECT_ROOT_DIR}/src/*")
foreach(_BYJY_DIR IN LISTS BYJY_SRC_DIRS)
    if(IS_DIRECTORY "${_BYJY_DIR}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_BYJY_DIR}")
    endif()
endforeach()

if(TEST_SOURCES)
    # 排除构建目录下的文件（防止把生成的临时文件误编译进去）
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER TEST_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
endif()

# 单元测试用例不并入 TestCore：它们由第 5 节的独立目标收编。
# 若并入静态库，未被引用的 .obj 会连 TEST 宏的注册一起被链接器丢弃，用例便静默不跑
list(FILTER TEST_SOURCES EXCLUDE REGEX "/src/单元测试/")

# 主调代码（含 main 与图形窗口）也不并入 TestCore：它是可执行入口，不是库内容
list(FILTER TEST_SOURCES EXCLUDE REGEX "/src/主调/")

# 头文件同样排除用例与主调目录（它们分别由下方独立目标收编）
list(FILTER TEST_HEADERS EXCLUDE REGEX "/src/单元测试/")
list(FILTER TEST_HEADERS EXCLUDE REGEX "/src/主调/")

# src/ 下暂无源文件时不视为错误：分阶段开发中测试代码尚未落位属正常状态，
# 此时不产出 TestCore，EngineTests 仍可只靠下层静态库独立生成。
# 说明：当前全部实现文件都在 src/单元测试/ 与 src/主调/ 下（已被上面的 FILTER 排除），
#       故 TEST_SOURCES 恒空、下方 TestCore 条件块不会进入；这是**有意的条件目标**，
#       不是残留引用 —— 若将来在 src/ 根或其它子目录新增功能测试代码，该块会自动生效，
#       并由 byjy_source_list_sync 在配置期生成 源文件列表_TestCore.cmake。
if(NOT TEST_SOURCES)
    message(STATUS "测试层 src/ 下暂无源文件，跳过 TestCore 静态库")
endif()

if(TEST_SOURCES)
    # =========================================================================
    # 3. 创建测试层静态库
    # =========================================================================
    # 源与头经 源文件列表_TestCore.cmake 的 target_sources 提供（理由见该清单头部）
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_TestCore.cmake"
        "${PROJECT_ROOT_DIR}"
        TestCore
        TEST_SOURCES
        TEST_HEADERS
        "Application/Test"
    )
    add_library(TestCore STATIC)
    include("${PROJECT_ROOT_DIR}/源文件列表_TestCore.cmake")

    # 包含根：下层头快照 + 项目根（供 Application/Test/... 全路径）
    target_include_directories(TestCore PUBLIC ${BYJY_APP_INC})

    # 链接下层静态库：包含路径 / 编译定义 / 第三方库由导入目标传递过来
    target_link_libraries(TestCore PUBLIC EngineFoundation EngineSystem EngineRenderer)

    # 编译选项与下层保持一致
    if(MSVC)
        target_compile_options(TestCore PRIVATE /MP /utf-8)
        target_compile_definitions(TestCore PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(TestCore PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(TestCore PRIVATE _GNU_SOURCE)
    endif()

    # 静态库输出到构建目录 lib/（与下层一致）
    set_target_properties(TestCore PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
endif()

# =============================================================================
# 3.5 GUI 第三方接入（Dear ImGui；平台后端 SDL3 + 渲染后端 Vulkan）
# =============================================================================
# 供主调的图形操作界面使用。与 external/googletest 同构：第三方源码以副本形式
# 放在本层 external/ 下，由本层自行编译，不改动下层的构建与对外接口。
# 说明：
#   - 不编 imgui_demo.cpp（示例窗口，本层用不到）
#   - 选择窗口改为经渲染层 UI 通道绘制：平台后端用 imgui_impl_sdl3.cpp、渲染后端用
#     imgui_impl_vulkan.cpp，不再使用 glfw / opengl3 后端与 glad
#   - Vulkan SDK 与 SDL3 的公共包含根、vulkan-1.lib 与 SDL3 系统库由 EngineRenderer 的对外面传递
if(EXISTS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/imgui/imgui.cpp"
   AND EXISTS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/backends/imgui_impl_sdl3.cpp"
   AND EXISTS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/backends/imgui_impl_vulkan.cpp")
    set(BYJY_IMGUI_DIR "${PROJECT_ROOT_DIR}/external/Dear_ImGui")

    add_library(TestGui STATIC
        "${BYJY_IMGUI_DIR}/imgui/imgui.cpp"
        "${BYJY_IMGUI_DIR}/imgui/imgui_draw.cpp"
        "${BYJY_IMGUI_DIR}/imgui/imgui_tables.cpp"
        "${BYJY_IMGUI_DIR}/imgui/imgui_widgets.cpp"
        "${BYJY_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
        "${BYJY_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    )

    # 包含目录：imgui 核心与后端；SDL3 / Vulkan 头与链接库随 EngineRenderer 的对外面传递
    target_include_directories(TestGui PUBLIC
        "${BYJY_IMGUI_DIR}/imgui"
        "${BYJY_IMGUI_DIR}/backends"
    )
    target_link_libraries(TestGui PUBLIC EngineFoundation EngineRenderer)

    if(MSVC)
        target_compile_options(TestGui PRIVATE /MP /utf-8)
        target_compile_definitions(TestGui PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    set_target_properties(TestGui PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
else()
    message(FATAL_ERROR "未找到 external/Dear_ImGui（imgui 核心或 imgui_impl_sdl3 / imgui_impl_vulkan 后端）：图形操作界面依赖它们，请先还原第三方副本")
endif()

# =============================================================================
# 4. 单元测试接入（googletest）
# =============================================================================
# 用例统一放在 src/单元测试/ 下，按被测层分「工具」「核心」两棵子树。
# 说明：googletest 以源码形式内嵌在 external/googletest，并以 EXCLUDE_FROM_ALL 接入：
#       只在被测试目标依赖时编译，不改变静态库的既有构建范围。
if(EXISTS "${PROJECT_ROOT_DIR}/external/googletest/CMakeLists.txt")
    #不安装 gtest、不构建 gmock；强制共享运行时库，避免与下层的 CRT 设置冲突（LNK2038）
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory("${PROJECT_ROOT_DIR}/external/googletest"
        "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
else()
    message(STATUS "未找到 external/googletest，跳过单元测试接入")
endif()

# 用例源收集
file(GLOB_RECURSE BYJY_TEST_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/单元测试/*.cpp"
)

# 用例头文件：同样纳入索引（不参与编译）
file(GLOB_RECURSE BYJY_TEST_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/单元测试/*.h"
    "${PROJECT_ROOT_DIR}/src/单元测试/*.hpp"
)

# 用例尚未落位时不视为错误，构建不因此中断
if(NOT BYJY_TEST_SOURCES)
    message(STATUS "src/单元测试/ 下暂无用例，跳过 EngineTests 与 EngineTestObjects")
elseif(NOT TARGET GTest::gtest)
    message(STATUS "gtest 目标不可用，跳过 EngineTests 与 EngineTestObjects")
else()
    # =========================================================================
    # 4.5 测试选择模型（主调逻辑，供主程序与用例共用）
    # =========================================================================
    # 只收纯逻辑部分：图形窗口依赖 ImGui，单独编进 EngineTests，不并入此库，
    # 使用例不必拖上图形库。
    # 主调头文件（MAIN_HEADERS）经 源文件列表_TestLauncher.cmake 的 target_sources
    # 提供，使 VS 文件视图对其改名 / 迁移时可改写引用并触发重配；两个 .cpp 是固定条目，
    # 仍写字面量（本就不属 GLOB 产物）。
    # 注意：引擎日志屏蔽.cpp 已随日志系统用例移入 tests/Foundation 子工程（该用例要
    #       临时关/开屏蔽）。主程序从 TestFoundation.lib 取得同名符号，此处不再重复编译，
    #       否则同一个符号会在链接期出现两份。
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_TestLauncher.cmake"
        "${PROJECT_ROOT_DIR}"
        TestLauncher
        BYJY_NO_SOURCES
        MAIN_HEADERS
        "Application/Test"
    )
    add_library(TestLauncher STATIC
        "${PROJECT_ROOT_DIR}/src/主调/测试选择模型.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/控制台选择菜单.cpp"
    )
    include("${PROJECT_ROOT_DIR}/源文件列表_TestLauncher.cmake")
    # 主调实现文件以本层根相对（src/主调/...）包含本层头；主调与用例共用的日志屏蔽
    # 需要引擎日志系统与引擎环境，故包含根取 BYJY_APP_INC（下层快照在前、项目根在后）
    target_include_directories(TestLauncher PUBLIC ${BYJY_APP_INC})
    target_link_libraries(TestLauncher PUBLIC GTest::gtest EngineFoundation)
    if(MSVC)
        target_compile_options(TestLauncher PRIVATE /MP /utf-8)
        target_compile_definitions(TestLauncher PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    set_target_properties(TestLauncher PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    # 用例源与头经 源文件列表_EngineTestObjects.cmake 的 target_sources 提供
    #（理由见该清单头部说明）
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_EngineTestObjects.cmake"
        "${PROJECT_ROOT_DIR}"
        EngineTestObjects
        BYJY_TEST_SOURCES
        BYJY_TEST_HEADERS
        "Application/Test"
    )
    #用例目标：编一次，EngineTests 取一份 .obj
    add_library(EngineTestObjects OBJECT)
    include("${PROJECT_ROOT_DIR}/源文件列表_EngineTestObjects.cmake")

    #包含根：下层快照 + 项目根（用例要 include "Application/Test/src/主调/测试选择模型.h"）
    target_include_directories(EngineTestObjects PUBLIC ${BYJY_APP_INC})

    #链接下层静态库以取得对外的包含路径；从 gtest 取得框架头文件路径；并由 TestLauncher 取得本层自身根
    target_link_libraries(EngineTestObjects PUBLIC EngineFoundation EngineSystem EngineRenderer GTest::gtest TestLauncher)

    #编译选项与下层保持一致
    if(MSVC)
        target_compile_options(EngineTestObjects PRIVATE /MP /utf-8)
        target_compile_definitions(EngineTestObjects PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(EngineTestObjects PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(EngineTestObjects PRIVATE _GNU_SOURCE)
    endif()

    # =========================================================================
    # 5. 独立测试可执行文件（本层唯一的可执行产物）
    # =========================================================================
    # main 由本层主调提供（src/主调/主程序.cpp），不再链接 GTest::gtest_main；
    # 图形操作界面（依赖 ImGui）只编进本可执行文件。
    # 产物落在本层根目录，与 assets/ 同级
    #（Engine_Env 以「可执行文件所在目录」为基准解析资源路径，这是硬性要求）
    # 主调头文件（MAIN_HEADERS）经 源文件列表_EngineTests.cmake 的 target_sources
    # 提供；$<TARGET_OBJECTS> 与三个 .cpp 是固定条目，仍写字面量。
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_EngineTests.cmake"
        "${PROJECT_ROOT_DIR}"
        EngineTests
        BYJY_NO_SOURCES
        MAIN_HEADERS
        "Application/Test"
    )
    add_executable(EngineTests
        $<TARGET_OBJECTS:EngineTestObjects>
        "${PROJECT_ROOT_DIR}/src/主调/主程序.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/图形操作界面.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/测试界面.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/测试运行器.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/gtest输出镜像.cpp"
    )
    include("${PROJECT_ROOT_DIR}/源文件列表_EngineTests.cmake")
    target_include_directories(EngineTests PRIVATE ${BYJY_APP_INC})
    target_link_libraries(EngineTests PRIVATE
        EngineTestObjects EngineFoundation EngineSystem EngineRenderer
        GTest::gtest TestLauncher TestGui
    )
    # 分层用例库：整体收编（见第 1.7 节），漏链时注册锚会令链接失败
    foreach(_byjy_test_lib IN LISTS BYJY_TEST_LIBS)
        target_link_libraries(EngineTests PRIVATE
            "$<LINK_LIBRARY:WHOLE_ARCHIVE,${_byjy_test_lib}>")
    endforeach()
    # 多配置生成器（VS）默认在输出目录后追加配置子目录（Debug/、Release/）；
    # 而用例按「可执行文件目录」解析 assets/ 与 shaders/，落到子目录会导致大量用例失败。
    # 用空生成器表达式（含 genex 即不追加）让 Debug / Release 都落在本层根目录；
    # 两配置顺序构建时覆盖同名产物（同一时刻只跑一份）。
    set_target_properties(EngineTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT_DIR}/$<0:>"
    )

    if(MSVC)
        target_compile_options(EngineTests PRIVATE /MP /utf-8)
    endif()

    #注册 ctest
    enable_testing()

    # gtest_discover_tests 会以 --gtest_output=json:<文件> 枚举用例，
    # 而 googletest 写该报告文件时按 ANSI 代码页解析路径：工程路径含中文时必然失败
    #（[FATAL] Unable to open file ...\\cmake_test_discovery_*.json），构建会被判失败。
    # 故路径为纯 ASCII 时才启用逐用例注册，否则退化为整体注册一个用例。
    if("${PROJECT_ROOT_DIR}" MATCHES "^[ -~]+$")
        message(STATUS "工程路径为纯 ASCII，启用逐用例注册（gtest_discover_tests）")
        include(GoogleTest)
        gtest_discover_tests(EngineTests
            WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
        )
    else()
        message(STATUS "工程路径含非 ASCII 字符，EngineTests 改为整体注册（逐用例）
   请直接运行 EngineTests.exe --gtest_filter=xxx 筛选用例")
        # 不开窗直接全跑：无参数的 ctest 不会阻塞在图形窗口上
        add_test(NAME EngineTests COMMAND EngineTests --selector=off
            WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
        )
    endif()
endif()

# =============================================================================
# 6. 宿主机制已取消
# =============================================================================
# 本层过去是「宿主产地」：把下层组件按由浅到深的范围接在一起，作为开发时的启动项。
# 该机制已移除，原因有二：
#     1) 宿主把下层源码拉进本构建树重复编译，与「层间一律链接已构建静态库」的契约冲突；
#     2) 分层宿主与 EngineTests 职责重叠，可执行文件只留一个入口更清楚。
# 现在的分工：下层各自构建静态库（Engine/EngineFoundation → EngineFoundation.lib、
# Engine/EngineSystem → EngineSystem.lib），本层构建 EngineTests.exe 作为唯一启动与测试入口。