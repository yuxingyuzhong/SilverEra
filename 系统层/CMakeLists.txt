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

if(SYSTEM_SOURCES)
    # 1) 排除构建目录下的文件（防止把生成的临时文件误编译进去）
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

    # 2) 排除测试目录（Tests/ 下为功能测试代码，不编入静态库）
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/Tests/")

    # 3) 排除配置编辑器目录（含独立 main()，不应编入静态库）
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/src/gui/Config_Editor/")
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

add_library(SystemCore STATIC ${SYSTEM_SOURCES})

target_include_directories(SystemCore PUBLIC ${BYJY_SYSTEM_OUT_INC})

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
# 4. 暂未参与编译的模块
# =============================================================================
# src/gui/Config_Editor/（配置编辑器，含独立 main()）已由 引擎层 迁入本层，
# 尚未建立独立可执行目标；将来接入时在下方追加 add_executable 并链接 SystemCore，
# 同时把 系统层/external/ 下对应的包含路径补进 cmake/对外接口.cmake。
#
# 本层不再生成宿主可执行文件：层级契约统一为「各层只产出静态库，只有测试层产出
# 可执行文件」，测试层侧的分层宿主入口已移除。
