# =============================================================================
# EngineFoundation —— 构建脚本
# =============================================================================
# 位置：白银纪元/Engine/EngineFoundation
# 依赖：无（拓扑最底层）
# 产物：EngineFoundation.lib（静态库，内含版本锚点符号）
# 用法（推荐用 CMakePresets.json 的预设；Debug / Release 双构建树并存，互不覆盖）：
#     Debug  ：cmake --preset x64-Debug   && cmake --build --preset build-Debug
#     Release：cmake --preset x64-Release && cmake --build --preset build-Release
#     预设等价于下列手工命令（需要先进入 VS 开发人员环境）：
#     Debug  ：cmake -S <本目录> -B <本目录>/out/build/x64-Debug   -G Ninja -DCMAKE_BUILD_TYPE=Debug
#     Release：cmake -S <本目录> -B <本目录>/out/build/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
#     两棵树的产物各自落在自己的构建树内：
#       <构建树>/lib/EngineFoundation.lib                    静态库
#       <构建树>/include/Engine/EngineFoundation/...          对外头快照（含 版本锚点.h）
#       <构建树>/生成/版本锚点.{h,cpp}                   版次锚点生成物
#     省略 -DCMAKE_BUILD_TYPE 时按 Debug 处理并打印提示（见第 1.5 节）。
# 包含路径：头文件跨目录包含写「项目根相对全路径」Engine/EngineFoundation/...；
#           实现文件（.cpp）跨目录包含写「本层根相对」src/...
#           编译包含根 = 项目根 + 本层根（本层不消费任何下层头，无需头快照）
# 层间契约：
#     cmake/对外接口.cmake     本层对外自描述（前缀 BYJY_ENGINE_FOUNDATION）
#     cmake/导出头快照.cmake   对外头快照导出脚本（上层按与本层库同版次的头编译）
#     cmake/生成版本锚点.cmake 版次锚点生成脚本（头 / 库错配在链接期报错）
#     源文件列表.cmake         源文件显式清单（生成物，由 cmake/生成源文件列表.cmake
#                              自动维护；用途见第 2 节「源文件收集」）
# 说明：本层不含 cmake/接入下层.cmake —— 无下层可接；
#       本层不对外传递任何第三方库或系统库（窗口 / 图形系统库由渲染层提供）。
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(MSVC)
    add_compile_options(/WX-)
endif()

# =============================================================================
# 1. 项目基本信息
# =============================================================================
project(EngineFoundation LANGUAGES C CXX)

# 导出编译数据库（供 IDE / 语言工具链读取本层的包含路径与宏；Ninja 生成器默认不产出该文件）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# 1.5 构建类型（Debug / Release 双构建树）
# =============================================================================
# 背景：本层用 Ninja（单配置生成器），一次配置只产一种构建类型；Debug 与 Release
#       靠两棵独立构建树并存：
#           out/build/x64-Debug   → Debug   （默认）
#           out/build/x64-Release → Release （需显式指定 -DCMAKE_BUILD_TYPE=Release）
# 做法：单配置生成器下校验 CMAKE_BUILD_TYPE —— 缺省取 Debug 并打印提示；
#       取值非法直接失败。否则会落入「既非 Debug 也非 Release」的裸配置
#       （MSVC 在构建类型为空时按 /MD 编译，但不开优化、也不定义 NDEBUG）。
# 说明：多配置生成器（Visual Studio）由 CMAKE_CONFIGURATION_TYPES 管理，
#       此处不介入，避免与 IDE 的配置下拉互相覆盖。
# 入口：推荐 CMakePresets.json 的 x64-Debug / x64-Release，其 binaryDir 与上述两棵树一致；
#       预设只是把命令固定下来，不改变构建行为。
# =============================================================================
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型（Debug / Release / RelWithDebInfo / MinSizeRel）" FORCE)
        message(STATUS "未指定 CMAKE_BUILD_TYPE，按 Debug 处理（构建树：${CMAKE_BINARY_DIR}）")
    endif()

    set(BYJY_VALID_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
    if(NOT CMAKE_BUILD_TYPE IN_LIST BYJY_VALID_BUILD_TYPES)
        message(FATAL_ERROR
            "CMAKE_BUILD_TYPE 取值非法：'${CMAKE_BUILD_TYPE}'\n"
            "合法取值（区分大小写）：${BYJY_VALID_BUILD_TYPES}\n"
            "示例：cmake -S <本目录> -B <本目录>/out/build/x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release")
    endif()

    message(STATUS "构建类型：${CMAKE_BUILD_TYPE}（构建树：${CMAKE_BINARY_DIR}）")
endif()

# =============================================================================
# 归档响应文件 BOM 包装器（MSVC + Ninja 专用）
# =============================================================================
# 背景：Ninja 写出的归档响应文件（*.rsp）是 UTF-8 无 BOM，而 MSVC 的 lib.exe
#       在没有 BOM 时按系统 ANSI 代码页解读文件内容，中文对象名会被解成乱码，
#       归档阶段报 LNK1181。
# 做法：把归档命令换成「先给响应文件补 BOM，再调用真实归档器」的包装脚本，
#       使源文件名保持中文（本层与上层都不必把中文源名改成英文）。
# 说明：CMAKE_CXX_CREATE_STATIC_LIBRARY 必须在 project() 之后设置，否则会被
#       Platform/Windows-MSVC.cmake 的默认值覆盖回去；只对 Ninja 生成器生效，
#       避免影响 Visual Studio 生成器（其归档由 MSBuild 的 Lib 任务完成）。
# =============================================================================
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    # 此处尚未执行下方的 get_filename_component，故用 CMAKE_CURRENT_SOURCE_DIR 定位
    set(BYJY_AR_WRAPPER "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ar_rsp_bom.ps1")
    find_program(BYJY_POWERSHELL_EXECUTABLE NAMES powershell powershell.exe)

    # 包装脚本与 PowerShell 都就位时才改写归档命令
    if(EXISTS "${BYJY_AR_WRAPPER}" AND BYJY_POWERSHELL_EXECUTABLE)
        set(CMAKE_CXX_CREATE_STATIC_LIBRARY
            "${BYJY_POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass -File ${BYJY_AR_WRAPPER} <CMAKE_AR> /nologo <LINK_FLAGS> /out:<TARGET> <OBJECTS> ")
        message(STATUS "归档响应文件 BOM 包装器已启用：${BYJY_AR_WRAPPER}")
    else()
        message(WARNING "未找到归档包装器或 PowerShell，中文对象名的归档可能报 LNK1181：${BYJY_AR_WRAPPER}")
    endif()
endif()

# 强制使用 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # 如果编译器不支持 C++20 则报错
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁用 GCC/MSVC 的编译器扩展，提高跨平台兼容性

# =============================================================================
# 2. 源文件收集
# =============================================================================
# 本层根目录；项目根 = Engine/EngineFoundation 上溯两级（项目内部包含路径的基准）
get_filename_component(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BYJY_PROJECT_ROOT "${PROJECT_ROOT_DIR}/../.." ABSOLUTE)

# 递归搜索引擎源码目录（src / common / external）下的 .cpp 文件
# 注：本层源文件统一用 .cpp；external/ 下即便存在 .c 也不收（Lua 等 C 源码不在本层编译）
# CONFIGURE_DEPENDS 让 CMake 在构建时自动检测新增/删除的源文件（不必手动重新运行 cmake）
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.cpp"
    "${PROJECT_ROOT_DIR}/common/*.cpp"
    "${PROJECT_ROOT_DIR}/external/*.cpp"
)

# 递归收集本层头文件：仅用于让 IDE 与生成系统建立文件索引（不参与编译），
# external/ 下第三方头文件不纳入以免污染索引
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src/*.h"
    "${PROJECT_ROOT_DIR}/src/*.hpp"
    "${PROJECT_ROOT_DIR}/common/*.h"
    "${PROJECT_ROOT_DIR}/common/*.hpp"
)

# 把被 GLOB 的目录登记为配置依赖：目录内容变化（增删文件）时，生成系统自动重跑
# 配置并重扫 GLOB，不必手动「重新生成缓存」才刷新目录视图。
# 说明：GLOB_RECURSE 覆盖子目录，但 CMAKE_CONFIGURE_DEPENDS 要逐个目录登记。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_ROOT_DIR}/src"
    "${PROJECT_ROOT_DIR}/common"
    "${PROJECT_ROOT_DIR}/external"
)

# ----- 过滤掉不需要的文件 -----
if(ENGINE_SOURCES)
    # 1) 排除构建目录下的文件（防止将生成的临时文件误编译进去）
    #    将反斜杠转为正斜杠，方便正则匹配
    string(REPLACE "\\" "/" SAFE_BINARY_DIR "${CMAKE_BINARY_DIR}")
    # 转义正则特殊字符（如 . + * ? 等）
    string(REGEX REPLACE "([.+*?^$()])" "\\\\\\1" SAFE_BINARY_REGEX "${SAFE_BINARY_DIR}")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")
    # 头文件同样排除构建目录下的生成物
    list(FILTER ENGINE_HEADERS EXCLUDE REGEX "^${SAFE_BINARY_REGEX}/")

endif()

# 如果过滤后没有源文件，终止配置并给出明确错误提示
if(NOT ENGINE_SOURCES)
    message(FATAL_ERROR 
        "未在 '${PROJECT_ROOT_DIR}' 的 src/、common/、external/ 下找到任何 .cpp 文件！\n"
        "请检查目录结构是否完整，以及 CMakeLists.txt 是否放置正确。"
    )
endif()

# 把扫描结果同步进显式清单 源文件列表.cmake（仅在路径集合有差异时写回），
# 清单随后以 target_sources 提供全部源与头。
# 动机：GLOB 工程里构建脚本不含任何「具体文件名」，VS 的文件视图改名 / 迁移操作
#       找不到可改写的引用，于是既不触发 CMake 重新配置，VS 的模型与错误列表也
#       停在旧版次。清单把扫描结果显式化，为该操作提供可改写的引用。
# 说明：清单被 include，因此它本身也是配置依赖 —— 被改写即触发重新配置。
include("${PROJECT_ROOT_DIR}/cmake/生成源文件列表.cmake")
byjy_source_list_sync(
    "${PROJECT_ROOT_DIR}/源文件列表.cmake"
    "${PROJECT_ROOT_DIR}"
    EngineFoundation
    ENGINE_SOURCES
    ENGINE_HEADERS
    "Engine/EngineFoundation"
)

# =============================================================================
# 3. 创建引擎静态库（核心）
# =============================================================================
# 此处不传任何源文件：源与头全部由 源文件列表.cmake 的 target_sources 提供。
# 原因见清单头部说明 —— 只有由 target_sources 直接加入的字面文件名，其 File API
# backtrace 才落在清单文件上，VS 的改名 / 迁移操作才改写得到引用。
add_library(EngineFoundation STATIC)
include("${PROJECT_ROOT_DIR}/源文件列表.cmake")

# =============================================================================
# 3.2 版次锚点（头 / 库同版次由链接期符号强制）
# =============================================================================
# 动机：头快照与静态库此前只靠「在同一棵构建树里」这一弱约定绑定，
#       手工拷贝库、跨树取库、跨配置回退都不会被发现。
# 做法：把「对外头集合 + 构建类型 + 运行库口径」的哈希编进符号名，
#       生成 版本锚点.h/.cpp；cpp 编入本库，h 随头快照发布；上层由导入目标
#       强制包含该头（MSVC /FI、GCC -include），任何 TU 都会引用锚点符号。
# 结果：头与库不同版次 → 链接期 LNK2019，不需要运行期比较。
# 说明：哈希目录与本层头快照的镜像范围严格一致（见第 5.5 节 MIRROR_DIRS）。
# =============================================================================
set(BYJY_ENGINE_FOUNDATION_ANCHOR_REL "Engine/EngineFoundation/版本锚点.h")
set(BYJY_ENGINE_FOUNDATION_ANCHOR_DIR "${CMAKE_BINARY_DIR}/生成")
set(BYJY_ENGINE_FOUNDATION_ANCHOR_H "${BYJY_ENGINE_FOUNDATION_ANCHOR_DIR}/版本锚点.h")
set(BYJY_ENGINE_FOUNDATION_ANCHOR_CPP "${BYJY_ENGINE_FOUNDATION_ANCHOR_DIR}/版本锚点.cpp")

add_custom_command(
    OUTPUT "${BYJY_ENGINE_FOUNDATION_ANCHOR_H}" "${BYJY_ENGINE_FOUNDATION_ANCHOR_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${BYJY_PROJECT_ROOT}"
        "-DMIRROR_DIRS=Engine/EngineFoundation/common;Engine/EngineFoundation/src"
        "-DBUILD_TYPE=$<CONFIG>"
        "-DOUT_H=${BYJY_ENGINE_FOUNDATION_ANCHOR_H}"
        "-DOUT_CPP=${BYJY_ENGINE_FOUNDATION_ANCHOR_CPP}"
        "-DLAYER_TOKEN=engine_foundation"
        -P "${PROJECT_ROOT_DIR}/cmake/生成版本锚点.cmake"
    DEPENDS ${ENGINE_HEADERS} "${PROJECT_ROOT_DIR}/cmake/生成版本锚点.cmake"
    COMMENT "生成 EngineFoundation 版次锚点"
    VERBATIM
)
set_source_files_properties("${BYJY_ENGINE_FOUNDATION_ANCHOR_CPP}" PROPERTIES GENERATED TRUE)
target_sources(EngineFoundation PRIVATE "${BYJY_ENGINE_FOUNDATION_ANCHOR_CPP}")

# =============================================================================
# 4. 头文件搜索路径设置
# =============================================================================
# 私有包含根 = 项目根 + 本层根，两种写法各司其职：
#   ① 项目根（BYJY_PROJECT_ROOT）：头文件里跨目录包含一律写 Engine/EngineFoundation/... 全路径。
#      对外头快照正是把本层头镜像到 <快照根>/Engine/EngineFoundation/...，上层以「快照根」替换
#      「项目根」来解析，故头内的包含必须以项目根为基准，否则上层消费快照时会解析错位置。
#   ② 本层根（PROJECT_ROOT_DIR）：层内实现文件（.cpp）跨目录包含写 src/... 层根相对。
#      .cpp 不进对外头快照、不被别层消费，无需背复项目根前缀；而层根相对又不像 ../ 那样
#      「相对当前文件所在目录」、层级一错就断。头与 .cpp 分属两种基准，互不干扰。
target_include_directories(EngineFoundation PRIVATE "${BYJY_PROJECT_ROOT}" "${PROJECT_ROOT_DIR}")

# =============================================================================
# 5. 第三方库
# =============================================================================
# 本层采用「自用面 / 对外面」分开写的模型：
#   自用面 = 下面的 PRIVATE 列表（本层源码实际要什么）
#   对外面 = cmake/对外接口.cmake 的 BYJY_ENGINE_FOUNDATION_OUT_INC（上层要什么）
# 两者当前内容相同（项目根 + external/Json），但角色不同：对外面要随头快照根一起被
# 上层重定向解析，自用面不需要。因此新增第三方库时**两侧都要写**（见 README 10.2 第 4 条）。
# 对照：EngineSystem 把对外面直接挂成自己的 PUBLIC 包含根（单点声明），因为它没有
#       「只给上层」的条目；两种写法结果等价，本层保留双写是为了让自用面保持最小。

# ---------- Json ---------- json解析库
set(JSON_ROOT "${PROJECT_ROOT_DIR}/external/Json")
target_include_directories(EngineFoundation PRIVATE "${JSON_ROOT}")

# =============================================================================
# 5.5 对外头快照导出（供上层按「与本层静态库同版次」的头编译，并携带版次锚点）
# =============================================================================
# 背景：上层的包含目录此前直接指向本层源码树，与本层构建目录里的 .lib 无绑定关系，
#       一旦「头已改、库未重建」，上层就会新头配旧库地编译链接。
# 做法：每轮构建把对外包含目录下的头镜像到 <构建目录>/include/，上层改读该快照；
#       只复制内容有变化的头，未变头不改变时间戳，不会引起上层无谓重编。
# 路径形态：镜像目录相对「项目根」保留完整层级 —— 快照根/Engine/EngineFoundation/...
#           上层把对外面里的「项目根」条目换成该快照根即可无缝解析同一个前缀。
# 镜像范围：只镜像本层自有头（common、src）。第三方（external/Json）在外对面上是
#           独立的绝对路径条目，不经过快照解析，也就不必镜像 —— 少复制上千个第三方头，
#           构建更快。
# 锚点：快照同时携带构建期生成的 Engine/EngineFoundation/版本锚点.h；该目标依赖 EngineFoundation，
#       库构建失败时不会导出新快照，从而不出现「新头 + 旧库」的中间状态。
# =============================================================================
set(BYJY_ENGINE_FOUNDATION_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include")
# 多配置生成器（VS）：一棵树装两个配置，快照必须按配置分目录，否则两配置互相覆盖
if(CMAKE_CONFIGURATION_TYPES)
    set(BYJY_ENGINE_FOUNDATION_INCLUDE_SNAPSHOT_DIR "${CMAKE_BINARY_DIR}/include/$<CONFIG>")
endif()

add_custom_target(EngineFoundationIncludeSnapshot ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_ROOT=${BYJY_PROJECT_ROOT}"
        "-DDST_ROOT=${BYJY_ENGINE_FOUNDATION_INCLUDE_SNAPSHOT_DIR}"
        "-DMIRROR_DIRS=Engine/EngineFoundation/common;Engine/EngineFoundation/src"
        "-DANCHOR_SRC=${BYJY_ENGINE_FOUNDATION_ANCHOR_H}"
        "-DANCHOR_REL=${BYJY_ENGINE_FOUNDATION_ANCHOR_REL}"
        -P "${PROJECT_ROOT_DIR}/cmake/导出头快照.cmake"
    COMMENT "导出 EngineFoundation 对外头快照 → ${BYJY_ENGINE_FOUNDATION_INCLUDE_SNAPSHOT_DIR}"
    VERBATIM
)

# 快照必须在本层库构建成功之后导出：库失败时保留旧快照（头与库仍一致），
# 不允许出现「新头 + 旧库」的中间状态。
add_dependencies(EngineFoundationIncludeSnapshot EngineFoundation)

# =============================================================================
# 6. 版本缓存发布（版本锁机制的供方）
# =============================================================================
# 快照与库构建成功后，把它们按「版本哈希」存进 Engine/版本缓存/<层>/x64-<配置>/<哈希>/。
# 上层在 Engine/版本锁.cmake 里填该哈希，就会从这里取头与库，从而停在这个版本。
# 说明：
#   - 版本哈希即版次锚点符号里的 12 位哈希（每配置不同）；
#   - 头按内容不同才覆盖、库按内容不同才覆盖，重复发布不产生额外变更；
#   - 保留最近 KEEP 个版本，被版本锁引用的版本永不清理；
#   - 依赖版本.cmake 一并拷入（本层无下层，内容为空，供上层统一读取）。
# =============================================================================
include("${PROJECT_ROOT_DIR}/cmake/版本工具.cmake")
byjy_banben_jilu("${CMAKE_BINARY_DIR}")

add_custom_target(EngineFoundationVersionPublish ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSRC_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DSNAPSHOT_ROOT=${BYJY_ENGINE_FOUNDATION_INCLUDE_SNAPSHOT_DIR}"
        "-DLIB_FILE=$<TARGET_FILE:EngineFoundation>"
        "-DLAYER_NAME=EngineFoundation"
        "-DLAYER_TOKEN=engine_foundation"
        "-DCONFIG_TREE=x64-$<CONFIG>"
        "-DCACHE_ROOT=${BYJY_BANBEN_CACHE_ROOT}"
        "-DPROJECT_ROOT=${BYJY_PROJECT_ROOT}"
        "-DKEEP=8"
        -P "${PROJECT_ROOT_DIR}/cmake/发布版本缓存.cmake"
    COMMENT "发布 EngineFoundation 版本缓存"
    VERBATIM
)
add_dependencies(EngineFoundationVersionPublish EngineFoundationIncludeSnapshot)

# =============================================================================
# 7. 平台兼容性设置（编译选项、宏定义、输出目录）
# =============================================================================
if(MSVC)
    # MSVC 特有选项：
    # /MP   : 多核编译加速
    # /utf-8: 指定源文件字符集为 UTF-8，避免中文乱码警告
    target_compile_options(EngineFoundation PRIVATE /MP /utf-8)
    # 抑制 fopen 等传统 C 函数的安全警告（本层与下游均可能用到）
    target_compile_definitions(EngineFoundation PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # GCC / Clang 通用警告选项（严苛但有助于发现潜在问题）
    target_compile_options(EngineFoundation PRIVATE -Wall -Wextra -pedantic)
    # GNU/Linux 需要的宏定义
    target_compile_definitions(EngineFoundation PRIVATE _GNU_SOURCE)
endif()

# 静态库输出到 <构建树>/lib（本层不产出可执行文件）
set_target_properties(EngineFoundation PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"   # 静态库 .lib / .a
)

# =============================================================================
# 8. 安装规则（方便部署）
# =============================================================================
# 头文件按「项目根相对」层级安装到 include/Engine/EngineFoundation/，
# 与源码内 Engine/EngineFoundation/... 的包含写法保持一致。
# 注意：安装规则只覆盖源码树里的头；构建期生成的 版本锚点.h 不在其中。
#       当前没有安装使用方；若将来以 install 产物对外分发，需补装该锚点头，
#       否则消费方拿不到版本校验。
install(DIRECTORY "${PROJECT_ROOT_DIR}/"
    DESTINATION "include/Engine/EngineFoundation"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"   # 只安装 .h 和 .hpp
    PATTERN "Private" EXCLUDE                     # 排除 Private 子目录（内部实现）
)

# 将静态库安装到 lib/ 下
install(TARGETS EngineFoundation
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
)