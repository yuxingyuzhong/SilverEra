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
# 依赖形态由 BYJY_ENGINE_LINK_MODE 控制：
#   AUTO     —— 默认。找得到预编译静态库就吃库，找不到退回源码编译
#   PREBUILT —— 只吃预编译静态库，找不到直接报错
#   SOURCE   —— 始终把引擎层源码拉进本次构建（旧行为，也可用于快速回退）
#
# 走预编译库时引擎层源码不进入本次构建，引擎层自身的编译错误不再中断系统层。
# 顶层统一编排时 EngineCore 已是同一构建树中的真目标，本段整体跳过，行为不变。
if(NOT TARGET EngineCore)
    set(ENGINE_LAYER_DIR "${PROJECT_ROOT_DIR}/../引擎层")
    # 依赖层的 CMakeLists 必须存在：即使吃预编译库，头文件仍需从引擎层源码树读取
    if(NOT EXISTS "${ENGINE_LAYER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到引擎层工程：${ENGINE_LAYER_DIR}\n"
            "系统层依赖引擎层，请确认两层目录同级。"
        )
    endif()

    # ---------- 1.1 链接模式与库路径 ----------
    set(BYJY_ENGINE_LINK_MODE "AUTO" CACHE STRING
        "引擎层依赖形态：AUTO=有库用库/无库编源码，PREBUILT=只用预编译库，SOURCE=始终编译源码")
    set_property(CACHE BYJY_ENGINE_LINK_MODE PROPERTY STRINGS AUTO PREBUILT SOURCE)

    # 显式指定优先；留空则自动探测 引擎层/out/build/*/lib/EngineCore.lib，多个候选取最新
    set(BYJY_ENGINE_LIB_PATH "" CACHE FILEPATH
        "引擎层预编译静态库路径；留空则自动探测")

    set(_ENGINE_LIB_PATH "")
    if(NOT BYJY_ENGINE_LINK_MODE STREQUAL "SOURCE")
        # 用户显式指定的库路径优先
        if(BYJY_ENGINE_LIB_PATH)
            if(EXISTS "${BYJY_ENGINE_LIB_PATH}")
                set(_ENGINE_LIB_PATH "${BYJY_ENGINE_LIB_PATH}")
            else()
                message(WARNING "BYJY_ENGINE_LIB_PATH 指向的文件不存在：${BYJY_ENGINE_LIB_PATH}")
            endif()
        else()
            # 自动探测：多个候选取时间戳最新的一份
            file(GLOB _ENGINE_LIB_CANDIDATES "${ENGINE_LAYER_DIR}/out/build/*/lib/EngineCore.lib")
            set(_ENGINE_LIB_STAMP 0)
            foreach(_CANDIDATE IN LISTS _ENGINE_LIB_CANDIDATES)
                file(TIMESTAMP "${_CANDIDATE}" _CANDIDATE_STAMP "%Y%m%d%H%M%S")
                if(_CANDIDATE_STAMP GREATER _ENGINE_LIB_STAMP)
                    set(_ENGINE_LIB_STAMP "${_CANDIDATE_STAMP}")
                    set(_ENGINE_LIB_PATH "${_CANDIDATE}")
                endif()
            endforeach()
        endif()
    endif()

    if(_ENGINE_LIB_PATH)
        # ---------- 1.2 命中预编译库：建立导入目标 ----------
        add_library(EngineCore STATIC IMPORTED GLOBAL)
        set_target_properties(EngineCore PROPERTIES IMPORTED_LOCATION "${_ENGINE_LIB_PATH}")

        # 复刻 EngineCore 的 PUBLIC 传递面（来源：引擎层/CMakeLists.txt 第 4–6 节），
        # 少一条的症状是上层编译期找不到头文件，藏不住。
        target_include_directories(EngineCore INTERFACE
            "${ENGINE_LAYER_DIR}"                                # 引擎层根目录
            "${ENGINE_LAYER_DIR}/external/Sol2/include"          # Sol2（纯头文件）
            "${ENGINE_LAYER_DIR}/external/Lua"                   # Lua
            "${ENGINE_LAYER_DIR}/external/Json"                  # Json（纯头文件）
            "${ENGINE_LAYER_DIR}/external/glfw"                  # GLFW
            "${ENGINE_LAYER_DIR}/external/glad/include"          # GLAD
            "${ENGINE_LAYER_DIR}/external"                       # GLM（代码里用 <glm/glm.hpp>）
            "${ENGINE_LAYER_DIR}/external/Dear_ImGui/imgui"      # Dear ImGui
            "${ENGINE_LAYER_DIR}/external/Dear_ImGui/backends"   # ImGui 后端
            "${ENGINE_LAYER_DIR}/external/stb"                   # stb
        )
        # GLFW 静态库模式宏，引擎层以 PUBLIC 传递
        target_compile_definitions(EngineCore INTERFACE GLFW_STATIC)
        # GLFW 静态库与 OpenGL
        find_package(OpenGL REQUIRED)
        target_link_libraries(EngineCore INTERFACE
            "${ENGINE_LAYER_DIR}/external/glfw/glfw3.lib"
            OpenGL::GL
        )
        # GLFW 在 MSVC 下还需的系统库
        if(MSVC)
            target_link_libraries(EngineCore INTERFACE
                opengl32.lib
                user32.lib
                gdi32.lib
                shell32.lib
            )
        endif()

        message(STATUS "引擎层依赖形态：预编译静态库 —— ${_ENGINE_LIB_PATH}")

        # ---------- 1.3 陈旧检测 ----------
        # 导入目标不会自行重建，库比源码旧时提前告警，把故障挡在链接期之前。
        file(TIMESTAMP "${_ENGINE_LIB_PATH}" _LIB_STAMP_NUM "%Y%m%d%H%M%S")
        file(TIMESTAMP "${_ENGINE_LIB_PATH}" _LIB_STAMP_TEXT "%Y-%m-%d %H:%M:%S")
        file(GLOB_RECURSE _ENGINE_SOURCES_FOR_STAMP
            "${ENGINE_LAYER_DIR}/src/*.h"    "${ENGINE_LAYER_DIR}/src/*.hpp"
            "${ENGINE_LAYER_DIR}/src/*.c"    "${ENGINE_LAYER_DIR}/src/*.cpp"
            "${ENGINE_LAYER_DIR}/common/*.h" "${ENGINE_LAYER_DIR}/common/*.hpp"
            "${ENGINE_LAYER_DIR}/common/*.c" "${ENGINE_LAYER_DIR}/common/*.cpp"
        )
        set(_NEWEST_SRC_STAMP 0)
        set(_NEWEST_SRC_FILE "")
        foreach(_SRC IN LISTS _ENGINE_SOURCES_FOR_STAMP)
            file(TIMESTAMP "${_SRC}" _SRC_STAMP "%Y%m%d%H%M%S")
            if(_SRC_STAMP GREATER _NEWEST_SRC_STAMP)
                set(_NEWEST_SRC_STAMP "${_SRC_STAMP}")
                set(_NEWEST_SRC_FILE "${_SRC}")
            endif()
        endforeach()
        if(_NEWEST_SRC_FILE AND _NEWEST_SRC_STAMP GREATER _LIB_STAMP_NUM)
            file(TIMESTAMP "${_NEWEST_SRC_FILE}" _NEWEST_SRC_TEXT "%Y-%m-%d %H:%M:%S")
            message(WARNING
                "引擎层预编译库早于引擎层源码，可能已陈旧：\n"
                "  预编译库：${_ENGINE_LIB_PATH}（${_LIB_STAMP_TEXT}）\n"
                "  最新源码：${_NEWEST_SRC_FILE}（${_NEWEST_SRC_TEXT}）\n"
                "  刷新命令：cmake --build \"${ENGINE_LAYER_DIR}/out/build/x64-Debug\" --target EngineCore"
            )
        endif()
    elseif(BYJY_ENGINE_LINK_MODE STREQUAL "PREBUILT")
        # 强制预编译库模式却没找到库：直接报错，并给出补救说明
        message(FATAL_ERROR
            "BYJY_ENGINE_LINK_MODE=PREBUILT，但未找到引擎层预编译静态库。\n"
            "  期望位置：${ENGINE_LAYER_DIR}/out/build/<配置>/lib/EngineCore.lib\n"
            "  补救方式一：先构建引擎层\n"
            "      cmake --build \"${ENGINE_LAYER_DIR}/out/build/x64-Debug\" --target EngineCore\n"
            "  补救方式二：显式指定库路径\n"
            "      cmake -S <系统层> -B <构建目录> -DBYJY_ENGINE_LIB_PATH=<EngineCore.lib 绝对路径>"
        )
    else()
        # AUTO 模式未探测到库，或用户显式要求 SOURCE：退回源码编译
        message(STATUS "引擎层依赖形态：源码编译（引擎层将进入本次构建）")
        add_subdirectory("${ENGINE_LAYER_DIR}" "${CMAKE_BINARY_DIR}/引擎层")
    endif()
endif()

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
add_library(SystemCore STATIC ${SYSTEM_SOURCES})

# 公共包含路径：
#   - 本层根目录：层内以 "entity/..." 形式互相包含时使用
#   - src/：宿主与上层以 "entity/Entity/实体.h" 形式引用时使用
#     （实体 / 属性槽已迁至 src/ 下，旧写法依赖这条路径才能解析）
target_include_directories(SystemCore PUBLIC
    "${PROJECT_ROOT_DIR}"
    "${PROJECT_ROOT_DIR}/src"
)

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
# src/effect/（效应 / 效应管理器）本轮已随路径统一重新纳入 SystemCore；
# src/gui/Config_Editor/（配置编辑器，含独立 main()）已由 引擎层 迁入本层，
# 尚未建立独立可执行目标；接入时在下方追加 add_executable 并链接 SystemCore。

# =============================================================================
# 5. 测试层宿主可执行文件接入（可选）
# =============================================================================
# 静态库不具备「启动项目」身份，单独打开本层时 VS 启动项下拉为空、也无法 F5 调试。
# 打开本层文件夹做开发时，把测试层的分层宿主可执行文件并入本次构建树，于是
# 「改本层源码 → 生成 → 本层静态库重编 → 宿主重新链接 → 运行」形成完整闭环。
#
# 本段位于文件末尾，此时 SystemCore 已经定义，测试层内的宿主可直接链接它。
# 仅当本层是构建树的顶层源码目录时生效：被其他层 add_subdirectory 拉入时，
# 由拉入者负责编排，本段保持惰性。
# 默认开启：本层单独打开时即可在 VS 启动项下拉中看到宿主可执行文件。
# 关闭方式：配置时传 -DBYJY_JOIN_TEST_HOST=OFF，或在 CMakeSettings.json 里显式置为 "false"。
# 宿主范围：本层只请求 SystemHost（引擎层 + 系统层），全栈宿主不在此处生成。
#（若想在本层的构建树里连全栈宿主一起编，把下面的 "SystemHost" 换成 "FullHost"）
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
        # 请求范围：本层自带 SystemCore，系统宿主可直接链接
        set(BYJY_HOST_REQUEST "SystemHost")
        add_subdirectory("${TEST_LAYER_DIR}" "${CMAKE_BINARY_DIR}/测试层")
    endif()
endif()
