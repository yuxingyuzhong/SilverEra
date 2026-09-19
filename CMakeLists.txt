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
# 语言：C 和 C++（明确启用 C，以便 .c 文件默认按 C 编译）
project(引擎层 LANGUAGES C CXX)

# 强制使用 C++20 标准（Sol2 需要 C++17 以上，这里设 20 更现代）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # 如果编译器不支持 C++20 则报错
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁用 GCC/MSVC 的编译器扩展，提高跨平台兼容性

# =============================================================================
# 2. 源文件收集（关键：同时收集 .cpp 和 .c 文件）
# =============================================================================
# 获取当前 CMakeLists.txt 所在目录的绝对路径（即项目根目录）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# 递归搜索引擎源码目录（src / common / external）下的所有 .cpp 和 .c 文件
# CONFIGURE_DEPENDS 让 CMake 在构建时自动检测新增/删除的源文件（不必手动重新运行 cmake）
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
    "${PROJECT_ROOT_DIR}/common/*.cpp"
    "${PROJECT_ROOT_DIR}/common/*.c"
    "${PROJECT_ROOT_DIR}/external/*.cpp"
    "${PROJECT_ROOT_DIR}/external/*.c"      # 重要：Lua 官方源码是 .c，必须包含
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
# 4. 头文件搜索路径设置（使用 PUBLIC 让依赖者继承）
# =============================================================================
# PUBLIC 表示这个包含目录不仅对 EngineCore 自身可见，也对链接 EngineCore 的上层目标
#（系统层的 SystemCore、游戏层的 TestEngine）可见，上层无需重复添加同样的路径。
target_include_directories(EngineCore PUBLIC "${PROJECT_ROOT_DIR}")

# =============================================================================
# 5. 第三方库
# =============================================================================

# ---------- Sol2（纯头文件库） ----------
set(SOL2_ROOT "${PROJECT_ROOT_DIR}/external/Sol2/include")
target_include_directories(EngineCore PUBLIC "${SOL2_ROOT}")

# ---------- Lua（源码直接编译） ----------
set(LUA_SRC_DIR "${PROJECT_ROOT_DIR}/external/Lua")
target_include_directories(EngineCore PUBLIC "${LUA_SRC_DIR}")

# ---------- Json（纯头文件库） ----------
set(JSON_ROOT "${PROJECT_ROOT_DIR}/external/Json")
target_include_directories(EngineCore PUBLIC "${JSON_ROOT}")

# ---------- GLFW ----------
set(GLFW_ROOT "${PROJECT_ROOT_DIR}/external/glfw")
target_include_directories(EngineCore PUBLIC "${GLFW_ROOT}")

# ---------- GLAD ----------
set(GLAD_ROOT "${PROJECT_ROOT_DIR}/external/glad")
target_include_directories(EngineCore PUBLIC "${GLAD_ROOT}/include")

# ---------- GLM ----------
# 代码里用 #include <glm/glm.hpp>，故包含 external 目录本身
set(GLM_ROOT "${PROJECT_ROOT_DIR}/external")
target_include_directories(EngineCore PUBLIC "${GLM_ROOT}")

# ---------- Dear ImGui ----------
set(IMGUI_ROOT "${PROJECT_ROOT_DIR}/external/Dear_ImGui/imgui")
target_include_directories(EngineCore PUBLIC "${IMGUI_ROOT}")

set(IMGUI_BACKENDS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/backends")
target_include_directories(EngineCore PUBLIC "${IMGUI_BACKENDS}")

# ---------- stb ----------
set(STB_ROOT "${PROJECT_ROOT_DIR}/external/stb")
target_include_directories(EngineCore PUBLIC "${STB_ROOT}")

# ---------- 新增：公共包含路径打印 ----------

get_target_property(ENGINE_PUBLIC_INCLUDES EngineCore INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "EngineCore 公共包含路径: ${ENGINE_PUBLIC_INCLUDES}")

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
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"   # 动态库（本工程未用）
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT_DIR}"       # 👈 可执行文件输出到项目根目录
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

# =============================================================================
# 9. 测试层宿主可执行文件接入（可选）
# =============================================================================
# 静态库不具备「启动项目」身份，单独打开本层时 VS 启动项下拉为空、也无法 F5 调试。
# 打开本层文件夹做开发时，把测试层的分层宿主可执行文件并入本次构建树，于是
# 「改本层源码 → 生成 → 本层静态库重编 → 宿主重新链接 → 运行」形成完整闭环。
#
# 仅当本层是构建树的顶层源码目录时生效：被其他层 add_subdirectory 拉入时，
# 由拉入者负责编排，本段保持惰性。这同时断开了
# 「系统层 → 引擎层 → 测试层 → 系统层」这类环路（重入时 SystemCore 尚未定义，
# 若在此处接入测试层，宿主会链接到不存在的目标）。
# 默认开启：本层单独打开时即可在 VS 启动项下拉中看到宿主可执行文件。
# 关闭方式：配置时传 -DBYJY_JOIN_TEST_HOST=OFF，或在 CMakeSettings.json 里显式置为 "false"。
# 宿主范围：本层只请求 EngineHost —— 系统层源码不会进入本构建树，
# 因此在引擎层里生成时不会再被系统层的编译错误打断。
#（若想在本层的构建树里连系统层一起编，把下面的 "EngineHost" 换成 "SystemHost" 或 "FullHost"）
option(BYJY_JOIN_TEST_HOST "把测试层宿主可执行文件并入本构建树" ON)
if(BYJY_JOIN_TEST_HOST
   AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
   AND NOT BYJY_TEST_HOST_JOINED)
    set(TEST_LAYER_DIR "${PROJECT_ROOT_DIR}/../测试层")
    # 本层源码被单独复制分发时，同级目录下可能没有测试层，此时安静跳过，不让配置失败
    if(NOT EXISTS "${TEST_LAYER_DIR}/CMakeLists.txt")
        message(STATUS "未找到测试层工程（${TEST_LAYER_DIR}），跳过宿主接入")
    else()
        # 环路保护的二次防护：用普通变量（不写入缓存），避免污染 CMakeCache 影响后续配置
        set(BYJY_TEST_HOST_JOINED TRUE)
        # 请求范围：只起引擎层组件，系统层不进本构建树
        set(BYJY_HOST_REQUEST "EngineHost")
        add_subdirectory("${TEST_LAYER_DIR}" "${CMAKE_BINARY_DIR}/测试层")
    endif()
endif()
