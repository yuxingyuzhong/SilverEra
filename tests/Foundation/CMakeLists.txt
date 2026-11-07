# =============================================================================
# tests/Foundation —— EngineFoundation 层独立测试子工程
# -----------------------------------------------------------------------------
# 位置：Application/Test/tests/Foundation
#
# 定位
#     只测 EngineFoundation 一层的静态库 + 薄入口。改动 Foundation 的人只需构建
#     本子工程即可验证自己的修改，不必等 System / Renderer 的测试库编过；反之，
#     System / Renderer 的测试库也不会因为 Foundation 的未升锁修改而整体挂掉。
#
# 产物（均落在本子工程自己的构建树）
#     out/build/x64-<配置>/lib/TestFoundation.lib     用例 + 注册锚（静态库）
#     <Test 层根>/TestFoundation.exe                  薄入口，只跑本层用例
#
# 依赖
#     EngineFoundation（按 Engine/版本锁.cmake 解析：锁空跟随最新构建树，锁非空取
#     Engine/版本缓存/ 里的同版次副本）。版本锁只读，升级由工作站脚本成套完成。
#
# 用法
#     cmake -S . -B out/build/x64-Debug -G Ninja
#     cmake --build out/build/x64-Debug
#     ../TestFoundation.exe --gtest_filter=...
# =============================================================================
cmake_minimum_required(VERSION 3.24)

project(TestFoundation LANGUAGES C CXX)

# 子工程根 / Test 层根 / 项目根
get_filename_component(BYJY_SUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_TEST_ROOT "${BYJY_SUB_ROOT}/../.." ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${BYJY_TEST_ROOT}/../.." ABSOLUTE)

include("${BYJY_TEST_ROOT}/cmake/测试子工程.cmake")
byjy_ceshi_zhunbei()

# =============================================================================
# 1. 接入选定层（版本锁解析 + 导入库 + 全链一致性）
# =============================================================================
byjy_ceshi_yilai("TestFoundation" EngineFoundation "BYJY_ENGINE_FOUNDATION" engine_foundation)

# 本子工程源码引用的 Foundation 头必须都在快照里（下层新增头未重建即报错）
byjy_ceshi_tou_jiaoyan("TestFoundation" "${BYJY_SUB_ROOT}/src")
# 记录本子工程构建时使用的下层版本（供图形界面判断该层级的下层库是否已更新）
byjy_ceshi_jilu()

# 包含根次序：下层头快照 → 项目根 → Test 层根（后者供 src/主调/... 形式引用）
set(BYJY_APP_INC
    "${EngineFoundation_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
    "${BYJY_TEST_ROOT}"
)

# =============================================================================
# 2. googletest（每个子工程各编一份；测试静态库本身不内嵌 gtest 目标）
# =============================================================================
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory("${BYJY_TEST_ROOT}/external/googletest"
    "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

# =============================================================================
# 3. 用例静态库
# =============================================================================
file(GLOB_RECURSE BYJY_TEST_SOURCES CONFIGURE_DEPENDS
    "${BYJY_SUB_ROOT}/src/*.cpp"
)
file(GLOB_RECURSE BYJY_TEST_HEADERS CONFIGURE_DEPENDS
    "${BYJY_SUB_ROOT}/src/*.h"
    "${BYJY_SUB_ROOT}/src/*.hpp"
)

include("${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/生成源文件列表.cmake")
byjy_source_list_sync(
    "${BYJY_SUB_ROOT}/源文件列表_TestFoundation.cmake"
    "${BYJY_SUB_ROOT}"
    TestFoundation
    BYJY_TEST_SOURCES
    BYJY_TEST_HEADERS
    "Application/Test/tests/Foundation"
)

# 引擎日志屏蔽是测试运行器的基础设施（日志系统用例需临时关/开屏蔽），
# 只依赖 Foundation，故直接编入本库；主层不再重复编译它（避免同一符号两份）。
add_library(TestFoundation STATIC
    "${BYJY_TEST_ROOT}/src/主调/引擎日志屏蔽.cpp"
)
include("${BYJY_SUB_ROOT}/源文件列表_TestFoundation.cmake")

target_include_directories(TestFoundation PUBLIC ${BYJY_APP_INC})
target_link_libraries(TestFoundation PUBLIC EngineFoundation)
# gtest 仅提供头与链接依赖，不把 gtest 目标编进本库
target_link_libraries(TestFoundation PRIVATE GTest::gtest)

if(MSVC)
    target_compile_options(TestFoundation PRIVATE /MP /utf-8)
    target_compile_definitions(TestFoundation PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(TestFoundation PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(TestFoundation PRIVATE _GNU_SOURCE)
endif()

set_target_properties(TestFoundation PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# =============================================================================
# 4. 薄入口（只跑本层用例；与主层共用同一份测试库）
# =============================================================================
# 静态库里未被引用的 .obj 会被链接器丢弃，TEST 宏的静态注册随之消失；
# 故用 WHOLE_ARCHIVE 整体收编，再由薄入口调用注册锚做「漏链即链接失败」的保险。
add_executable(TestFoundationMain "${BYJY_SUB_ROOT}/entry/薄入口.cpp")
target_link_libraries(TestFoundationMain PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,TestFoundation>"
    GTest::gtest
)
set_target_properties(TestFoundationMain PROPERTIES
    OUTPUT_NAME "TestFoundation"
    RUNTIME_OUTPUT_DIRECTORY "${BYJY_TEST_ROOT}/$<0:>"
)
if(MSVC)
    target_compile_options(TestFoundationMain PRIVATE /MP /utf-8)
endif()
