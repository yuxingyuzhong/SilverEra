# =============================================================================
# 系统层 —— 构建脚本
# =============================================================================
# 位置：白银纪元/系统层
# 依赖：引擎层（EngineCore）—— 系统层链接引擎层，方向单向，不反向依赖
# 产物：SystemCore 静态库
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先构建引擎层。本层链接的是「已经构建好的」引擎层静态库，
#           不存在源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 层间契约：
#     cmake/对外接口.cmake   本层对外自描述（已并入引擎层对外面）
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，三层逐字节相同
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(系统层 LANGUAGES C CXX)

# 强制使用 C++20 标准（与引擎层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# =============================================================================
# 1. 依赖：引擎层 —— 链接已构建好的静态库
# =============================================================================
# 本层只消费引擎层「已经构建好的」EngineCore.lib：引擎层源码不进入本次构建，
# 也没有任何源码回退路径（层级契约统一）。
#
# 库定位顺序由 cmake/接入下层.cmake 的 byjy_jieru_xiaceng() 统一实现：
#   ① 覆盖变量 BYJY_ENGINE_LIB_PATH 非空 → 用它；指向的文件不存在则直接报错
#   ② 留空 → 自动探测 引擎层/out/build/*/lib/EngineCore.lib，取时间戳最新的一份
#   ③ 都没有 → FATAL_ERROR，并提示先构建引擎层
#
# 引擎层对外面（包含目录 / 编译定义 / 链接库 / 系统库）由
# 引擎层/cmake/对外接口.cmake 声明，本函数只负责读取并建立导入目标 EngineCore。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE FILEPATH
    "引擎层静态库路径；留空则自动探测 引擎层/out/build/*/lib/EngineCore.lib")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../引擎层/cmake/对外接口.cmake"
    "BYJY_ENGINE"
    "BYJY_ENGINE_LIB_PATH"
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

message(STATUS "系统层源文件: ${SYSTEM_SOURCES}")

# =============================================================================
# 3. 创建系统层静态库
# =============================================================================
# 本层的 PUBLIC 面直接取自 cmake/对外接口.cmake，让它成为唯一真源：包含目录 /
# 编译定义 / 链接库漏写或写错都会在构建期立刻暴露，不会与声明悄悄漂移。
# 该接口已并入引擎层对外面，所以这一份声明同时覆盖「本层源码编译」与「上层使用」。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/对外接口.cmake")

# =============================================================================
# 3.5 引擎层头快照接入（本层改为按「与 EngineCore.lib 同版次」的引擎头编译）
# =============================================================================
# 本层包含目录里既有本层自有面，也并入了引擎层对外面（${BYJY_ENGINE_OUT_INC}，指向引擎层
# 源码树）。此处把其中位于引擎层源码树下的条目改指引擎层构建目录内的头快照，消除
# 「引擎头已改、EngineCore.lib 未重建」时新头配旧库地编译链接的风险。
# 说明：普通库目标自身的编译包含路径由 INCLUDE_DIRECTORIES 决定，因此必须先把对外包含
#       目录「按列表换算」再交给 target_include_directories；只改导入目标
#       （EngineCore）的 INTERFACE 面不足以改变 SystemCore 自身的编译包含路径。
# =============================================================================
get_target_property(BYJY_ENGINE_LIB_FILE EngineCore IMPORTED_LOCATION)
get_filename_component(BYJY_ENGINE_BUILD_DIR "${BYJY_ENGINE_LIB_FILE}/../.." ABSOLUTE)
get_filename_component(BYJY_ENGINE_SOURCE_DIR "${PROJECT_ROOT_DIR}/../引擎层" ABSOLUTE)

if(NOT IS_DIRECTORY "${BYJY_ENGINE_BUILD_DIR}/include")
    message(FATAL_ERROR
        "系统层：找不到引擎层头快照\n"
        "  期望位置：${BYJY_ENGINE_BUILD_DIR}/include\n"
        "  该目录由引擎层构建时导出，请先构建引擎层：\n"
        "      cmake --build \"${PROJECT_ROOT_DIR}/../引擎层/out/build/x64-Debug\"")
endif()

# 把包含目录里位于引擎层源码树下的条目改指引擎层头快照
byjy_kuaizhao_lujing(BYJY_SYSTEM_INC_SNAPSHOT
    "${BYJY_ENGINE_BUILD_DIR}/include" "${BYJY_ENGINE_SOURCE_DIR}" "${BYJY_SYSTEM_OUT_INC}")

# 引擎层导入面同步改指快照（否则其源码树条目仍会随链接进入本层编译包含路径）
byjy_qiehuan_tou_kuaizhao(EngineCore "${BYJY_ENGINE_BUILD_DIR}/include" "${BYJY_ENGINE_SOURCE_DIR}")

add_library(SystemCore STATIC ${SYSTEM_SOURCES} ${SYSTEM_HEADERS})

target_include_directories(SystemCore PUBLIC ${BYJY_SYSTEM_INC_SNAPSHOT})

if(BYJY_SYSTEM_OUT_DEF)
    target_compile_definitions(SystemCore PUBLIC ${BYJY_SYSTEM_OUT_DEF})
endif()

target_link_libraries(SystemCore PUBLIC
    EngineCore
    ${BYJY_SYSTEM_OUT_LINK}
    ${BYJY_SYSTEM_OUT_SYS}
)

# 编译选项与引擎层保持一致
if(MSVC)
    target_compile_options(SystemCore PRIVATE /MP /utf-8)
    target_compile_definitions(SystemCore PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(SystemCore PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(SystemCore PRIVATE _GNU_SOURCE)
endif()

# 静态库输出到构建目录 lib/（与引擎层一致）
set_target_properties(SystemCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# =============================================================================
# 3.6 对外头快照导出（供上层按「与本层静态库同版次」的头编译）
# =============================================================================
# 镜像范围与本层 cmake/对外接口.cmake 声明的自有包含目录一一对应：
#   层根 ＋ 层根/src ← common、src（层根对应预编译头与 src/ 两种引用形式）
#   external/Sol2/include、external/Lua ← 本层公共头暴露 LuaState 所需
# 脚本复用引擎层的同名通用脚本（纯路径操作，与本层无耦合）。
# =============================================================================
set(BYJY_SYSTEM_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include")

add_custom_target(SystemIncludeSnapshot ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${PROJECT_ROOT_DIR}"
        "-DDST_ROOT=${BYJY_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
        "-DMIRROR_DIRS=common;src;external/Sol2/include;external/Lua"
        -P "${PROJECT_ROOT_DIR}/../引擎层/cmake/导出头快照.cmake"
    COMMENT "导出系统层对外头快照 → ${BYJY_SYSTEM_INCLUDE_SNAPSHOT_DIR}"
    VERBATIM
)

# =============================================================================
# 4. 暂未参与编译的模块
# =============================================================================
# src/gui/Config_Editor/（配置编辑器，含独立 main()）已由 引擎层 迁入本层，
# 尚未建立独立可执行目标；将来接入时在下方追加 add_executable 并链接 SystemCore，
# 同时把 系统层/external/ 下对应的包含路径补进 cmake/对外接口.cmake。
#
# 本层不再生成宿主可执行文件：层级契约统一为「各层只产出静态库，只有测试层产出
# 可执行文件」，测试层侧的分层宿主入口已移除。
