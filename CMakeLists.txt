# =============================================================================
# EngineCore —— 构建脚本
# =============================================================================
# 位置：白银纪元/Engine/EngineCore
# 依赖：无（拓扑最底层）
# 产物：EngineCore.lib（静态库）
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 包含路径：项目内部一律写「项目根相对全路径」——
#           本层头   Engine/EngineCore/...
#           编译包含根 = 项目根（本层不消费任何下层头，无需头快照）
# 层间契约：
#     cmake/对外接口.cmake   本层对外自描述（前缀 BYJY_ENGINE_CORE）
#     cmake/导出头快照.cmake 对外头快照导出脚本（上层按与本层库同版次的头编译）
# 说明：本层不含 cmake/接入下层.cmake —— 无下层可接。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

# =============================================================================
# 1. 项目基本信息
# =============================================================================
project(EngineCore LANGUAGES C CXX)

# 导出编译数据库（供 IDE / 语言工具链读取本层的包含路径与宏；Ninja 生成器默认不产出该文件）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# 归档响应文件 BOM 包装器（MSVC + Ninja 专用）
# =============================================================================
# 背景：Ninja 写出的归档响应文件（*.rsp）是 UTF-8 无 BOM，而 MSVC 的 lib.exe
#       在没有 BOM 时按系统 ANSI 代码页解读文件内容，中文对象名会被解成乱码，
#       归档阶段报 LNK1181。
# 做法：把归档命令换成「先给响应文件补 BOM，再调用真实归档器」的包装脚本，
#       使源文件名保持中文、bullet3 继续留在 EngineCore 内。
# 说明：CMAKE_CXX_CREATE_STATIC_LIBRARY 必须在 project() 之后设置，否则会被
#       Platform/Windows-MSVC.cmake 的默认值覆盖回去；只对 Ninja 生成器生效，
#       避免影响 Visual Studio 生成器（其归档由 MSBuild 的 Lib 任务完成）。
# =============================================================================
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    # 此处尚未执行下方的 get_filename_component，故用 CMAKE_CURRENT_SOURCE_DIR 定位
    set(BYJY_AR_WRAPPER "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ar_rsp_bom.ps1")
    find_program(BYJY_POWERSHELL_EXECUTABLE NAMES powershell powershell.exe)

    # 包装脚本与 PowerShell 都就位时才改写归档命令
    if(EXISTS "${BYJY_AR_WRAPPER}" AND BYJY_POWERSHELL_EXECUTABLE)
        set(CMAKE_CXX_CREATE_STATIC_LIBRARY
            "${BYJY_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass -File ${BYJY_AR_WRAPPER} <CMAKE_AR> /nologo <LINK_FLAGS> /out:<TARGET> <OBJECTS> ")
        message(STATUS "归档响应文件 BOM 包装器已启用：${BYJY_AR_WRAPPER}")
    else()
        message(WARNING "未找到归档包装器或 PowerShell，中文对象名的归档可能报 LNK1181：${BYJY_AR_WRAPPER}")
    endif()
endif()

# 强制使用 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # 如果编译器不支持 C++20 则报错
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁用 GCC/MSVC 的编译器扩展，提高跨平台兼容性

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 本层根目录；项目根 = Engine/EngineCore 上溯两级（项目内部包含路径的基准）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${PROJECT_ROOT_DIR}/../.." ABSOLUTE)

# 递归搜索引擎源码目录（src / common / external）下的所有 .cpp 和 .c 文件
# CONFIGURE_DEPENDS 让 CMake 在构建时自动检测新增/删除的源文件（不必手动重新运行 cmake）
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/common/*.cpp"
    "${PROJECT_ROOT_DIR}/external/*.cpp"
)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译），
# external/ 下第三方头文件不纳入以免污染索引
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.h"
    "${PROJECT_ROOT_DIR}/src/*.hpp"
    "${PROJECT_ROOT_DIR}/common/*.h"
    "${PROJECT_ROOT_DIR}/common/*.hpp"
)

# 任务<1>：把被 GLOB 的目录登记为配置依赖 —— 目录内容变化（增删文件）时，
# 生成系统自动重跑配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
# 说明：GLOB_RECURSE 覆盖子目录，但 CMAKE_CONFIGURE_DEPENDS 要逐个目录登记。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src"
    "${PROJECT_ROOT_DIR}/common"
    "${PROJECT_ROOT_DIR}/external"
)

# ----- 过滤掉不需要的文件 -----
if(ENGINE_SOURCES)
    # 1) 排除构建目录下的文件（防止将生成的临时文件误编译进去）
    #    将反斜杠转为正斜杠，方便正则匹配
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    # 转义正则特殊字符（如 . + * ? 等）
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
    # 头文件同样排除构建目录下的生成物
    list(FILTER ENGINE_HEADERS EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

endif()

# 如果过滤后没有源文件，终止配置并给出明确错误提示
if(NOT ENGINE_SOURCES)
    message(FATAL_ERROR 
        "未在 '${PROJECT_ROOT_DIR}' 的 src/、common/、external/ 下找到任何 .cpp 或 .c 文件！\n"
        "请检查目录结构是否完整，以及 CMakeLists.txt 是否放置正确。"
    )
endif()

# 打印源文件数量，方便调试
message(STATUS "EngineCore 源文件数量: ${ENGINE_SOURCES}")

# =============================================================================
# 3. 创建引擎静态库（核心）
# =============================================================================
add_library(EngineCore STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# =============================================================================
# 4. 头文件搜索路径设置
# =============================================================================
# 私有包含根 = 项目根：本层源码以 Engine/EngineCore/... 形式互相包含，
# 与对外面使用同一套写法，彻底消除「直接 src/... 包含」的路径归属不明与重名问题
target_include_directories(EngineCore PRIVATE "${BYJY_PROJECT_ROOT}")

# =============================================================================
# 5. 第三方库
# =============================================================================

# ---------- Json ---------- json解析库
set(JSON_ROOT "${PROJECT_ROOT_DIR}/external/Json")
target_include_directories(EngineCore PRIVATE "${JSON_ROOT}")

# ---------- GLFW ---------- OPENGL输入/窗口
set(GLFW_ROOT "${PROJECT_ROOT_DIR}/external/glfw")
target_include_directories(EngineCore PRIVATE "${GLFW_ROOT}")

# ---------- bullet3 ---------- 碰撞检测库
set(BULLET_ROOT "${PROJECT_ROOT_DIR}/external/bullet3/src")
target_include_directories(EngineCore PRIVATE "${BULLET_ROOT}")

# =============================================================================
# 5.5 对外头快照导出（供上层按「与本层静态库同版次」的头编译）
# =============================================================================
# 背景：上层的包含目录此前直接指向本层源码树，与本层构建目录里的 .lib 无绑定关系，
#       一旦「头已改、库未重建」，上层就会新头配旧库地编译链接。
# 做法：每轮构建把对外包含目录下的头镜像到 <构建目录>/include/，上层改读该快照；
#       只复制内容有变化的头，未变头不改变时间戳，不会引起上层无谓重编。
# 路径形态：镜像目录相对「项目根」保留完整层级 —— 快照根/Engine/EngineCore/...
#           上层把对外面里的「项目根」条目换成该快照根即可无缝解析同一个前缀。
# 镜像范围：只镜像本层自有头（common、src）。第三方（external/Json、external/glfw、
#           external/bullet3/src）在外对面上是独立的绝对路径条目，不经过快照解析，
#           也就不必镜像 —— 少复制上千个第三方头，构建更快。
# =============================================================================
set(BYJY_ENGINE_CORE_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include")

add_custom_target(EngineCoreIncludeSnapshot ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${BYJY_PROJECT_ROOT}"
        "-DDST_ROOT=${BYJY_ENGINE_CORE_INCLUDE_SNAPSHOT_DIR}"
        "-DMIRROR_DIRS=Engine/EngineCore/common;Engine/EngineCore/src"
        -P "${PROJECT_ROOT_DIR}/cmake/导出头快照.cmake"
    COMMENT "导出 EngineCore 对外头快照 → ${BYJY_ENGINE_CORE_INCLUDE_SNAPSHOT_DIR}"
    VERBATIM
)

# =============================================================================
# 6. 链接第三方库
# =============================================================================

# 查找 OpenGL 库（跨平台）
find_package(OpenGL REQUIRED)

# 链接 GLFW 静态库和 OpenGL
target_link_libraries(EngineCore PUBLIC
    "${GLFW_ROOT}/glfw3.lib"
    OpenGL::GL
)

# 定义 GLFW_STATIC 宏，确保 GLFW 头文件使用静态库模式
target_compile_definitions(EngineCore PUBLIC GLFW_STATIC)

# 如果是 MSVC，补充 Windows 系统库（GLFW 依赖）
if(MSVC)
    target_link_libraries(EngineCore PUBLIC
        opengl32.lib
        user32.lib
        gdi32.lib
        shell32.lib
    )
endif()

# =============================================================================
# 7. 平台兼容性设置（编译选项、宏定义、输出目录）
# =============================================================================
if(MSVC)
    # MSVC 特有选项：
    # /MP   : 多核编译加速
    # /utf-8: 指定源文件字符集为 UTF-8，避免中文乱码警告
    target_compile_options(EngineCore PRIVATE /MP /utf-8)
    # 禁用某些安全警告（如 fopen 等），因为 Lua 使用传统 C 函数
    target_compile_definitions(EngineCore PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # GCC / Clang 通用警告选项（严苛但有助于发现潜在问题）
    target_compile_options(EngineCore PRIVATE -Wall -Wextra -pedantic)
    # GNU/Linux 需要的宏定义
    target_compile_definitions(EngineCore PRIVATE _GNU_SOURCE)
endif()

# 设置输出的库文件和可执行文件的目录（统一放在 build/lib）
set_target_properties(EngineCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"   # 静态库 .lib / .a
)

# =============================================================================
# 8. 安装规则（方便部署）
# =============================================================================
# 头文件按「项目根相对」层级安装到 include/Engine/EngineCore/，
# 与源码内 Engine/EngineCore/... 的包含写法保持一致
install(DIRECTORY "${PROJECT_ROOT_DIR}/"
    DESTINATION "include/Engine/EngineCore"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"   # 只安装 .h 和 .hpp
    PATTERN "Private" EXCLUDE                     # 排除 Private 子目录（内部实现）
)

# 将静态库安装到 lib/ 下
install(TARGETS EngineCore
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
)