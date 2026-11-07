# =============================================================================
# EngineSystem —— 构建脚本
# =============================================================================
# 位置：白银纪元/Engine/EngineSystem
# 依赖：EngineFoundation —— 只链接「已经构建好的」静态库，方向单向
#       （渲染层是本层的消费者而非依赖：EngineFoundation → EngineSystem → EngineRenderer → 应用层）
# 产物：EngineSystem.lib（静态库，内含版本锚点符号与内嵌的 bullet3 碰撞实现）
# 用法（推荐 CMakePresets.json 的预设；Debug / Release 双构建树并存）：
#     Debug  ：cmake --preset x64-Debug   && cmake --build --preset build-Debug
#     Release：cmake --preset x64-Release && cmake --build --preset build-Release
#     产物：
#       <构建树>/lib/EngineSystem.lib
#       <构建树>/include/Engine/EngineSystem/...（对外头快照，含 版本锚点.h）
#       <构建树>/生成/版本锚点.{h,cpp}
# 前置条件：先按顺序构建 EngineFoundation 的对应配置树；
#           本层只链接已构建静态库，不存在源码回退路径。
# 包含路径：项目内部一律写「项目根相对全路径」——
#           本层头   Engine/EngineSystem/...
#           下层头   Engine/EngineFoundation/...
#           编译包含根 = [Foundation 快照根, 项目根, 第三方目录]
# 源码收集：src/、common/、external/ 三处整目录 GLOB（与 EngineFoundation 同口径）；
#           第三方源码（bullet3 的 LinearMath + BulletCollision、Lua 的 C 运行时）随 external/ 一并编入。
# 层间契约：
#     cmake/对外接口.cmake   本层对外自描述（前缀 BYJY_ENGINE_SYSTEM）
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，四份逐字节相同
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(EngineSystem LANGUAGES C CXX)

# 强制使用 C++20 标准（与 EngineFoundation 保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录；项目根 = Engine/EngineSystem 上溯两级（项目内部包含路径的基准）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${PROJECT_ROOT_DIR}/../.." ABSOLUTE)

# =============================================================================
# 0.5 归档响应文件 BOM 包装器（MSVC + Ninja 专用，与 EngineFoundation 共用脚本）
# =============================================================================
# 背景：Ninja 写出的归档响应文件无 BOM，MSVC lib.exe 会按 ANSI 代码页解读中文
#       对象名，归档阶段报 LNK1181。
# 说明：只对 Ninja 生成器生效；必须在 project() 之后设置，否则会被平台默认值覆盖。
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set(BYJY_AR_WRAPPER "${PROJECT_ROOT_DIR}/../EngineFoundation/cmake/ar_rsp_bom.ps1")
    find_program(BYJY_POWERSHELL_EXECUTABLE NAMES powershell powershell.exe)
    if(EXISTS "${BYJY_AR_WRAPPER}" AND BYJY_POWERSHELL_EXECUTABLE)
        set(CMAKE_CXX_CREATE_STATIC_LIBRARY
            "${BYJY_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass -File ${BYJY_AR_WRAPPER} <CMAKE_AR> /nologo <LINK_FLAGS> /out:<TARGET> <OBJECTS> ")
        message(STATUS "归档响应文件 BOM 包装器已启用：${BYJY_AR_WRAPPER}")
    else()
        message(WARNING "未找到归档包装器或 PowerShell，中文对象名的归档可能报 LNK1181：${BYJY_AR_WRAPPER}")
    endif()
endif()

# =============================================================================
# 0. 构建类型（Debug / Release 双构建树）
# =============================================================================
# 与 EngineFoundation 同一套约定：CMAKE_BUILD_TYPE 决定本层产出的 EngineSystem.lib 是
# Debug(/MDd) 还是 Release(/MD)，构建树目录也按类型分开（out/build/x64-<类型>）。
# 省略 -DCMAKE_BUILD_TYPE 时按 Debug 处理；非法取值直接报错，避免静默生成意外的库。
if(NOT CMAKE_CONFIGURATION_TYPES)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型（Debug / Release）" FORCE)
        message(STATUS "未指定 CMAKE_BUILD_TYPE，按 Debug 处理（构建树：${CMAKE_BINARY_DIR}）")
    endif()

    set(BYJY_VALID_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
    if(NOT CMAKE_BUILD_TYPE IN_LIST BYJY_VALID_BUILD_TYPES)
        message(FATAL_ERROR
            "EngineSystem：不支持的 CMAKE_BUILD_TYPE = \"${CMAKE_BUILD_TYPE}\"\n"
            "  合法取值：${BYJY_VALID_BUILD_TYPES}\n"
            "  双构建树示例：\n"
            "      cmake -S \"${CMAKE_CURRENT_SOURCE_DIR}\" -B \"${CMAKE_CURRENT_SOURCE_DIR}/out/build/x64-Debug\"   -G Ninja -DCMAKE_BUILD_TYPE=Debug\n"
            "      cmake -S \"${CMAKE_CURRENT_SOURCE_DIR}\" -B \"${CMAKE_CURRENT_SOURCE_DIR}/out/build/x64-Release\" -G Ninja -DCMAKE_BUILD_TYPE=Release")
    endif()

    message(STATUS "构建类型：${CMAKE_BUILD_TYPE}（构建树：${CMAKE_BINARY_DIR}）")
endif()

# =============================================================================
# 1. 依赖：EngineFoundation —— 链接已构建好的静态库
# =============================================================================
# 本层只消费 EngineFoundation「已经构建好的」EngineFoundation.lib：下层源码不进入本次构建，
# 也没有任何源码回退路径（层级契约统一）。
#
# 版本解析（cmake/版本工具.cmake）：
#   ① 显式覆盖 BYJY_ENGINE_FOUNDATION_LIB_PATH 非空 → 用它；文件不存在直接报错
#   ② 否则读 Engine/版本锁.cmake：
#        锁空   → 跟随最新：Engine/EngineFoundation/out/build/x64-<当前配置>
#        锁非空 → 锁定版本：Engine/版本缓存/EngineFoundation/x64-<配置>/<版本哈希>
#   ③ 锁指定但缓存缺失 → FATAL_ERROR（提示先构建该层或清空锁）
#
# EngineFoundation 对外面（包含目录 / 编译定义 / 链接库 / 系统库）由
# Engine/EngineFoundation/cmake/对外接口.cmake 声明，本函数只负责读取并建立导入目标 EngineFoundation。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")
include("${PROJECT_ROOT_DIR}/../EngineFoundation/cmake/版本工具.cmake")

set(BYJY_ENGINE_FOUNDATION_LIB_PATH "" CACHE FILEPATH
    "EngineFoundation 静态库路径；留空则按版本锁解析（锁空 = 跟随最新构建树）")

# 双配置解析：Debug / Release 各算一份。多配置生成器（VS）下由 IMPORTED_LOCATION_<CONFIG>
# 与 $<CONFIG> 包含根分别选取；单配置生成器（Ninja）等价于只用当前配置那一份。
if("${BYJY_ENGINE_FOUNDATION_LIB_PATH}" STREQUAL "")
    byjy_banben_jiexi_shuang(EngineFoundation engine_foundation
        "${PROJECT_ROOT_DIR}/../EngineFoundation"
        BYJY_ENGINE_FOUNDATION_LIB BYJY_ENGINE_FOUNDATION_SNAPSHOT BYJY_ENGINE_FOUNDATION_VERSION)
    # 单配置生成器取当前配置；多配置生成器稍后由分配置属性覆盖
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
        set(BYJY_ENGINE_FOUNDATION_LIB_PATH "${BYJY_ENGINE_FOUNDATION_LIB_Release}")
    else()
        set(BYJY_ENGINE_FOUNDATION_LIB_PATH "${BYJY_ENGINE_FOUNDATION_LIB_Debug}")
    endif()
else()
    # 手工覆盖：两配置共用同一份库
    set(BYJY_ENGINE_FOUNDATION_LIB_Debug "${BYJY_ENGINE_FOUNDATION_LIB_PATH}")
    set(BYJY_ENGINE_FOUNDATION_LIB_Release "${BYJY_ENGINE_FOUNDATION_LIB_PATH}")
endif()

# 给导入函数提供分配置库路径：锚点头按 $<CONFIG> 分别注入，必须先于导入设置。
set(BYJY_ENGINE_FOUNDATION_LIB_PATH_DEBUG   "${BYJY_ENGINE_FOUNDATION_LIB_Debug}")
set(BYJY_ENGINE_FOUNDATION_LIB_PATH_RELEASE "${BYJY_ENGINE_FOUNDATION_LIB_Release}")
# 分配置头快照根：VS 构建树的布局与 Ninja 不同，锚点不能靠库路径反推
set(BYJY_ENGINE_FOUNDATION_LIB_PATH_INCLUDE_DEBUG   "${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug}")
set(BYJY_ENGINE_FOUNDATION_LIB_PATH_INCLUDE_RELEASE "${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Release}")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../EngineFoundation/cmake/对外接口.cmake"
    "BYJY_ENGINE_FOUNDATION"
    "BYJY_ENGINE_FOUNDATION_LIB_PATH"
)

# 多配置：逐配置设置导入库位置（单配置生成器忽略这些属性）
foreach(_byjy_cfg IN ITEMS DEBUG RELEASE)
    set(_byjy_cfg_lib "${BYJY_ENGINE_FOUNDATION_LIB_${_byjy_cfg}}")
    if(NOT "${_byjy_cfg_lib}" STREQUAL "" AND EXISTS "${_byjy_cfg_lib}")
        set_target_properties(EngineFoundation PROPERTIES
            IMPORTED_LOCATION_${_byjy_cfg} "${_byjy_cfg_lib}")
    endif()
endforeach()
if(EXISTS "${BYJY_ENGINE_FOUNDATION_LIB_Release}")
    set_target_properties(EngineFoundation PROPERTIES
        IMPORTED_LOCATION_RELWITHDEBINFO "${BYJY_ENGINE_FOUNDATION_LIB_Release}"
        IMPORTED_LOCATION_MINSIZEREL "${BYJY_ENGINE_FOUNDATION_LIB_Release}")
endif()

# 手工覆盖 / 仅解析出单配置时：快照根由导入目标反推，并补齐成双配置同一份
if(NOT DEFINED BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug OR "${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug}" STREQUAL "")
    byjy_tou_kuaizhao_gen(BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug EngineFoundation)
endif()
if(NOT DEFINED BYJY_ENGINE_FOUNDATION_SNAPSHOT_Release OR "${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Release}" STREQUAL "")
    set(BYJY_ENGINE_FOUNDATION_SNAPSHOT_Release "${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug}")
endif()

# 包含根：按 $<CONFIG> 在 Debug / Release 快照之间切换
set(BYJY_ENGINE_FOUNDATION_SNAPSHOT
    "$<IF:$<CONFIG:Debug>,${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Debug},${BYJY_ENGINE_FOUNDATION_SNAPSHOT_Release}>")

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 与 EngineFoundation 同一口径：整目录 GLOB，不再逐模块列白名单。
# 收录范围：
#   src/、common/   本层自有源码（entity / prop / effect / scene / collision 等）
#   external/       第三方源码 —— bullet3 的 LinearMath + BulletCollision、Lua 的 C 运行时
# 说明：external/ 在磁盘上已裁剪为「只含需要编译的子集」（bullet3 仅保留两个库目录、
#       Lua 仅保留核心源码且无 lua.c / luac.c / onelua.c 等独立入口），故不再单独列清单。
file(GLOB_RECURSE SYSTEM_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/common/*.cpp"
    "${PROJECT_ROOT_DIR}/external/*.cpp"
    "${PROJECT_ROOT_DIR}/external/*.c"
)

# 导出编译数据库（供 IDE / 语言工具链读取本层包含路径与宏）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译）
file(GLOB_RECURSE SYSTEM_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.h"
    "${PROJECT_ROOT_DIR}/src/*.hpp"
    "${PROJECT_ROOT_DIR}/common/*.h"
    "${PROJECT_ROOT_DIR}/common/*.hpp"
)

# 把被 GLOB 的目录登记为配置依赖：目录内容变化（增删文件）时，生成系统自动
# 重跑配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src"
    "${PROJECT_ROOT_DIR}/common"
    "${PROJECT_ROOT_DIR}/external"
)

if(SYSTEM_SOURCES)
    # 1) 排除构建目录下的文件（防止把生成的临时文件误编译进去）
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

    # 2) 排除功能测试目录（若将来出现 src/Tests，测试代码不编入静态库）
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/Tests/")
    list(FILTER SYSTEM_HEADERS EXCLUDE REGEX "/Tests/")
endif()

if(NOT SYSTEM_SOURCES)
    message(FATAL_ERROR
        "未在 '${PROJECT_ROOT_DIR}/src/' 下找到任何 .cpp 文件！\n"
        "请检查目录结构是否完整。"
    )
endif()

# 把扫描结果同步进显式清单 源文件列表.cmake（仅在路径集合有差异时写回），
# 清单随后以 target_sources 提供全部源与头。
# 动机：GLOB 工程里构建脚本不含任何「具体文件名」，VS 的文件视图改名 / 迁移操作
#       找不到可改写的引用，于是既不触发 CMake 重新配置，VS 的模型与错误列表也
#       停在旧版次。清单把扫描结果显式化，为该操作提供可改写的引用。
# 说明：清单被 include，因此它本身也是配置依赖 —— 被改写即触发重新配置。
#       脚本与 EngineFoundation 共用同一份（构建工具，非层间契约），避免多份副本漂移。
include("${PROJECT_ROOT_DIR}/../EngineFoundation/cmake/生成源文件列表.cmake")
byjy_source_list_sync(
    "${PROJECT_ROOT_DIR}/源文件列表.cmake"
    "${PROJECT_ROOT_DIR}"
    EngineSystem
    SYSTEM_SOURCES
    SYSTEM_HEADERS
    "Engine/EngineSystem"
)

# =============================================================================
# 3. 创建系统层静态库
# =============================================================================
# 本层的 PUBLIC 面直接取自 cmake/对外接口.cmake，让它成为唯一真源：包含目录 /
# 编译定义 / 链接库漏写或写错都会在构建期立刻暴露，不会与声明悄悄漂移。
# 该接口已并入 EngineFoundation 对外面，所以这一份声明同时覆盖「本层源码编译」与「上层使用」。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/对外接口.cmake")

# =============================================================================
# 3.5 EngineFoundation 头快照接入（本层按「与 EngineFoundation.lib 同版次」的引擎头编译）
# =============================================================================
# 对外包含目录里的「项目根」同时承载两类路径：
#   Engine/EngineFoundation/...   —— 来自 EngineFoundation，必须命中 EngineFoundation 的头快照
#   Engine/EngineSystem/... —— 本层自身，仍读源码树
# 做法：把 EngineFoundation 的快照根作为「本层自身的包含目录」列出（排在项目根之前）——
#       目标自身的 -I 恒在导入目标传递来的 /external:I 之前，快照因此必然先命中。
# 快照根已在第 1 节由版本解析给出（锁空 = 构建树快照，锁非空 = 版本缓存快照）。
# =============================================================================
# 兜底：本层源码引用的 Foundation 头必须都在快照里，否则说明下层新增/重命名了头却没重建。
# 双配置逐份校验；对应构建树不存在的配置跳过。
foreach(_byjy_cfg IN ITEMS Debug Release)
    set(_byjy_snap "${BYJY_ENGINE_FOUNDATION_SNAPSHOT_${_byjy_cfg}}")
    if(IS_DIRECTORY "${_byjy_snap}")
        byjy_jiaoyan_xiacengtou("EngineSystem(${_byjy_cfg})" "${_byjy_snap}" EngineFoundation
            "${PROJECT_ROOT_DIR}/src" "${PROJECT_ROOT_DIR}/common")
    endif()
endforeach()

# 本层包含根：EngineFoundation 头快照在前、项目根在后。
# 本层不再接入任何上层头快照 —— 渲染层是本层的消费者，依赖方向为
# EngineFoundation → EngineSystem → EngineRenderer。
set(BYJY_ENGINE_SYSTEM_INC
    "${BYJY_ENGINE_FOUNDATION_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
)

# 此处不传任何源文件：源与头全部由 源文件列表.cmake 的 target_sources 提供。
# 原因见清单头部说明 —— 只有由 target_sources 直接加入的字面文件名，其 File API
# backtrace 才落在清单文件上，VS 的改名 / 迁移操作才改写得到引用。
add_library(EngineSystem STATIC)
include("${PROJECT_ROOT_DIR}/源文件列表.cmake")

# =============================================================================
# 3.1 第三方源码的编译口径（bullet3 / Lua）
# =============================================================================
# 第三方源码已由第 2 节的 `external/*.{cpp,c}` 一并收录，这里只补两件事：
#   1) 第三方代码不跟随本层警告口径 —— MSVC 下对 external/ 下的源单独 /W3；
#   2) 收集第三方头，供版次锚点做哈希与依赖（external 参与本层编译，头变化须触发版次变化）。
# 说明：
#   - bullet3 只保留 LinearMath + BulletCollision（磁盘已裁剪，GLOB 结果即所需子集），
#     不使用上游自带的 CMakeLists（含安装规则与上游专属变量）；Gimpact 属于 BulletCollision。
#   - Lua 的 .c 全部编入本库，对外只暴露其包含根（见 cmake/对外接口.cmake）。
#     平台宏无需额外定义：luaconf.h 在 _WIN32 下自行定义 LUA_USE_WINDOWS / LUA_DL_DLL。
# =============================================================================
set(BYJY_EXTERNAL_SOURCES "")
foreach(_byjy_src IN LISTS SYSTEM_SOURCES)
    # external/ 下的第三方源
    if(_byjy_src MATCHES "/external/")
        list(APPEND BYJY_EXTERNAL_SOURCES "${_byjy_src}")
    endif()
endforeach()

if(MSVC AND BYJY_EXTERNAL_SOURCES)
    set_source_files_properties(${BYJY_EXTERNAL_SOURCES} PROPERTIES COMPILE_OPTIONS "/W3")
endif()

file(GLOB_RECURSE BYJY_EXTERNAL_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/external/*.h"
    "${PROJECT_ROOT_DIR}/external/*.hpp"
    "${PROJECT_ROOT_DIR}/external/*.inl"
)

# =============================================================================
# 3.2 版次锚点（头 / 库同版次由链接期符号强制）
# =============================================================================
# 与 EngineFoundation / Renderer 同一套机制：对外头集合 + 构建类型 + 运行库口径的哈希
# 编进符号名，生成 版本锚点.h/.cpp；cpp 编入本库，h 随本层头快照发布。
# 上层（EngineRenderer / Test / Game）编译时由导入目标强制包含该头，头库错配即 LNK2019。
# 说明：哈希目录默认与头快照镜像范围一致；本层额外把 external/ 下的第三方头
#       （bullet3 / Lua / Sol2）纳入哈希（HASH_DIRS），使第三方头变更同样触发版次变化。
# =============================================================================
set(BYJY_ENGINE_SYSTEM_ANCHOR_REL "Engine/EngineSystem/版本锚点.h")
set(BYJY_ENGINE_SYSTEM_ANCHOR_DIR "${CMAKE_BINARY_DIR}/生成")
set(BYJY_ENGINE_SYSTEM_ANCHOR_H "${BYJY_ENGINE_SYSTEM_ANCHOR_DIR}/版本锚点.h")
set(BYJY_ENGINE_SYSTEM_ANCHOR_CPP "${BYJY_ENGINE_SYSTEM_ANCHOR_DIR}/版本锚点.cpp")

add_custom_command(
    OUTPUT "${BYJY_ENGINE_SYSTEM_ANCHOR_H}" "${BYJY_ENGINE_SYSTEM_ANCHOR_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${BYJY_PROJECT_ROOT}"
        "-DMIRROR_DIRS=Engine/EngineSystem/common;Engine/EngineSystem/src"
        "-DHASH_DIRS=Engine/EngineSystem/common;Engine/EngineSystem/src;Engine/EngineSystem/external"
        "-DBUILD_TYPE=$<CONFIG>"
        "-DOUT_H=${BYJY_ENGINE_SYSTEM_ANCHOR_H}"
        "-DOUT_CPP=${BYJY_ENGINE_SYSTEM_ANCHOR_CPP}"
        "-DLAYER_TOKEN=engine_system"
        -P "${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/生成版本锚点.cmake"
    DEPENDS ${SYSTEM_HEADERS} ${BYJY_EXTERNAL_HEADERS} "${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/生成版本锚点.cmake"
    COMMENT "生成 EngineSystem 版次锚点"
    VERBATIM
)
set_source_files_properties("${BYJY_ENGINE_SYSTEM_ANCHOR_CPP}" PROPERTIES GENERATED TRUE)
target_sources(EngineSystem PRIVATE "${BYJY_ENGINE_SYSTEM_ANCHOR_CPP}")

target_include_directories(EngineSystem PUBLIC
    ${BYJY_ENGINE_SYSTEM_INC}
    ${BYJY_ENGINE_SYSTEM_OUT_INC}
)
# 层内实现文件（.cpp）跨目录包含写「本层根相对」src/...；头文件仍写项目根全路径。
# 仅本层可见、不向外传播（否则消费者用 src/... 可能命中本层源码）。
target_include_directories(EngineSystem PRIVATE "${PROJECT_ROOT_DIR}")

if(BYJY_ENGINE_SYSTEM_OUT_DEF)
    target_compile_definitions(EngineSystem PUBLIC ${BYJY_ENGINE_SYSTEM_OUT_DEF})
endif()

target_link_libraries(EngineSystem PUBLIC
    EngineFoundation
    ${BYJY_ENGINE_SYSTEM_OUT_LINK}
    ${BYJY_ENGINE_SYSTEM_OUT_SYS}
)

# 编译选项与 EngineFoundation 保持一致
if(MSVC)
    target_compile_options(EngineSystem PRIVATE /MP /utf-8)
    target_compile_definitions(EngineSystem PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(EngineSystem PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(EngineSystem PRIVATE _GNU_SOURCE)
endif()

# 静态库输出到构建目录 lib/（与 EngineFoundation 一致）
set_target_properties(EngineSystem PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# =============================================================================
# 3.6 对外头快照导出（供上层按「与本层静态库同版次」的头编译）
# =============================================================================
# 镜像范围与本层公共头实际暴露的面一一对应：
#   common、src —— 本层自身公共头（含 common/external/Sol2/ 下的包装头）
# 第三方（external/Sol2/include、external/Lua）在外对面上是独立的绝对路径条目，
# 不经过快照解析，故不需镜像。
# 路径形态：镜像目录相对「项目根」保留完整层级 —— 快照根/Engine/EngineSystem/...
# 脚本复用 EngineFoundation 的同名通用脚本（纯路径操作，与本层无耦合）。
# =============================================================================
set(BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include")
# 多配置生成器（VS）：一棵树装两个配置，快照必须按配置分目录，否则两配置互相覆盖
if(CMAKE_CONFIGURATION_TYPES)
    set(BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include/$<CONFIG>")
endif()

add_custom_target(EngineSystemIncludeSnapshot ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${BYJY_PROJECT_ROOT}"
        "-DDST_ROOT=${BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
        "-DMIRROR_DIRS=Engine/EngineSystem/common;Engine/EngineSystem/src"
        "-DANCHOR_SRC=${BYJY_ENGINE_SYSTEM_ANCHOR_H}"
        "-DANCHOR_REL=${BYJY_ENGINE_SYSTEM_ANCHOR_REL}"
        -P "${PROJECT_ROOT_DIR}/../EngineFoundation/cmake/导出头快照.cmake"
    COMMENT "导出 EngineSystem 对外头快照 → ${BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
    VERBATIM
)

# 快照必须在本层库构建成功之后导出，避免「新头 + 旧库」的中间状态。
add_dependencies(EngineSystemIncludeSnapshot EngineSystem)

# =============================================================================
# 3.7 版本缓存发布（版本锁机制的供方）
# =============================================================================
# 把本层刚构建好的对外头快照与 EngineSystem.lib 按版本哈希存入共享缓存，
# 供上层「锁定到本版本」时取用。依赖版本.cmake 一并拷入，供上层做全链一致性校验。
# =============================================================================
byjy_banben_jilu("${CMAKE_BINARY_DIR}")

add_custom_target(EngineSystemVersionPublish ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DSNAPSHOT_ROOT=${BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
        "-DLIB_FILE=$<TARGET_FILE:EngineSystem>"
        "-DLAYER_NAME=EngineSystem"
        "-DLAYER_TOKEN=engine_system"
        "-DCONFIG_TREE=x64-$<CONFIG>"
        "-DCACHE_ROOT=${BYJY_BANBEN_CACHE_ROOT}"
        "-DPROJECT_ROOT=${BYJY_PROJECT_ROOT}"
        "-DKEEP=8"
        -P "${PROJECT_ROOT_DIR}/../EngineFoundation/cmake/发布版本缓存.cmake"
    COMMENT "发布 EngineSystem 版本缓存"
    VERBATIM
)
add_dependencies(EngineSystemVersionPublish EngineSystemIncludeSnapshot)

# =============================================================================
# 4. 安装规则（与 EngineFoundation 同口径，仅安装本层自有头）
# =============================================================================
# 说明：external/ 下的第三方头（Sol2 / Lua / bullet3）不随本层安装 —— 它们在外
#       对面上是独立的绝对路径条目，不进头快照，也不属于本层产物。
#       构建期生成的 版本锚点.h 同样不在安装范围（当前无安装使用方）。
foreach(_byjy_install_dir IN ITEMS "common" "src")
    install(DIRECTORY "${PROJECT_ROOT_DIR}/${_byjy_install_dir}"
        DESTINATION "include/Engine/EngineSystem/${_byjy_install_dir}"
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endforeach()

install(TARGETS EngineSystem
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
)

