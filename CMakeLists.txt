# =============================================================================
# 游戏层 —— 构建脚本
# =============================================================================
# 位置：白银纪元/游戏层
# 依赖：系统层（SystemCore）—— 游戏层链接系统层
# 产物：GameCore 静态库（本层当前尚无源码，目录内出现 .cpp/.c 后自动纳入）
#       TestEngine 可执行文件（组合根：把引擎层与系统层接在一起）
# 用法：
#     - 作为 白银纪元 顶层构建的一部分（顶层 CMakeLists 已 add_subdirectory 本目录）
#     - 或独立配置：cmake -S <本目录> -B <构建目录>
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
# 1. 依赖：系统层
# =============================================================================
# 系统层的 CMakeLists 内部会按需把引擎层一并拉入构建，此处无需重复处理。
if(NOT TARGET SystemCore)
    set(SYSTEM_LAYER_DIR "${PROJECT_ROOT_DIR}/../系统层")
    # 依赖层的 CMakeLists 必须存在，否则给出明确报错
    if(NOT EXISTS "${SYSTEM_LAYER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到系统层工程：${SYSTEM_LAYER_DIR}\n"
            "游戏层依赖系统层，请确认两层目录同级。"
        )
    endif()
    add_subdirectory("${SYSTEM_LAYER_DIR}" "${CMAKE_BINARY_DIR}/系统层")
endif()

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
    string(REGEX REPLACE "([.+*?^$()])" "\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER GAME_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
endif()

# =============================================================================
# 3. 创建游戏层静态库（无源码时跳过，不报错）
# =============================================================================
if(GAME_SOURCES)
    add_library(GameCore STATIC ${GAME_SOURCES})

    # 公共包含路径：本层根目录
    target_include_directories(GameCore PUBLIC "${PROJECT_ROOT_DIR}")

    # 链接系统层（引擎层的依赖由 SystemCore 继续向上传递）
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
# 4. 组合根可执行文件 —— TestEngine
# =============================================================================
# 主调文件把「事件中转站 + 配置加载器 + 属性槽分发器 + 实体管理器」接在一起，
# 同时依赖引擎层与系统层，因此其可执行目标放在本层（而非引擎层）。
#
# 注意：Engine_Env 解析资源路径的基准是「可执行文件所在目录」（exe_dir / "assets"），
#       因此 TestEngine.exe 需要与资产目录同级才能正确加载 assets/。
file(GLOB_RECURSE ENTRY_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/主调文件/*.cpp"
)

if(ENTRY_SOURCES)
    add_executable(TestEngine ${ENTRY_SOURCES})

    # 链接系统层：其 PUBLIC 依赖会继续把 EngineCore、glfw3.lib、OpenGL 传递上来
    target_link_libraries(TestEngine PRIVATE SystemCore)

    # 编译选项与下层保持一致
    if(MSVC)
        target_compile_options(TestEngine PRIVATE /MP /utf-8)
        target_compile_definitions(TestEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(TestEngine PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(TestEngine PRIVATE _GNU_SOURCE)
    endif()

    # 输出到本层根目录
    set_target_properties(TestEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT_DIR}"
    )

    # Visual Studio 中作为默认启动工程（方便调试）
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "TestEngine")

    message(STATUS "游戏层已添加: TestEngine.exe")
else()
    message(WARNING "未找到 '${PROJECT_ROOT_DIR}/主调文件/' 下的 .cpp 文件，将不会生成 TestEngine.exe。")
endif()
