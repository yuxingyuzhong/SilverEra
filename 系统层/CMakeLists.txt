# =============================================================================
# 系统层 —— 构建脚本
# =============================================================================
# 位置：白银纪元/系统层
# 依赖：引擎层（EngineCore）—— 系统层链接引擎层，方向单向，不反向依赖
# 产物：SystemCore 静态库
# 用法：
#     - 作为 白银纪元 顶层构建的一部分（顶层 CMakeLists 已 add_subdirectory 本目录）
#     - 或独立配置：cmake -S <本目录> -B <构建目录>
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
# 1. 依赖：引擎层
# =============================================================================
# 顶层统一编排时 EngineCore 已在同一构建树中，直接复用；
# 独立配置时（cmake -S 系统层）则把引擎层一并拉进本次构建。
if(NOT TARGET EngineCore)
    set(ENGINE_LAYER_DIR "${PROJECT_ROOT_DIR}/../引擎层")
    # 依赖层的 CMakeLists 必须存在，否则给出明确报错而不是含糊的链接失败
    if(NOT EXISTS "${ENGINE_LAYER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到引擎层工程：${ENGINE_LAYER_DIR}\n"
            "系统层依赖引擎层，请确认两层目录同级。"
        )
    endif()
    add_subdirectory("${ENGINE_LAYER_DIR}" "${CMAKE_BINARY_DIR}/引擎层")
endif()

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 当前收录 entity（实体 / 实体管理器 / 属性槽分发器）
file(GLOB_RECURSE SYSTEM_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/entity/*.cpp"
    "${PROJECT_ROOT_DIR}/entity/*.c"
)

if(SYSTEM_SOURCES)
    # 1) 排除构建目录下的文件（防止把生成的临时文件误编译进去）
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

    # 2) 排除测试目录（Tests/ 下为功能测试代码，不编入静态库）
    list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/Tests/")
endif()

if(NOT SYSTEM_SOURCES)
    message(FATAL_ERROR
        "未在 '${PROJECT_ROOT_DIR}/entity/' 下找到任何 .cpp 或 .c 文件！\n"
        "请检查目录结构是否完整。"
    )
endif()

message(STATUS "系统层源文件: ${SYSTEM_SOURCES}")

# =============================================================================
# 3. 创建系统层静态库
# =============================================================================
add_library(SystemCore STATIC ${SYSTEM_SOURCES})

# 公共包含路径：本层根目录（层内以 "entity/..." 形式互相包含时使用）
target_include_directories(SystemCore PUBLIC "${PROJECT_ROOT_DIR}")

# 链接引擎层：引擎层的公共包含路径、GLFW / OpenGL 等依赖由 EngineCore 传递过来
target_link_libraries(SystemCore PUBLIC EngineCore)

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
# effect/（效应 / 效应管理器）此前长期处于「排除编译」状态，
# 且其内部 include 仍指向迁出前的旧路径，故本轮不纳入 SystemCore，
# 待路径统一后再开启收集。
