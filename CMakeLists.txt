# =============================================================================
# 白银纪元 —— 顶层构建编排
# =============================================================================
# 层次结构（下层被上层链接，方向单向）：
#     引擎层（最底）  ←  系统层  ←  游戏层
# 三层各自都保留独立的 CMakeLists：
#     - 被本文件 add_subdirectory 时，按同一构建树统一编排
#     - 也可单独执行 cmake -S <层目录> -B <构建目录> 独立配置编译
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

project(白银纪元 LANGUAGES C CXX)

# ---- 统一 C++ 标准 ----
# 各层 CMakeLists 内亦各自设置一次，以保证单层独立配置时同样生效
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # 编译器不支持 C++20 则报错
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁用编译器扩展，提高跨平台兼容性

# ---- 按依赖顺序加入各层 ----
add_subdirectory(引擎层)
add_subdirectory(系统层)
add_subdirectory(游戏层)
