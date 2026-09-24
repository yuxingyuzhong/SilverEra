# =============================================================================
# 测试层 —— 构建脚本
# =============================================================================
# 位置：白银纪元/测试层
# 依赖：引擎层（EngineCore）、系统层（SystemCore）—— 按请求的宿主范围按需拉取
# 产物：TestCore 静态库（src/Tests 下的功能测试器集合）
#       分层宿主可执行文件（主调文件/<宿主名>/，每层一个宿主，产物落在本层根目录）
# 用法：
#     - 独立配置：cmake -S <本目录> -B <构建目录>
#     - 顶层编排：在顶层 CMakeLists 按需追加 add_subdirectory(测试层)
#     - 作为开发启动项：在引擎层 / 系统层 / 游戏层打开 BYJY_JOIN_TEST_HOST，
#       各层单独用 VS 打开时即可把本层宿主并入其构建树
#     - 范围裁剪：发起层在 add_subdirectory 本层之前设置 BYJY_HOST_REQUEST，
#       声明本次构建需要哪些宿主；本层只创建被请求的宿主，并只拉取它们依赖的下层。
#       例：BYJY_HOST_REQUEST=EngineHost 时，本构建树里不会出现系统层源码。
# 说明：
#     - src/Tests 的功能测试器当前是「类实现」形态（无 main 入口），
#       故收编为静态库；main 入口由分层宿主承担（见第 7 节）。
#     - external/googletest 已接入构建（见第 6 节），用例统一放在 src/单元测试/ 下；
#       尚未写用例时不产生测试目标，也不影响宿主的生成。
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
# 1. 宿主请求清单（裁剪入口）
# =============================================================================
# 发起层（引擎层 / 系统层 / 游戏层）在 add_subdirectory 本层之前设置 BYJY_HOST_REQUEST，
# 声明本次构建树需要哪些宿主。本层据此裁剪两件事：
#     1) 只创建被请求的宿主可执行文件；
#     2) 只拉取被请求宿主真正依赖的下层。
# 于是「打开引擎层」的构建树里不会出现系统层源码，也不会被系统层的编译错误牵连。
# 未设置（或置空）时视为「全要」，即测试层独立配置时的默认行为。
# 手工指定示例（直接配置本层时有效）：-DBYJY_HOST_REQUEST=SystemHost。
# 注意：被其他层拉入时，请求清单由发起层写死，此处不会覆盖它 —— 改范围请改发起层的 set()。
if(NOT DEFINED BYJY_HOST_REQUEST OR BYJY_HOST_REQUEST STREQUAL "")
    set(BYJY_HOST_REQUEST "EngineHost" "SystemHost" "FullHost")
endif()

# 被请求的宿主中是否有需要系统层的：SystemHost 与 FullHost 都链接 SystemCore
if("SystemHost" IN_LIST BYJY_HOST_REQUEST OR "FullHost" IN_LIST BYJY_HOST_REQUEST)
    set(BYJY_NEED_SYSTEM_LAYER TRUE)
else()
    set(BYJY_NEED_SYSTEM_LAYER FALSE)
endif()

message(STATUS "测试层本次请求的宿主: ${BYJY_HOST_REQUEST}")

# =============================================================================
# 2. 依赖：引擎层
# =============================================================================
# 顶层统一编排时 EngineCore 已在同一构建树中，直接复用；
# 独立配置时（cmake -S 测试层）则把引擎层一并拉进本次构建。
# TestCore 与全部宿主都直接或间接需要引擎层，故本层无条件拉取。
if(NOT TARGET EngineCore)
    set(ENGINE_LAYER_DIR "${PROJECT_ROOT_DIR}/../引擎层")
    # 依赖层的 CMakeLists 必须存在，否则给出明确报错而不是含糊的链接失败
    if(NOT EXISTS "${ENGINE_LAYER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到引擎层工程：${ENGINE_LAYER_DIR}\n"
            "测试层依赖引擎层，请确认两层目录同级。"
        )
    endif()
    add_subdirectory("${ENGINE_LAYER_DIR}" "${CMAKE_BINARY_DIR}/引擎层")
endif()

# =============================================================================
# 3. 依赖：系统层（按请求裁剪）
# =============================================================================
# 系统宿主 / 全栈宿主需要把系统层组件接进来，故同样按需拉取。
# 系统层的 CMakeLists 内部会按需把引擎层一并拉入，此处无需重复处理。
# 只有被请求的宿主确实需要系统层时才拉取：只要 EngineHost 时，系统层源码不进入本构建树。
if(NOT BYJY_NEED_SYSTEM_LAYER)
    message(STATUS "本次未请求系统层宿主，跳过系统层拉取")
elseif(NOT TARGET SystemCore)
    set(SYSTEM_LAYER_DIR "${PROJECT_ROOT_DIR}/../系统层")
    # 依赖层的 CMakeLists 必须存在，否则给出明确报错而不是含糊的链接失败
    if(NOT EXISTS "${SYSTEM_LAYER_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "未找到系统层工程：${SYSTEM_LAYER_DIR}\n"
            "测试层宿主依赖系统层，请确认两层目录同级。"
        )
    endif()
    add_subdirectory("${SYSTEM_LAYER_DIR}" "${CMAKE_BINARY_DIR}/系统层")
endif()

# =============================================================================
# 4. 源文件收集
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

# 单元测试用例不并入 TestCore：它们由第 6 节的独立目标收编。
# 若并入静态库，未被引用的 .obj 会连 TEST 宏的注册一起被链接器丢弃，用例便静默不跑
list(FILTER TEST_SOURCES EXCLUDE REGEX "/src/单元测试/")

# src/ 下暂无源文件时不视为错误：分阶段开发中测试代码尚未落位属正常状态，
# 此时不产出 TestCore，宿主仍可只靠下层静态库独立生成
if(NOT TEST_SOURCES)
    message(STATUS "测试层 src/ 下暂无源文件，跳过 TestCore 静态库")
endif()

if(TEST_SOURCES)
    message(STATUS "测试层源文件: ${TEST_SOURCES}")

    # =========================================================================
    # 5. 创建测试层静态库
    # =========================================================================
    add_library(TestCore STATIC ${TEST_SOURCES})

    # 公共包含路径：本层根目录
    target_include_directories(TestCore PUBLIC "${PROJECT_ROOT_DIR}")

    # 链接引擎层：引擎层的公共包含路径、GLFW / OpenGL 等依赖由 EngineCore 传递过来
    target_link_libraries(TestCore PUBLIC EngineCore)

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
# 6. 单元测试接入（googletest）
# =============================================================================
# 用例统一放在 src/单元测试/ 下，按被测层分「工具」「核心」两棵子树。
# 说明：
#     - googletest 以源码形式内嵌在 external/googletest，并以 EXCLUDE_FROM_ALL 接入：
#       只在被测试目标依赖时编译，不改变宿主与静态库的既有构建范围。
#     - 用例用 OBJECT 库收集而非静态库：静态库中未被引用的 .obj 会被链接器丢弃，
#       TEST 宏的静态注册随之丢失，用例便静默不跑。
#     - 本节必须位于宿主之前：EngineHost 要在这里取得的用例目标上追加依赖。
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

    #用例目标：编一次，EngineTests 与附带测试的宿主各取一份 .obj
    add_library(EngineTestObjects OBJECT ${BYJY_TEST_SOURCES})

    #链接引擎层以取得其对外的包含路径，并从 gtest 取得测试框架头文件路径
    target_link_libraries(EngineTestObjects PUBLIC EngineCore GTest::gtest)

    #编译选项与下层保持一致
    if(MSVC)
        target_compile_options(EngineTestObjects PRIVATE /MP /utf-8)
        target_compile_definitions(EngineTestObjects PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(EngineTestObjects PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(EngineTestObjects PRIVATE _GNU_SOURCE)
    endif()

    #独立测试可执行文件：main 由 gtest_main 提供，产物与宿主同落本层根目录
    add_executable(EngineTests $<TARGET_OBJECTS:EngineTestObjects>)
    target_link_libraries(EngineTests PRIVATE EngineTestObjects EngineCore GTest::gtest_main)
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
    #（[FATAL] Unable to open file ...\cmake_test_discovery_*.json），构建会被判失败。
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
# 7. 分层宿主可执行文件
# =============================================================================
# 本层是「宿主产地」：宿主把下层组件按由浅到深的范围接在一起，作为开发时的启动项运行。
# 静态库不能充当 VS 启动项目，因此「改下层源码 → 生成 → 下层静态库重编 → 宿主重新链接
# → 运行」这条闭环由宿主承担入口角色。
#
# 清单格式：宿主名|源码目录|链接目标|附带单元测试
# 宿主名必须是纯 ASCII：CMake 不接受非 ASCII 目标名，会在 add_executable 阶段直接报错
#（源码目录仍用中文，与 主调文件/ 下的目录名一一对应）
#   - EngineHost：只起引擎层，兼作引擎层单元测试入口，范围最浅
#   - SystemHost：引擎层 + 系统层（属性槽分发器 / 实体管理器）
#   - FullHost：完整启动链路（配置加载 + 实体管理 + 属性槽分发）
# 第四列置 ON 的宿主会额外链入全部用例，从而兼作测试入口而不只是启动项
set(BYJY_HOST_LIST
    "EngineHost|${PROJECT_ROOT_DIR}/主调文件/引擎宿主|EngineCore|ON"
    "SystemHost|${PROJECT_ROOT_DIR}/主调文件/系统宿主|SystemCore|OFF"
    "FullHost|${PROJECT_ROOT_DIR}/主调文件/全栈宿主|SystemCore|OFF"
)

set(BYJY_HOST_TARGETS "")

foreach(BYJY_HOST_ENTRY IN LISTS BYJY_HOST_LIST)
    # 拆分清单项：宿主名 / 源码目录 / 链接目标
    string(REPLACE "|" ";" BYJY_HOST_FIELDS "${BYJY_HOST_ENTRY}")
    list(GET BYJY_HOST_FIELDS 0 BYJY_HOST_NAME)
    list(GET BYJY_HOST_FIELDS 1 BYJY_HOST_SRC_DIR)
    list(GET BYJY_HOST_FIELDS 2 BYJY_HOST_LINK_TARGET)
    list(GET BYJY_HOST_FIELDS 3 BYJY_HOST_WITH_TESTS)

    # 未被本次请求的宿主直接跳过：这是「按层裁剪」的落点，
    # 目标不产生，自然不会进入生成步骤，也不会牵连它依赖的下层
    if(NOT (BYJY_HOST_NAME IN_LIST BYJY_HOST_REQUEST))
        message(STATUS "宿主 [${BYJY_HOST_NAME}] 未被本次构建请求，跳过")
        continue()
    endif()

    # 收集该宿主目录下的全部源文件
    file(GLOB_RECURSE BYJY_HOST_SOURCES CONFIGURE_DEPENDS
        "${BYJY_HOST_SRC_DIR}/*.cpp"
        "${BYJY_HOST_SRC_DIR}/*.c"
    )

    # 源码尚未落位则跳过该宿主，不视为错误，构建不因此中断
    if(NOT BYJY_HOST_SOURCES)
        message(STATUS "宿主 [${BYJY_HOST_NAME}] 暂无源码，跳过（目录：${BYJY_HOST_SRC_DIR}）")
        continue()
    endif()

    # 链接目标必须已存在：本层前面已按需拉取系统层与引擎层
    if(NOT TARGET ${BYJY_HOST_LINK_TARGET})
        message(FATAL_ERROR
            "宿主 [${BYJY_HOST_NAME}] 需要链接 ${BYJY_HOST_LINK_TARGET}，但该目标不存在。\n"
            "请确认对应层的 CMakeLists 已加入本构建树。"
        )
    endif()

    # 创建宿主可执行文件并链接下层静态库
    add_executable(${BYJY_HOST_NAME} ${BYJY_HOST_SOURCES})
    target_link_libraries(${BYJY_HOST_NAME} PRIVATE ${BYJY_HOST_LINK_TARGET})

    # 附带单元测试的宿主：追加用例目标，宿主本身即成为测试入口。
    # 只链 GTest::gtest 而不链 gtest_main —— 宿主的 main 由自身源码提供，两者不可并存
    if(BYJY_HOST_WITH_TESTS AND TARGET EngineTestObjects)
        target_sources(${BYJY_HOST_NAME} PRIVATE $<TARGET_OBJECTS:EngineTestObjects>)
        target_link_libraries(${BYJY_HOST_NAME} PRIVATE GTest::gtest)
    endif()

    # 编译选项与下层保持一致
    if(MSVC)
        target_compile_options(${BYJY_HOST_NAME} PRIVATE /MP /utf-8)
        target_compile_definitions(${BYJY_HOST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${BYJY_HOST_NAME} PRIVATE -Wall -Wextra -pedantic)
        target_compile_definitions(${BYJY_HOST_NAME} PRIVATE _GNU_SOURCE)
    endif()

    # 产物输出到本层根目录，与 assets/ 同级
    #（Engine_Env 以「可执行文件所在目录」为基准解析资源路径，这是硬性要求）
    set_target_properties(${BYJY_HOST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT_DIR}"
    )

    list(APPEND BYJY_HOST_TARGETS ${BYJY_HOST_NAME})
    message(STATUS "测试层已添加宿主: ${BYJY_HOST_NAME}.exe（链接 ${BYJY_HOST_LINK_TARGET}）")
endforeach()

# VS 启动项：在已存在的宿主里取范围最深的一个。
# 若发起层自带启动项（例如游戏层的 TestEngine），则不覆盖它的选择。
get_property(BYJY_EXISTING_STARTUP DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT)
if(NOT BYJY_EXISTING_STARTUP)
    foreach(BYJY_PREFERRED_HOST IN ITEMS FullHost SystemHost EngineHost)
        if(TARGET ${BYJY_PREFERRED_HOST})
            set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "${BYJY_PREFERRED_HOST}")
            break()
        endif()
    endforeach()
endif()

if(NOT BYJY_HOST_TARGETS)
    message(STATUS "尚未生成任何宿主可执行文件：请在 主调文件/<宿主名>/ 下添加入口源码。")
endif()
