# =============================================================================
# tests/Renderer —— EngineRenderer 层独立测试子工程
# -----------------------------------------------------------------------------
# 位置：Application/Test/tests/Renderer
#
# 定位
#     只测渲染层（含其依赖的 EngineSystem / EngineFoundation 面）的静态库 + 薄入口。
#     依赖顺序：Foundation → System → Renderer，三层都按版本锁解析。
#
# 产物
#     out/build/x64-<配置>/lib/TestRenderer.lib     用例 + 注册锚（静态库）
#     <Test 层根>/TestRenderer.exe                  薄入口，只跑本层用例
#
# 资源
#     渲染用例按「可执行文件目录」找 assets/ 与 shaders/，故薄入口也落在 Test 层根；
#     着色器与所链 EngineRenderer.lib 同版次，配置期从渲染层构建树拷入。
#
# 用法
#     cmake -S . -B out/build/x64-Debug -G Ninja
#     cmake --build out/build/x64-Debug
#     ../TestRenderer.exe --gtest_filter=...
# =============================================================================
cmake_minimum_required(VERSION 3.24)

project(TestRenderer LANGUAGES C CXX)

get_filename_component(BYJY_SUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_TEST_ROOT "${BYJY_SUB_ROOT}/../.." ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${BYJY_TEST_ROOT}/../.." ABSOLUTE)

include("${BYJY_TEST_ROOT}/cmake/测试子工程.cmake")
byjy_ceshi_zhunbei()

# =============================================================================
# 1. 接入依赖层（Foundation → System → Renderer：成套校验逐层累积）
# =============================================================================
byjy_ceshi_yilai("TestRenderer" EngineFoundation "BYJY_ENGINE_FOUNDATION" engine_foundation)
byjy_ceshi_yilai("TestRenderer" EngineSystem "BYJY_ENGINE_SYSTEM" engine_system)
byjy_ceshi_yilai("TestRenderer" EngineRenderer "BYJY_RENDERER" renderer)

byjy_ceshi_tou_jiaoyan("TestRenderer" "${BYJY_SUB_ROOT}/src")

# 包含根次序：下层头快照 → 项目根 → Test 层根
set(BYJY_APP_INC
    "${EngineFoundation_SNAPSHOT}"
    "${EngineSystem_SNAPSHOT}"
    "${EngineRenderer_SNAPSHOT}"
    "${BYJY_PROJECT_ROOT}"
    "${BYJY_TEST_ROOT}"
)

# =============================================================================
# 2. 渲染层着色器拷贝（与配置对应的 EngineRenderer.lib 同源）
# =============================================================================
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
   OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"
   OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    set(_byjy_renderer_lib "${EngineRenderer_LIB_RELEASE}")
else()
    set(_byjy_renderer_lib "${EngineRenderer_LIB_DEBUG}")
endif()
get_filename_component(_shader_root_a "${_byjy_renderer_lib}/../.." ABSOLUTE)
get_filename_component(_shader_root_b "${_byjy_renderer_lib}/../../.." ABSOLUTE)
if(IS_DIRECTORY "${_shader_root_b}/shaders")
    set(BYJY_RENDERER_SHADERS "${_shader_root_b}/shaders")
else()
    set(BYJY_RENDERER_SHADERS "${_shader_root_a}/shaders")
endif()
if(IS_DIRECTORY "${BYJY_RENDERER_SHADERS}")
    file(COPY "${BYJY_RENDERER_SHADERS}" DESTINATION "${BYJY_TEST_ROOT}")
    message(STATUS "渲染层着色器已拷贝：${BYJY_RENDERER_SHADERS} → ${BYJY_TEST_ROOT}/shaders")
else()
    message(WARNING "未找到渲染层着色器目录：${BYJY_RENDERER_SHADERS}（渲染用例需要它，请先构建渲染层）")
endif()

# =============================================================================
# 3. googletest
# =============================================================================
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory("${BYJY_TEST_ROOT}/external/googletest"
    "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

# =============================================================================
# 4. 用例静态库
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
    "${BYJY_SUB_ROOT}/源文件列表_TestRenderer.cmake"
    "${BYJY_SUB_ROOT}"
    TestRenderer
    BYJY_TEST_SOURCES
    BYJY_TEST_HEADERS
    "Application/Test/tests/Renderer"
)

add_library(TestRenderer STATIC)
include("${BYJY_SUB_ROOT}/源文件列表_TestRenderer.cmake")

target_include_directories(TestRenderer PUBLIC ${BYJY_APP_INC})
target_link_libraries(TestRenderer PUBLIC EngineRenderer EngineSystem EngineFoundation)
target_link_libraries(TestRenderer PRIVATE GTest::gtest)

if(MSVC)
    target_compile_options(TestRenderer PRIVATE /MP /utf-8)
    target_compile_definitions(TestRenderer PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(TestRenderer PRIVATE -Wall -Wextra -pedantic)
    target_compile_definitions(TestRenderer PRIVATE _GNU_SOURCE)
endif()

set_target_properties(TestRenderer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# =============================================================================
# 5. 薄入口
# =============================================================================
add_executable(TestRendererMain "${BYJY_SUB_ROOT}/entry/薄入口.cpp")
target_link_libraries(TestRendererMain PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,TestRenderer>"
    GTest::gtest
)
set_target_properties(TestRendererMain PROPERTIES
    OUTPUT_NAME "TestRenderer"
    RUNTIME_OUTPUT_DIRECTORY "${BYJY_TEST_ROOT}/$<0:>"
)
if(MSVC)
    target_compile_options(TestRendererMain PRIVATE /MP /utf-8)
endif()
