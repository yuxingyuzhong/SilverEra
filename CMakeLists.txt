# =============================================================================
# Test —— 构建脚本
# =============================================================================
# 位置：白银纪元/Application/Test
# 依赖：Engine 聚合层（对外转发 EngineSystem.lib + EngineCore.lib）—— 一律链接「已经构建好的」静态库
# 产物：TestCore 静态库（src/ 下的功能测试器集合，当前为空）
#       TestLauncher 静态库（src/主调/ 下的测试选择模型与控制台菜单）
#       TestGui 静态库（external/Dear_ImGui + external/glad，供图形选择窗口使用）
#       EngineTests.exe（本层唯一可执行文件，main 由 src/主调/主程序.cpp 提供）
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先依次构建 Engine/EngineCore 与 Engine/EngineSystem。本层不拉入任何下层源码，
#           也没有源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 包含路径：项目内部一律写「项目根相对全路径」——
#           引擎头   Engine/EngineCore/...、Engine/EngineSystem/...
#           本层自身 Application/Test/...
#           编译包含根 = [下层头快照根, ..., 项目根]（快照在前，见第 1.5 节，次序不可颠倒）
# 层间契约：
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，EngineSystem / Engine / Test / Game 逐字节相同
# 说明：
#     - 本层不再产出分层宿主（引擎宿主 / 系统宿主 / 全栈宿主）。宿主机制取消后，
#       可执行文件只在测试层产生：EngineTests.exe 既是单元测试入口，也是启动项。
#     - 用例统一放在 src/单元测试/ 下，用 OBJECT 库收集而非静态库：静态库里未被
#       引用的 .obj 会被链接器丢弃，TEST 宏的静态注册会随之丢失，用例便静默不跑。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(Test LANGUAGES C CXX)

# 强制使用 C++20 标准（与下层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录；项目根供 Application/Test/... 全路径解析
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
#   ② 留空 → 对每个库，先看 <该库所在层目录>/out/build/x64-Debug/lib/<库名>.lib；
#            没有则扫 <该库所在层目录>/out/build/*/lib/<库名>.lib，取时间戳最新的一份
#            （双构建树并存时，自动探测固定默认 Debug；要 Release 请用 ①）
#   ③ 都没有 → FATAL_ERROR，并提示先构建产出该库的下层
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE STRING
    "Engine 聚合层静态库路径（分号列表，顺序同 EngineSystem;EngineCore）；留空则自动探测（各库优先 out/build/x64-Debug/lib，其次 out/build/*/lib 取最新）")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../../Engine/cmake/对外接口.cmake"
    "BYJY_ENGINE"
    "BYJY_ENGINE_LIB_PATH"
)

# =============================================================================
# 1.2 接入渲染层 —— 链接已构建好的 RendererCore.lib
# =============================================================================
# 渲染层与 Test / Game 平级，且只接 EngineCore 单层（不走 Engine 聚合面）；
# 其对外面内含 EngineCore 对外面，故接入即取得引擎侧公共路径与 Vulkan 链接库。
# 渲染层公共头不暴露 SDL / Vulkan / VMA 类型，本层按公共面（渲染器.h、网格数据.h）
# 编写用例即可，无需、也不应引入 Vulkan 头。
set(BYJY_RENDERER_LIB_PATH "" CACHE STRING
    "渲染层静态库路径（分号列表，单库 RendererCore）；留空则自动探测（Application/Renderer/out/build/x64-Debug/lib 优先）")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../Renderer/cmake/对外接口.cmake"
    "BYJY_RENDERER"
    "BYJY_RENDERER_LIB_PATH"
)

# =============================================================================
# 1.5 下层头快照接入（本层专用）
# =============================================================================
# 本层按「与下层静态库同版次」的头文件编译：把两个子层的头快照根作为「本层自身的
# 包含目录」列出，并排在项目根之前 —— 目标自身的 -I 恒在导入目标传递来的 /external:I
# 之前，因此 Engine/... 必然先命中快照，不会落到源码树上的同名头。
# 快照根由各库的 IMPORTED_LOCATION 反推，缺失即硬失败（不静默退回源码树）。
# =============================================================================
byjy_tou_kuaizhao_gen(BYJY_CORE_SNAPSHOT     EngineCore)
byjy_tou_kuaizhao_gen(BYJY_SYSTEM_SNAPSHOT   EngineSystem)
byjy_tou_kuaizhao_gen(BYJY_RENDERER_SNAPSHOT RendererCore)

# 本层各目标的包含根：下层快照在前、项目根在后（供「头文件」写 Application/Test/... 全路径）。
# 渲染层快照排在项目根之前，使 Application/Renderer/... 先命中快照、不落到源码树。
# 末位补本层根：本层实现文件（.cpp）的跨目录包含写「本层根相对」src/...，无需背项目根前缀。
set(BYJY_APP_INC
    "${BYJY_CORE_SNAPSHOT}"
    "${BYJY_SYSTEM_SNAPSHOT}"
    "${BYJY_RENDERER_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
    "${PROJECT_ROOT_DIR}"
)

# =============================================================================
# 1.6 渲染层着色器拷贝
# =============================================================================
# 渲染层以 SPIR-V 在运行期加载着色器：Renderer_Config::shader_dir 为空时按
# 「相对可执行文件目录的 shaders」解析。着色器与 RendererCore.lib 同版次，
# 故从其 IMPORTED_LOCATION 同源推导着色器目录，配置期拷到本层根目录 shaders/。
get_target_property(_BYJY_RENDERER_LIB_FILE RendererCore IMPORTED_LOCATION)
get_filename_component(_BYJY_RENDERER_BUILD_DIR "${_BYJY_RENDERER_LIB_FILE}/../.." ABSOLUTE)
set(BYJY_RENDERER_SHADERS "${_BYJY_RENDERER_BUILD_DIR}/shaders")
if(IS_DIRECTORY "${BYJY_RENDERER_SHADERS}")
    file(COPY "${BYJY_RENDERER_SHADERS}" DESTINATION "${PROJECT_ROOT_DIR}")
    message(STATUS "渲染层着色器已拷贝：${BYJY_RENDERER_SHADERS} → ${PROJECT_ROOT_DIR}/shaders")
else()
    message(WARNING "未找到渲染层着色器目录：${BYJY_RENDERER_SHADERS}（渲染层用例需要它，请先构建渲染层）")
endif()

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 收录 src/ 下的功能测试代码
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
)

# 导出编译数据库（供 IDE / 语言工具链读取本层包含路径与宏）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译）
file(GLOB_RECURSE TEST_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.h"
    "${PROJECT_ROOT_DIR}/src/*.hpp"
)

# 主调头文件（TestLauncher 与 EngineTests 共用，单独收编以纳入索引）
file(GLOB_RECURSE MAIN_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/主调/*.h"
    "${PROJECT_ROOT_DIR}/src/主调/*.hpp"
)

# 本层各目标共用同一份生成脚本（与 EngineCore 共用，属构建工具而非层间契约）。
# 用途：把 GLOB 扫描结果落成显式清单（target_sources 形式），使 VS 文件视图的
#       改名 / 迁移操作有「可改写的引用」，从而触发 CMake 重新配置；原理与约束
#       见 Engine/EngineCore/cmake/生成源文件列表.cmake 头部说明。
# 说明：本层有多个目标，故按目标各一份清单文件 —— 目标多为条件创建，逐目标清单
#       可随目标所在的条件块按需 include，不会引用到不存在的目标。
include("${PROJECT_ROOT_DIR}/../../Engine/EngineCore/cmake/生成源文件列表.cmake")

# 占位空列表：供「只登记头文件、无源文件」的清单调用使用（同步函数要求传源 / 头两个变量名）
set(BYJY_NO_SOURCES "")

# 任务<1>：把被 GLOB 的目录登记为配置依赖 —— 目录内容变化（增删文件）时，
# 生成系统自动重跑配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
# 说明：GLOB_RECURSE 覆盖子目录，但 CMAKE_CONFIGURE_DEPENDS 要逐个目录登记，
#       故这里登记三个 GLOB 基点目录（src / src/单元测试 / src/主调）。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src"
    "${PROJECT_ROOT_DIR}/src/单元测试"
    "${PROJECT_ROOT_DIR}/src/主调"
)

if(TEST_SOURCES)
    # 排除构建目录下的文件（防止把生成的临时文件误编译进去）
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER TEST_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
endif()

# 单元测试用例不并入 TestCore：它们由第 5 节的独立目标收编。
# 若并入静态库，未被引用的 .obj 会连 TEST 宏的注册一起被链接器丢弃，用例便静默不跑
list(FILTER TEST_SOURCES EXCLUDE REGEX "/src/单元测试/")

# 主调代码（含 main 与图形窗口）也不并入 TestCore：它是可执行入口，不是库内容
list(FILTER TEST_SOURCES EXCLUDE REGEX "/src/主调/")

# 头文件同样排除用例与主调目录（它们分别由下方独立目标收编）
list(FILTER TEST_HEADERS EXCLUDE REGEX "/src/单元测试/")
list(FILTER TEST_HEADERS EXCLUDE REGEX "/src/主调/")

# src/ 下暂无源文件时不视为错误：分阶段开发中测试代码尚未落位属正常状态，
# 此时不产出 TestCore，EngineTests 仍可只靠下层静态库独立生成
if(NOT TEST_SOURCES)
    message(STATUS "测试层 src/ 下暂无源文件，跳过 TestCore 静态库")
endif()

if(TEST_SOURCES)
    message(STATUS "测试层源文件: ${TEST_SOURCES}")

    # =========================================================================
    # 3. 创建测试层静态库
    # =========================================================================
    # 源与头经 源文件列表_TestCore.cmake 的 target_sources 提供（理由见该清单头部）
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_TestCore.cmake"
        "${PROJECT_ROOT_DIR}"
        TestCore
        TEST_SOURCES
        TEST_HEADERS
        "Application/Test"
    )
    add_library(TestCore STATIC)
    include("${PROJECT_ROOT_DIR}/源文件列表_TestCore.cmake")

    # 包含根：下层头快照 + 项目根（供 Application/Test/... 全路径）
    target_include_directories(TestCore PUBLIC ${BYJY_APP_INC})

    # 链接下层静态库：包含路径 / 编译定义 / 第三方库由导入目标传递过来
    target_link_libraries(TestCore PUBLIC EngineCore EngineSystem RendererCore)

    # 编译选项与下层保持一致
    if(MSVC)
        target_compile_options(TestCore PRIVATE /MP /utf-8)
        target_compile_definitions(TestCore PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(TestCore PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(TestCore PRIVATE _GNU_SOURCE)
    endif()

    # 静态库输出到构建目录 lib/（与下层一致）
    set_target_properties(TestCore PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
endif()

# =============================================================================
# 3.5 GUI 第三方接入（Dear ImGui；平台后端 SDL3 + 渲染后端 Vulkan）
# =============================================================================
# 供主调的图形选择窗口使用。与 external/googletest 同构：第三方源码以副本形式
# 放在本层 external/ 下，由本层自行编译，不改动下层的构建与对外接口。
# 说明：
#   - 不编 imgui_demo.cpp（示例窗口，本层用不到）
#   - 选择窗口改为经渲染层 UI 通道绘制：平台后端用 imgui_impl_sdl3.cpp、渲染后端用
#     imgui_impl_vulkan.cpp，不再使用 glfw / opengl3 后端与 glad
#   - Vulkan SDK 与 SDL3 的公共包含根、vulkan-1.lib 与 SDL3 系统库由 RendererCore 的对外面传递
if(EXISTS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/imgui/imgui.cpp"
   AND EXISTS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/backends/imgui_impl_sdl3.cpp"
   AND EXISTS "${PROJECT_ROOT_DIR}/external/Dear_ImGui/backends/imgui_impl_vulkan.cpp")
    set(BYJY_IMGUI_DIR "${PROJECT_ROOT_DIR}/external/Dear_ImGui")

    add_library(TestGui STATIC
        "${BYJY_IMGUI_DIR}/imgui/imgui.cpp"
        "${BYJY_IMGUI_DIR}/imgui/imgui_draw.cpp"
        "${BYJY_IMGUI_DIR}/imgui/imgui_tables.cpp"
        "${BYJY_IMGUI_DIR}/imgui/imgui_widgets.cpp"
        "${BYJY_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
        "${BYJY_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
    )

    # 包含目录：imgui 核心与后端；SDL3 / Vulkan 头与链接库随 RendererCore 的对外面传递
    target_include_directories(TestGui PUBLIC
        "${BYJY_IMGUI_DIR}/imgui"
        "${BYJY_IMGUI_DIR}/backends"
    )
    target_link_libraries(TestGui PUBLIC EngineCore RendererCore)

    if(MSVC)
        target_compile_options(TestGui PRIVATE /MP /utf-8)
        target_compile_definitions(TestGui PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    set_target_properties(TestGui PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
else()
    message(FATAL_ERROR "未找到 external/Dear_ImGui 或 external/glad：图形选择窗口依赖它们，请先还原第三方副本")
endif()

# =============================================================================
# 4. 单元测试接入（googletest）
# =============================================================================
# 用例统一放在 src/单元测试/ 下，按被测层分「工具」「核心」两棵子树。
# 说明：googletest 以源码形式内嵌在 external/googletest，并以 EXCLUDE_FROM_ALL 接入：
#       只在被测试目标依赖时编译，不改变静态库的既有构建范围。
if(EXISTS "${PROJECT_ROOT_DIR}/external/googletest/CMakeLists.txt")
    #不安装 gtest、不构建 gmock；强制共享运行时库，避免与下层的 CRT 设置冲突（LNK2038）
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory("${PROJECT_ROOT_DIR}/external/googletest"
        "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
else()
    message(STATUS "未找到 external/googletest，跳过单元测试接入")
endif()

# 用例源收集
file(GLOB_RECURSE BYJY_TEST_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/单元测试/*.cpp"
)

# 用例头文件：同样纳入索引（不参与编译）
file(GLOB_RECURSE BYJY_TEST_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/单元测试/*.h"
    "${PROJECT_ROOT_DIR}/src/单元测试/*.hpp"
)

# 用例尚未落位时不视为错误，构建不因此中断
if(NOT BYJY_TEST_SOURCES)
    message(STATUS "src/单元测试/ 下暂无用例，跳过 EngineTests 与 EngineTestObjects")
elseif(NOT TARGET GTest::gtest)
    message(STATUS "gtest 目标不可用，跳过 EngineTests 与 EngineTestObjects")
else()
    message(STATUS "测试层用例文件: ${BYJY_TEST_SOURCES}")

    # =========================================================================
    # 4.5 测试选择模型（主调逻辑，供主程序与用例共用）
    # =========================================================================
    # 只收纯逻辑部分：图形窗口依赖 ImGui，单独编进 EngineTests，不并入此库，
    # 使用例不必拖上图形库。
    # 主调头文件（MAIN_HEADERS）经 源文件列表_TestLauncher.cmake 的 target_sources
    # 提供，使 VS 文件视图对其改名 / 迁移时可改写引用并触发重配；两个 .cpp 是固定条目，
    # 仍写字面量（本就不属 GLOB 产物）。
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_TestLauncher.cmake"
        "${PROJECT_ROOT_DIR}"
        TestLauncher
        BYJY_NO_SOURCES
        MAIN_HEADERS
        "Application/Test"
    )
    add_library(TestLauncher STATIC
        "${PROJECT_ROOT_DIR}/src/主调/测试选择模型.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/控制台选择菜单.cpp"
    )
    include("${PROJECT_ROOT_DIR}/源文件列表_TestLauncher.cmake")
    # 主调实现文件以本层根相对（src/主调/...）包含本层头，故包含根 = 项目根 + 本层根
    #（与 EngineCore 本轮确立的「头写项目根相对、.cpp 写本层根相对」双基准约定一致）
    target_include_directories(TestLauncher PUBLIC "${BYJY_PROJECT_ROOT}" "${PROJECT_ROOT_DIR}")
    target_link_libraries(TestLauncher PUBLIC GTest::gtest)
    if(MSVC)
        target_compile_options(TestLauncher PRIVATE /MP /utf-8)
        target_compile_definitions(TestLauncher PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    set_target_properties(TestLauncher PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    # 用例源与头经 源文件列表_EngineTestObjects.cmake 的 target_sources 提供
    #（理由见该清单头部说明）
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_EngineTestObjects.cmake"
        "${PROJECT_ROOT_DIR}"
        EngineTestObjects
        BYJY_TEST_SOURCES
        BYJY_TEST_HEADERS
        "Application/Test"
    )
    #用例目标：编一次，EngineTests 取一份 .obj
    add_library(EngineTestObjects OBJECT)
    include("${PROJECT_ROOT_DIR}/源文件列表_EngineTestObjects.cmake")

    #包含根：下层快照 + 项目根（用例要 include "Application/Test/src/主调/测试选择模型.h"）
    target_include_directories(EngineTestObjects PUBLIC ${BYJY_APP_INC})

    #链接下层静态库以取得对外的包含路径；从 gtest 取得框架头文件路径；并由 TestLauncher 取得本层自身根
    target_link_libraries(EngineTestObjects PUBLIC EngineCore EngineSystem RendererCore GTest::gtest TestLauncher)

    #编译选项与下层保持一致
    if(MSVC)
        target_compile_options(EngineTestObjects PRIVATE /MP /utf-8)
        target_compile_definitions(EngineTestObjects PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(EngineTestObjects PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(EngineTestObjects PRIVATE _GNU_SOURCE)
    endif()

    # =========================================================================
    # 5. 独立测试可执行文件（本层唯一的可执行产物）
    # =========================================================================
    # main 由本层主调提供（src/主调/主程序.cpp），不再链接 GTest::gtest_main；
    # 图形选择窗口（依赖 ImGui）只编进本可执行文件。
    # 产物落在本层根目录，与 assets/ 同级
    #（Engine_Env 以「可执行文件所在目录」为基准解析资源路径，这是硬性要求）
    # 主调头文件（MAIN_HEADERS）经 源文件列表_EngineTests.cmake 的 target_sources
    # 提供；$<TARGET_OBJECTS> 与三个 .cpp 是固定条目，仍写字面量。
    byjy_source_list_sync(
        "${PROJECT_ROOT_DIR}/源文件列表_EngineTests.cmake"
        "${PROJECT_ROOT_DIR}"
        EngineTests
        BYJY_NO_SOURCES
        MAIN_HEADERS
        "Application/Test"
    )
    add_executable(EngineTests
        $<TARGET_OBJECTS:EngineTestObjects>
        "${PROJECT_ROOT_DIR}/src/主调/主程序.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/图形操作界面.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/测试界面.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/测试运行器.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/gtest输出镜像.cpp"
        "${PROJECT_ROOT_DIR}/src/主调/引擎日志屏蔽.cpp"
    )
    include("${PROJECT_ROOT_DIR}/源文件列表_EngineTests.cmake")
    target_include_directories(EngineTests PRIVATE ${BYJY_APP_INC})
    target_link_libraries(EngineTests PRIVATE
        EngineTestObjects EngineCore EngineSystem RendererCore
        GTest::gtest TestLauncher TestGui
    )
    set_target_properties(EngineTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT_DIR}"
    )

    if(MSVC)
        target_compile_options(EngineTests PRIVATE /MP /utf-8)
    endif()

    #注册 ctest
    enable_testing()

    # gtest_discover_tests 会以 --gtest_output=json:<文件> 枚举用例，
    # 而 googletest 写该报告文件时按 ANSI 代码页解析路径：工程路径含中文时必然失败
    #（[FATAL] Unable to open file ...\\cmake_test_discovery_*.json），构建会被判失败。
    # 故路径为纯 ASCII 时才启用逐用例注册，否则退化为整体注册一个用例。
    if("${PROJECT_ROOT_DIR}" MATCHES "^[ -~]+$")
        message(STATUS "工程路径为纯 ASCII，启用逐用例注册（gtest_discover_tests）")
        include(GoogleTest)
        gtest_discover_tests(EngineTests
            WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
        )
    else()
        message(STATUS "工程路径含非 ASCII 字符，EngineTests 改为整体注册（逐用例）
   请直接运行 EngineTests.exe --gtest_filter=xxx 筛选用例")
        # 不开窗直接全跑：无参数的 ctest 不会阻塞在图形窗口上
        add_test(NAME EngineTests COMMAND EngineTests --selector=off
            WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
        )
    endif()
endif()

# =============================================================================
# 6. 宿主机制已取消
# =============================================================================
# 本层过去是「宿主产地」：把下层组件按由浅到深的范围接在一起，作为开发时的启动项。
# 该机制已移除，原因有二：
#     1) 宿主把下层源码拉进本构建树重复编译，与「层间一律链接已构建静态库」的契约冲突；
#     2) 分层宿主与 EngineTests 职责重叠，可执行文件只留一个入口更清楚。
# 现在的分工：下层各自构建静态库（Engine/EngineCore → EngineCore.lib、
# Engine/EngineSystem → EngineSystem.lib），本层构建 EngineTests.exe 作为唯一启动与测试入口。