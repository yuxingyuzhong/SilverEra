# =============================================================================
# 测试层 —— 构建脚本
# =============================================================================
# 位置：白银纪元/测试层
# 依赖：引擎层（EngineCore）、系统层（SystemCore）—— 一律链接「已经构建好的」静态库
# 产物：TestCore 静态库（src/ 下的功能测试器集合，当前为空）
#       EngineTests.exe（本层唯一可执行文件，main 由 googletest 提供）
# 用法（本层独立配置与构建）：
#     cmake -S <本目录> -B <本目录>/out/build/x64-Debug -G Ninja
#     cmake --build <本目录>/out/build/x64-Debug
# 前置条件：先构建引擎层与系统层。本层不拉入任何下层源码，
#           也没有源码回退路径（层级契约统一：层与层之间一律链接静态库）。
# 层间契约：
#     cmake/接入下层.cmake   通用接入函数 byjy_jieru_xiaceng()，系统层 / 本层 / 游戏层逐字节相同
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

project(测试层 LANGUAGES C CXX)

# 强制使用 C++20 标准（与引擎层保持一致）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 本层根目录
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# =============================================================================
# 1. 接入下层：引擎层 / 系统层 —— 链接已构建好的静态库
# =============================================================================
# 本层只消费下层「已经构建好的」静态库：下层源码不进入本次构建，
# 也没有任何源码回退路径（层级契约统一）。
#
# 库定位顺序由 cmake/接入下层.cmake 的 byjy_jieru_xiaceng() 统一实现：
#   ① 覆盖变量非空 → 用它；指向的文件不存在则直接报错
#   ② 留空 → 自动探测 <下层>/out/build/*/lib/<库名>.lib，取时间戳最新的一份
#   ③ 都没有 → FATAL_ERROR，并提示先构建下层
#
# 下层的对外面（包含目录 / 编译定义 / 链接库 / 系统库）由各层 cmake/对外接口.cmake
# 声明。系统层的对外面已并入引擎层的对外面，引擎层的包含目录（common/ 预编译头、
# src/ 下的头文件、external/Json 与 external/glfw 等）因此一并到位。
#
# 两层都接：本层是各层的测试产地，将来出现系统层用例时不必再改这里。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")

set(BYJY_ENGINE_LIB_PATH "" CACHE FILEPATH
    "引擎层静态库路径；留空则自动探测 引擎层/out/build/*/lib/EngineCore.lib")
set(BYJY_SYSTEM_LIB_PATH "" CACHE FILEPATH
    "系统层静态库路径；留空则自动探测 系统层/out/build/*/lib/SystemCore.lib")

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../引擎层/cmake/对外接口.cmake"
    "BYJY_ENGINE"
    "BYJY_ENGINE_LIB_PATH"
)

byjy_jieru_xiaceng(
    "${PROJECT_ROOT_DIR}/../系统层/cmake/对外接口.cmake"
    "BYJY_SYSTEM"
    "BYJY_SYSTEM_LIB_PATH"
)

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 收录 src/ 下的功能测试代码
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/src/*.c"
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
    add_library(TestCore STATIC ${TEST_SOURCES})

    # 公共包含路径：本层根目录
    target_include_directories(TestCore PUBLIC "${PROJECT_ROOT_DIR}")

    # 链接下层静态库：包含路径 / 编译定义 / 第三方库由导入目标传递过来
    target_link_libraries(TestCore PUBLIC EngineCore SystemCore)

    # 编译选项与引擎层保持一致
    if(MSVC)
        target_compile_options(TestCore PRIVATE /MP /utf-8)
        target_compile_definitions(TestCore PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(TestCore PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(TestCore PRIVATE _GNU_SOURCE)
    endif()

    # 静态库输出到构建目录 lib/（与引擎层一致）
    set_target_properties(TestCore PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
endif()

# =============================================================================
# 4. 单元测试接入（googletest）
# =============================================================================
# 用例统一放在 src/单元测试/ 下，按被测层分「工具」「核心」两棵子树。
# 说明：googletest 以源码形式内嵌在 external/googletest，并以 EXCLUDE_FROM_ALL 接入：
#       只在被测试目标依赖时编译，不改变静态库的既有构建范围。
if(EXISTS "${PROJECT_ROOT_DIR}/external/googletest/CMakeLists.txt")
    #不安装 gtest、不构建 gmock；强制共享运行时库，避免与引擎层的 CRT 设置冲突（LNK2038）
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

# 用例尚未落位时不视为错误，构建不因此中断
if(NOT BYJY_TEST_SOURCES)
    message(STATUS "src/单元测试/ 下暂无用例，跳过 EngineTests 与 EngineTestObjects")
elseif(NOT TARGET GTest::gtest)
    message(STATUS "gtest 目标不可用，跳过 EngineTests 与 EngineTestObjects")
else()
    message(STATUS "测试层用例文件: ${BYJY_TEST_SOURCES}")

    #用例目标：编一次，EngineTests 取一份 .obj
    add_library(EngineTestObjects OBJECT ${BYJY_TEST_SOURCES})

    #链接下层静态库以取得对外的包含路径，并从 gtest 取得测试框架头文件路径
    target_link_libraries(EngineTestObjects PUBLIC EngineCore SystemCore GTest::gtest)

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
    # main 由 gtest_main 提供；产物落在本层根目录，与 assets/ 同级
    #（Engine_Env 以「可执行文件所在目录」为基准解析资源路径，这是硬性要求）
    add_executable(EngineTests $<TARGET_OBJECTS:EngineTestObjects>)
    target_link_libraries(EngineTests PRIVATE EngineTestObjects EngineCore SystemCore GTest::gtest_main)
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
        add_test(NAME EngineTests COMMAND EngineTests
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
# 现在的分工：下层各自构建静态库（引擎层 EngineCore、系统层 SystemCore），
# 本层构建 EngineTests.exe 作为唯一启动与测试入口。
