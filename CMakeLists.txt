# =============================================================================
# CMake 最低版本要求（必须 ≥ 3.20，因为使用了 CONFIGURE_DEPENDS 等特性）
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

# =============================================================================
# 1. 项目基本信息
# =============================================================================
# 项目名称：引擎层（支持中文，但建议英文）
project(引擎层 LANGUAGES CXX)

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

# 强制使用 C++20 标准（Sol2 需要 C++17 以上，这里设 20 更现代）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # 如果编译器不支持 C++20 则报错
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁用 GCC/MSVC 的编译器扩展，提高跨平台兼容性

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 获取当前 CMakeLists.txt 所在目录的绝对路径（即项目根目录）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# 递归搜索引擎源码目录（src / common / external）下的所有 .cpp 和 .c 文件
# CONFIGURE_DEPENDS 让 CMake 在构建时自动检测新增/删除的源文件（不必手动重新运行 cmake）
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/common/*.cpp"
    "${PROJECT_ROOT_DIR}/external/*.cpp"
)

# ----- 过滤掉不需要的文件 -----
if(ENGINE_SOURCES)
    # 1) 排除构建目录下的文件（防止将生成的临时文件误编译进去）
    #    将反斜杠转为正斜杠，方便正则匹配
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    # 转义正则特殊字符（如 . + * ? 等）
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

endif()

# 如果过滤后没有源文件，终止配置并给出明确错误提示
if(NOT ENGINE_SOURCES)
    message(FATAL_ERROR 
        "未在 '${PROJECT_ROOT_DIR}' 的 src/、common/、external/ 下找到任何 .cpp 或 .c 文件！\n"
        "请检查目录结构是否完整，以及 CMakeLists.txt 是否放置正确。"
    )
endif()

# 打印源文件数量，方便调试
message(STATUS "引擎源文件数量: ${ENGINE_SOURCES}")

# =============================================================================
# 3. 创建引擎静态库（核心）
# =============================================================================
add_library(EngineCore STATIC ${ENGINE_SOURCES})

# =============================================================================
# 4. 头文件搜索路径设置
# =============================================================================
target_include_directories(EngineCore PRIVATE "${PROJECT_ROOT_DIR}")

# =============================================================================
# 5. 第三方库
# =============================================================================

# ---------- Json ---------- json解析库
set(JSON_ROOT "${PROJECT_ROOT_DIR}/external/Json")
target_include_directories(EngineCore PRIVATE "${JSON_ROOT}")

# ---------- GLFW ---------- OPENGL输入/窗口
set(GLFW_ROOT "${PROJECT_ROOT_DIR}/external/glfw")
target_include_directories(EngineCore PRIVATE "${GLFW_ROOT}")

# ---------- GLM ---------- 数学库
set(GLM_ROOT "${PROJECT_ROOT_DIR}/external/glm")
target_include_directories(EngineCore PRIVATE "${GLM_ROOT}")

# ---------- GLM ---------- 碰撞检测库
set(BULLET_ROOT "${PROJECT_ROOT_DIR}/external/bullet3/src")
target_include_directories(EngineCore PRIVATE "${BULLET_ROOT}")

# =============================================================================
# 6. 链接第三方库（新增）
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

# 设置输出的库文件和可执行文件的目录（统一放在 build/bin 和 build/lib）
set_target_properties(EngineCore PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"   # 静态库 .lib / .a
)

# =============================================================================
# 8. 安装规则（方便部署）
# =============================================================================
# 将引擎的头文件安装到 include/EngineCore/ 下
install(DIRECTORY "${PROJECT_ROOT_DIR}/"
    DESTINATION "include/EngineCore"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"   # 只安装 .h 和 .hpp
    PATTERN "Private" EXCLUDE                     # 排除 Private 子目录（内部实现）
)

# 将静态库安装到 lib/ 下
install(TARGETS EngineCore
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
)
