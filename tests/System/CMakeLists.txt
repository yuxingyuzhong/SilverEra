# =============================================================================
# tests/System —— EngineSystem 层独立测试子工程
# -----------------------------------------------------------------------------
# 位置：Application/Test/tests/System
#
# 定位
#     只测 EngineSystem 一层（含其依赖的 EngineFoundation 面）的静态库 + 薄入口。
#     依赖顺序：EngineFoundation → EngineSystem，两层都按版本锁解析。
#
# 产物
#     out/build/x64-<配置>/lib/TestSystem.lib     用例 + 注册锚（静态库）
#     <Test 层根>/TestSystem.exe                  薄入口，只跑本层用例
#
# 用法
#     cmake -S . -B out/build/x64-Debug -G Ninja
#     cmake --build out/build/x64-Debug
#     ../TestSystem.exe --gtest_filter=...
# =============================================================================
cmake_minimum_required(VERSION 3.24)

project(TestSystem LANGUAGES C CXX)

get_filename_component(BYJY_SUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_TEST_ROOT "${BYJY_SUB_ROOT}/../.." ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${BYJY_TEST_ROOT}/../.." ABSOLUTE)

include("${BYJY_TEST_ROOT}/cmake/测试子工程.cmake")
byjy_ceshi_zhunbei()

# =============================================================================
# 1. 接入依赖层（先 Foundation 后 System：后者的一致性校验要读前者版本）
# =============================================================================
byjy_ceshi_yilai("TestSystem" EngineFoundation "BYJY_ENGINE_FOUNDATION" engine_foundation)
byjy_ceshi_yilai("TestSystem" EngineSystem "BYJY_ENGINE_SYSTEM" engine_system)

byjy_ceshi_tou_jiaoyan("TestSystem" "${BYJY_SUB_ROOT}/src")
# 记录本子工程构建时使用的下层版本（供图形界面判断该层级的下层库是否已更新）
byjy_ceshi_jilu()

# 包含根次序：下层头快照 → 项目根 → Test 层根
set(BYJY_APP_INC
    "${EngineFoundation_SNAPSHOT}"
    "${EngineSystem_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
    "${BYJY_TEST_ROOT}"
)

# =============================================================================
# 2. googletest
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
    "${BYJY_SUB_ROOT}/源文件列表_TestSystem.cmake"
    "${BYJY_SUB_ROOT}"
    TestSystem
    BYJY_TEST_SOURCES
    BYJY_TEST_HEADERS
    "Application/Test/tests/System"
)

add_library(TestSystem STATIC)
include("${BYJY_SUB_ROOT}/源文件列表_TestSystem.cmake")

target_include_directories(TestSystem PUBLIC ${BYJY_APP_INC})
target_link_libraries(TestSystem PUBLIC EngineSystem EngineFoundation)
target_link_libraries(TestSystem PRIVATE GTest::gtest)

if(MSVC)
    target_compile_options(TestSystem PRIVATE /MP /utf-8)
    target_compile_definitions(TestSystem PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(TestSystem PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(TestSystem PRIVATE _GNU_SOURCE)
endif()

set_target_properties(TestSystem PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# =============================================================================
# 4. 薄入口
# =============================================================================
add_executable(TestSystemMain "${BYJY_SUB_ROOT}/entry/薄入口.cpp")
target_link_libraries(TestSystemMain PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,TestSystem>"
    GTest::gtest
)
set_target_properties(TestSystemMain PROPERTIES
    OUTPUT_NAME "TestSystem"
    RUNTIME_OUTPUT_DIRECTORY "${BYJY_TEST_ROOT}/$<0:>"
)
if(MSVC)
    target_compile_options(TestSystemMain PRIVATE /MP /utf-8)
endif()
