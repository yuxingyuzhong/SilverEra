# =============================================================================
# EngineSystem —— 构建脚本
# =============================================================================
# 位置：白银纪元/Engine/EngineSystem
# 依赖：EngineCore —— 链接「已经构建好的」EngineCore.lib，方向单向，不反向依赖
# 产物：EngineSystem.lib（静态库）
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先构建 EngineCore。本层链接的是「已经构建好的」下层静态库，
#           不存在源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 包含路径：项目内部一律写「项目根相对全路径」——
#           本层头   Engine/EngineSystem/...
#           下层头   Engine/EngineCore/...
#           编译包含根 = [EngineCore 头快照根, 项目根, 第三方目录]（快照在前，见第 3.5 节）
# 层间契约：
#     cmake/对外接口.cmake   本层对外自描述（前缀 BYJY_ENGINE_SYSTEM，已并入 EngineCore 对外面）
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，逐字节与其他三层相同
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(EngineSystem LANGUAGES C CXX)

# 强制使用 C++20 标准（与 EngineCore 保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录；项目根 = Engine/EngineSystem 上溯两级（项目内部包含路径的基准）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${PROJECT_ROOT_DIR}/../.." ABSOLUTE)

# =============================================================================
# 1. 依赖：EngineCore —— 链接已构建好的静态库
# =============================================================================
# 本层只消费 EngineCore「已经构建好的」EngineCore.lib：下层源码不进入本次构建，
# 也没有任何源码回退路径（层级契约统一）。
#
# 库定位顺序由 cmake/接入下层.cmake 的 byjy_jieru_xiaceng() 统一实现：
#   ① 覆盖变量 BYJY_ENGINE_CORE_LIB_PATH 非空 → 用它；指向的文件不存在则直接报错
#   ② 留空 → 自动探测 Engine/EngineCore/out/build/*/lib/EngineCore.lib，取时间戳最新的一份
#   ③ 都没有 → FATAL_ERROR，并提示先构建 EngineCore
#
# EngineCore 对外面（包含目录 / 编译定义 / 链接库 / 系统库）由
# Engine/EngineCore/cmake/对外接口.cmake 声明，本函数只负责读取并建立导入目标 EngineCore。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_ENGINE_CORE_LIB_PATH "" CACHE FILEPATH
    "EngineCore 静态库路径；留空则自动探测 Engine/EngineCore/out/build/*/lib/EngineCore.lib")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../EngineCore/cmake/对外接口.cmake"
    "BYJY_ENGINE_CORE"
    "BYJY_ENGINE_CORE_LIB_PATH"
)

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 当前收录 src/entity（实体 / 实体管理器）、src/prop（属性 / 属性槽分发器）与 src/effect（效应 / 效应管理器）
file(GLOB_RECURSE SYSTEM_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/entity/*.cpp"
    "${PROJECT_ROOT_DIR}/src/entity/*.c"
    "${PROJECT_ROOT_DIR}/src/prop/*.cpp"
    "${PROJECT_ROOT_DIR}/src/prop/*.c"
    "${PROJECT_ROOT_DIR}/src/effect/*.cpp"
    "${PROJECT_ROOT_DIR}/src/effect/*.c"
)

# 导出编译数据库（供 IDE / 语言工具链读取本层包含路径与宏）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译）
file(GLOB_RECURSE SYSTEM_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/entity/*.h"
    "${PROJECT_ROOT_DIR}/src/entity/*.hpp"
    "${PROJECT_ROOT_DIR}/src/prop/*.h"
    "${PROJECT_ROOT_DIR}/src/prop/*.hpp"
    "${PROJECT_ROOT_DIR}/src/effect/*.h"
    "${PROJECT_ROOT_DIR}/src/effect/*.hpp"
)

# 任务<1>：把被 GLOB 的目录登记为配置依赖 —— 目录内容变化（增删文件）时，
# 生成系统自动重跑配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/entity"
    "${PROJECT_ROOT_DIR}/src/prop"
    "${PROJECT_ROOT_DIR}/src/effect"
)

if(SYSTEM_SOURCES)
    # 1) 排除构建目录下的文件（防止把生成的临时文件误编译进去）
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

    # 2) 排除测试目录（Tests/ 下为功能测试代码，不编入静态库）
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/Tests/")

    # 3) 排除配置编辑器目录（含独立 main()，不应编入静态库）
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/src/gui/Config_Editor/")

    # 头文件同样排除测试目录
    list(FILTER SYSTEM_HEADERS EXCLUDE REGEX "/Tests/")
endif()

if(NOT SYSTEM_SOURCES)
    message(FATAL_ERROR
        "未在 '${PROJECT_ROOT_DIR}/src/entity/' 或 '${PROJECT_ROOT_DIR}/src/prop/' 下找到任何 .cpp 或 .c 文件！\n"
        "请检查目录结构是否完整。"
    )
endif()

message(STATUS "EngineSystem 源文件: ${SYSTEM_SOURCES}")

# =============================================================================
# 3. 创建系统层静态库
# =============================================================================
# 本层的 PUBLIC 面直接取自 cmake/对外接口.cmake，让它成为唯一真源：包含目录 /
# 编译定义 / 链接库漏写或写错都会在构建期立刻暴露，不会与声明悄悄漂移。
# 该接口已并入 EngineCore 对外面，所以这一份声明同时覆盖「本层源码编译」与「上层使用」。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/对外接口.cmake")

# =============================================================================
# 3.5 EngineCore 头快照接入（本层按「与 EngineCore.lib 同版次」的引擎头编译）
# =============================================================================
# 对外包含目录里的「项目根」同时承载两类路径：
#   Engine/EngineCore/...   —— 来自 EngineCore，必须命中 EngineCore 构建目录内的头快照
#   Engine/EngineSystem/... —— 本层自身，仍读源码树
# 做法：把 EngineCore 的快照根作为「本层自身的包含目录」列出（排在项目根之前）——
#       目标自身的 -I 恒在导入目标传递来的 /external:I 之前，快照因此必然先命中。
# =============================================================================
byjy_tou_kuaizhao_gen(BYJY_ENGINE_CORE_SNAPSHOT EngineCore)

# 本层包含根：EngineCore 头快照在前、项目根在后
set(BYJY_ENGINE_SYSTEM_INC
    "${BYJY_ENGINE_CORE_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
)

add_library(EngineSystem STATIC ${SYSTEM_SOURCES} ${SYSTEM_HEADERS})

target_include_directories(EngineSystem PUBLIC
    ${BYJY_ENGINE_SYSTEM_INC}
    ${BYJY_ENGINE_SYSTEM_OUT_INC}
)

if(BYJY_ENGINE_SYSTEM_OUT_DEF)
    target_compile_definitions(EngineSystem PUBLIC ${BYJY_ENGINE_SYSTEM_OUT_DEF})
endif()

target_link_libraries(EngineSystem PUBLIC
    EngineCore
    ${BYJY_ENGINE_SYSTEM_OUT_LINK}
    ${BYJY_ENGINE_SYSTEM_OUT_SYS}
)

# 编译选项与 EngineCore 保持一致
if(MSVC)
    target_compile_options(EngineSystem PRIVATE /MP /utf-8)
    target_compile_definitions(EngineSystem PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(EngineSystem PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(EngineSystem PRIVATE _GNU_SOURCE)
endif()

# 静态库输出到构建目录 lib/（与 EngineCore 一致）
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
# 脚本复用 EngineCore 的同名通用脚本（纯路径操作，与本层无耦合）。
# =============================================================================
set(BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include")

add_custom_target(EngineSystemIncludeSnapshot ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${BYJY_PROJECT_ROOT}"
        "-DDST_ROOT=${BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
        "-DMIRROR_DIRS=Engine/EngineSystem/common;Engine/EngineSystem/src"
        -P "${PROJECT_ROOT_DIR}/../EngineCore/cmake/导出头快照.cmake"
    COMMENT "导出 EngineSystem 对外头快照 → ${BYJY_ENGINE_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
    VERBATIM
)

# =============================================================================
# 4. 暂未参与编译的模块
# =============================================================================
# src/gui/Config_Editor/（配置编辑器，含独立 main()）位于本层，
# 尚未建立独立可执行目标；将来接入时在下方追加 add_executable 并链接 EngineSystem，
# 同时把本层 external/ 下对应的包含路径补进 cmake/对外接口.cmake。
#
# 本层不生成宿主可执行文件：层级契约统一为「各层只产出静态库，只有 Test 层产出
# 可执行文件」，测试层侧的分层宿主入口已移除。