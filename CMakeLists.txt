# =============================================================================
# 游戏层 —— 构建脚本
# =============================================================================
# 位置：白银纪元/游戏层
# 依赖：系统层（SystemCore）—— 一律链接「已经构建好的」静态库
# 产物：GameCore 静态库（本层当前尚无源码，src/ 下出现 .cpp/.c 后自动纳入）
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先构建系统层。本层不拉入任何下层源码，
#           也没有源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 层间契约：
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，系统层 / 测试层 / 本层逐字节相同
# 说明：
#     - 本层不产出可执行文件。宿主机制取消后，可执行文件只在测试层产生
#       （EngineTests.exe）；游戏本体将来需要入口时，再在本层单独开一个可执行目标。
#     - 静态库输出到构建目录 lib/，供更上层或测试层链接。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(游戏层 LANGUAGES C CXX)

# 强制使用 C++20 标准（与下层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# =============================================================================
# 1. 接入下层：系统层 —— 链接已构建好的静态库
# =============================================================================
# 本层只消费下层「已经构建好的」静态库：下层源码不进入本次构建，
# 也没有任何源码回退路径（层级契约统一）。
#
# 库定位顺序由 cmake/接入下层.cmake 的 byjy_jieru_xiaceng() 统一实现：
#   ① 覆盖变量非空 → 用它；指向的文件不存在则直接报错
#   ② 留空 → 自动探测 <下层>/out/build/*/lib/<库名>.lib，取时间戳最新的一份
#   ③ 都没有 → FATAL_ERROR，并提示先构建下层
#
# 系统层的对外面已并入引擎层的对外面，接入系统层即同时取得两层的公共路径。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_SYSTEM_LIB_PATH "" CACHE FILEPATH
    "系统层静态库路径；留空则自动探测 系统层/out/build/*/lib/SystemCore.lib")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../系统层/cmake/对外接口.cmake"
    "BYJY_SYSTEM"
    "BYJY_SYSTEM_LIB_PATH"
)

# =============================================================================
# 2. 源文件收集 —— 游戏逻辑（src/）
# =============================================================================
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
)

if(GAME_SOURCES)
    # 排除构建目录下的文件
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER GAME_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
endif()

# =============================================================================
# 3. 创建游戏层静态库（无源码时跳过，不报错）
# =============================================================================
if(GAME_SOURCES)
    add_library(GameCore STATIC ${GAME_SOURCES})

    # 公共包含路径：本层根目录
    target_include_directories(GameCore PUBLIC "${PROJECT_ROOT_DIR}")

    # 链接系统层：包含路径 / 编译定义 / 第三方库由导入目标传递过来
    target_link_libraries(GameCore PUBLIC SystemCore)

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

    message(STATUS "游戏层源文件: ${GAME_SOURCES}")
else()
    message(STATUS "游戏层暂无源码，未生成 GameCore 目标。")
endif()

# =============================================================================
# 4. 可执行文件：本层不产出
# =============================================================================
# 过去这里挂着一个组合根可执行文件（把引擎层与系统层接在一起，从本层启动），
# 以及「把测试层宿主并入本构建树」的开关。两者都已移除：
#     1) 宿主机制取消 —— 可执行文件只在测试层产生（EngineTests.exe），
#        各层不再各自维护一个可运行的入口；
#     2) 组合根与宿主职责重叠，且会把下层源码拉进本构建树重复编译，
#        与「层间一律链接已构建静态库」的契约冲突。
# 现在各层的分工：下层各自构建静态库（引擎层 EngineCore、系统层 SystemCore），
# 本层构建 GameCore 静态库，测试层构建唯一可执行文件 EngineTests.exe。
