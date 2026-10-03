# =============================================================================
# Game —— 构建脚本
# =============================================================================
# 位置：白银纪元/Application/Game
# 依赖：Engine 聚合层（对外转发 EngineSystem.lib + EngineCore.lib）—— 一律链接「已经构建好的」静态库
# 产物：GameCore 静态库（本层当前尚无源码，src/ 下出现 .cpp/.c 后自动纳入）
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先依次构建 Engine/EngineCore 与 Engine/EngineSystem。本层不拉入任何下层源码，
#           也没有源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 包含路径：项目内部一律写「项目根相对全路径」——
#           引擎头   Engine/EngineCore/...、Engine/EngineSystem/...
#           本层自身 Application/Game/...
#           编译包含根 = [下层头快照根, ..., 项目根]（快照在前，见第 1.5 节，次序不可颠倒）
# 层间契约：
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，逐字节与其他三层相同
# 说明：
#     - 本层不产出可执行文件。可执行文件只在 Test 层产生（EngineTests.exe）；
#       游戏本体将来需要入口时，再在本层单独开一个可执行目标。
#     - 静态库输出到构建目录 lib/，供更上层或 Test 层链接。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(Game LANGUAGES C CXX)

# 强制使用 C++20 标准（与下层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录；项目根供 Application/Game/... 全路径解析
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${PROJECT_ROOT_DIR}/../.." ABSOLUTE)

# =============================================================================
# 1. 接入 Engine 聚合层 —— 链接已构建好的静态库
# =============================================================================
# 本层只消费下层「已经构建好的」静态库：下层源码不进入本次构建，也没有源码回退路径。
# Engine 聚合层的对外面转发 EngineSystem.lib + EngineCore.lib 两个库，故这里只接一次。
#
# 库定位顺序由 cmake/接入下层.cmake 的 byjy_jieru_xiaceng() 统一实现：
#   ① 覆盖变量非空（须与库数等长的分号列表）→ 用它；文件不存在则直接报错
#   ② 留空 → 自动探测 <该库所在层目录>/out/build/*/lib/<库名>.lib，取时间戳最新的一份
#   ③ 都没有 → FATAL_ERROR，并提示先构建产出该库的下层
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE STRING
    "Engine 聚合层静态库路径（分号列表，顺序同 EngineSystem;EngineCore）；留空则自动探测")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../../Engine/cmake/对外接口.cmake"
    "BYJY_ENGINE"
    "BYJY_ENGINE_LIB_PATH"
)

# =============================================================================
# 1.5 下层头快照接入
# =============================================================================
# 本层按「与下层静态库同版次」的头文件编译：把两个子层的头快照根作为「本层自身的
# 包含目录」列出，并排在项目根之前 —— 目标自身的 -I 恒在导入目标传递来的 /external:I
# 之前，因此 Engine/... 必然先命中快照，不会落到源码树上的同名头。
# 快照根由各库的 IMPORTED_LOCATION 反推，缺失即硬失败（不静默退回源码树）。
# =============================================================================
byjy_tou_kuaizhao_gen(BYJY_CORE_SNAPSHOT   EngineCore)
byjy_tou_kuaizhao_gen(BYJY_SYSTEM_SNAPSHOT EngineSystem)

# 本层各目标的包含根：下层快照在前、项目根在后（供 Application/Game/... 全路径）。
set(BYJY_APP_INC
    "${BYJY_CORE_SNAPSHOT}"
    "${BYJY_SYSTEM_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
)

# =============================================================================
# 2. 源文件收集 —— 游戏逻辑（src/）
# =============================================================================
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
)

# 导出编译数据库（供 IDE / 语言工具链读取本层包含路径与宏）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译）
file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.h"
    "${PROJECT_ROOT_DIR}/src/*.hpp"
)

# 任务<1>：把被 GLOB 的目录登记为配置依赖 —— 目录内容变化（增删文件）时，
# 生成系统自动重跑配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src"
)

if(GAME_SOURCES)
    # 排除构建目录下的文件
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER GAME_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
    # 头文件同样排除构建目录下的生成物
    list(FILTER GAME_HEADERS EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
endif()

# =============================================================================
# 3. 创建游戏层静态库（无源码时跳过，不报错）
# =============================================================================
if(GAME_SOURCES)
    add_library(GameCore STATIC ${GAME_SOURCES} ${GAME_HEADERS})

    # 包含根：下层头快照 + 项目根（供 Application/Game/... 全路径）
    target_include_directories(GameCore PUBLIC ${BYJY_APP_INC})

    # 链接下层静态库：包含路径 / 编译定义 / 第三方库由导入目标传递过来
    target_link_libraries(GameCore PUBLIC EngineCore EngineSystem)

    # 编译选项与下层保持一致
    if(MSVC)
        target_compile_options(GameCore PRIVATE /MP /utf-8)
        target_compile_definitions(GameCore PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(GameCore PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(GameCore PRIVATE _GNU_SOURCE)
    endif()

    # 静态库输出到构建目录 lib/
    set_target_properties(GameCore PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    message(STATUS "Game 源文件: ${GAME_SOURCES}")
else()
    message(STATUS "Game 暂无源码，未生成 GameCore 目标。")
endif()

# =============================================================================
# 4. 可执行文件：本层不产出
# =============================================================================
# 过去这里挂着一个组合根可执行文件（把下层接在一起，从本层启动），以及
# 「把 Test 层宿主并入本构建树」的开关。两者都已移除：
#     1) 宿主机制取消 —— 可执行文件只在 Test 层产生（EngineTests.exe）；
#     2) 组合根与宿主职责重叠，且会把下层源码拉进本构建树重复编译，
#        与「层间一律链接已构建静态库」的契约冲突。
# 现在各层的分工：子层各自构建静态库（Engine/EngineCore → EngineCore.lib、
# Engine/EngineSystem → EngineSystem.lib），本层构建 GameCore 静态库，
# Test 层构建唯一可执行文件 EngineTests.exe。