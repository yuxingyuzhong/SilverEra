# =============================================================================
# 测试子工程 —— 公共构建逻辑
# -----------------------------------------------------------------------------
# 位置：Application/Test/cmake/测试子工程.cmake
#
# 作用
#     tests/Foundation、tests/System、tests/Renderer 三个独立测试子工程共用的
#     构建逻辑：解析版本锁、接入被测层静态库、校验下层头快照、统一编译口径。
#     子工程的 CMakeLists 只负责「声明依赖哪些层 + 产出什么目标」，不再重复
#     这百余行接入细节。
#
# 前提
#     调用方（子工程 CMakeLists）须先定义：
#       BYJY_SUB_ROOT     子工程根（tests/<Layer>）
#       BYJY_TEST_ROOT    Test 层根（Application/Test）
#       BYJY_PROJECT_ROOT 项目根（白银纪元）
#
# 提供
#     byjy_ceshi_zhunbei()                    编译口径 / 链接包装 / 编译数据库
#     byjy_ceshi_yilai(<标签> <层名> <前缀> <标识>)
#                                             解析并接入一个下层（双配置）
#     byjy_ceshi_tou_jiaoyan(<标签> <目录>...)  逐配置做头快照存在性与头完整性校验
#
# 版本语义（与主层一致）
#     Engine/版本锁.cmake 里留空 → 跟随该层最新构建树（开发期）
#     填了哈希 → 取 Engine/版本缓存/<层>/x64-<配置>/<哈希>/ 的冻结副本
#     子工程只读版本锁，绝不改写；升级锁定由工作站脚本统一成套完成。
# =============================================================================

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/接入下层.cmake")
include("${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/版本工具.cmake")

# -----------------------------------------------------------------------------
# 编译口径：C++20、警告不阻断、UTF-8、链接响应文件 BOM 包装
# -----------------------------------------------------------------------------
function(byjy_ceshi_zhunbei)
    if(MSVC)
        add_compile_options(/WX-)
    endif()

    # 标准与编译数据库写回调用方作用域：函数作用域内的赋值不会影响后续目标
    set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON PARENT_SCOPE)

    # Ninja 写出的链接响应文件无 BOM，link.exe 按 ANSI 解读中文对象名，会报 LNK1104；
    # 与主层使用同一个包装脚本（仅 Ninja 生成器生效）。
    if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja" AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
        set(_wrapper "${BYJY_PROJECT_ROOT}/Engine/EngineFoundation/cmake/link_rsp_bom.ps1")
        find_program(_powershell NAMES powershell powershell.exe)
        if(EXISTS "${_wrapper}" AND _powershell)
            set(CMAKE_CXX_LINKER_LAUNCHER
                "${_powershell};-NoProfile;-ExecutionPolicy;Bypass;-File;${_wrapper}" PARENT_SCOPE)
        else()
            message(WARNING "未找到链接包装器或 PowerShell，中文对象名的链接可能报 LNK1104：${_wrapper}")
        endif()
    endif()
endfunction()

# -----------------------------------------------------------------------------
# 接入一个被测层
#   byjy_ceshi_yilai(<标签> <层名> <对外接口前缀> <版本标识>)
#
# 例：byjy_ceshi_yilai("TestSystem" EngineSystem "BYJY_ENGINE_SYSTEM" engine_system)
#
# 输出（调用方作用域）
#   <层名>_LIB_DEBUG / _RELEASE            该层分配置静态库路径
#   <层名>_SNAPSHOT_DEBUG / _RELEASE       该层分配置头快照根
#   <层名>_SNAPSHOT                        $<CONFIG> 选取的头快照根
#   <层名>                                 同名 IMPORTED 静态库目标（由接入函数建立）
#
# 附带
#   逐配置做「被接入层构建时使用的下层版本 vs 本次解析结果」的一致性校验；
#   不一致直接 FATAL_ERROR（锁必须成套）。
# -----------------------------------------------------------------------------
function(byjy_ceshi_yilai _label _layer _prefix _token)
    byjy_banben_jiexi_shuang(${_layer} ${_token} "${BYJY_PROJECT_ROOT}/Engine/${_layer}"
        _byjy_lib _byjy_snap _byjy_ver)

    # 分配置库与快照根
    set(${_layer}_LIB_DEBUG "${_byjy_lib_Debug}" PARENT_SCOPE)
    set(${_layer}_LIB_RELEASE "${_byjy_lib_Release}" PARENT_SCOPE)
    set(${_layer}_SNAPSHOT_DEBUG "${_byjy_snap_Debug}" PARENT_SCOPE)
    set(${_layer}_SNAPSHOT_RELEASE "${_byjy_snap_Release}" PARENT_SCOPE)
    set(${_layer}_SNAPSHOT
        "$<IF:$<CONFIG:Debug>,${_byjy_snap_Debug},${_byjy_snap_Release}>" PARENT_SCOPE)

    # 已解析版本/根：向上摊开，供后续层的成套校验读取（先 Foundation 后 System）
    foreach(_cfg IN ITEMS Debug Release)
        set(BYJY_RESOLVED_VERSION_${_layer}_${_cfg} "${_byjy_ver_${_cfg}}" PARENT_SCOPE)
        set(BYJY_RESOLVED_ROOT_${_layer}_${_cfg}
            "${BYJY_RESOLVED_ROOT_${_layer}_${_cfg}}" PARENT_SCOPE)
    endforeach()

    # 覆盖变量：单配置生成器取当前配置；多配置由 _DEBUG / _RELEASE 分派（见接入函数）
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo"
       OR "${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
        set(${_layer}_LIB_PATH "${_byjy_lib_Release}")
    else()
        set(${_layer}_LIB_PATH "${_byjy_lib_Debug}")
    endif()
    set(${_layer}_LIB_PATH_DEBUG "${_byjy_lib_Debug}")
    set(${_layer}_LIB_PATH_RELEASE "${_byjy_lib_Release}")
    set(${_layer}_LIB_PATH_INCLUDE_DEBUG "${_byjy_snap_Debug}")
    set(${_layer}_LIB_PATH_INCLUDE_RELEASE "${_byjy_snap_Release}")

    # 建立 IMPORTED 目标（包含路径 / 编译定义 / 系统库 / 版次锚点由下层对外面给出）
    byjy_jieru_xiaceng("${BYJY_PROJECT_ROOT}/Engine/${_layer}/cmake/对外接口.cmake"
        "${_prefix}" "${_layer}_LIB_PATH")

    # 多配置：逐配置覆盖导入库位置（单配置生成器忽略这些属性）
    foreach(_cfg IN ITEMS DEBUG RELEASE)
        set(_cfg_lib "${${_layer}_LIB_${_cfg}}")
        if(NOT "${_cfg_lib}" STREQUAL "" AND EXISTS "${_cfg_lib}")
            set_target_properties(${_layer} PROPERTIES
                IMPORTED_LOCATION_${_cfg} "${_cfg_lib}")
        endif()
    endforeach()
    if(EXISTS "${${_layer}_LIB_RELEASE}")
        set_target_properties(${_layer} PROPERTIES
            IMPORTED_LOCATION_RELWITHDEBINFO "${${_layer}_LIB_RELEASE}"
            IMPORTED_LOCATION_MINSIZEREL "${${_layer}_LIB_RELEASE}")
    endif()

    # 全链一致性（逐配置）：该层构建时用的下层版本必须与本次解析一致
    foreach(_cfg IN ITEMS Debug Release)
        byjy_banben_hedui("${_label}" ${_layer} "${_cfg}")
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# 记录依赖版本
#   byjy_ceshi_jilu()
#
# 把本子工程本次解析到的各下层版本写进 <本子工程构建目录>/依赖版本.cmake，
# 供图形界面的「层级版本新鲜度」提示读取：记录里的版本与该层当前可用版本不一致，
# 即说明「本子工程的测试库是用旧版下层编的」，界面会把该层级单独隔离并提示重建。
# 必须在全部 byjy_ceshi_yilai() 之后调用。
# -----------------------------------------------------------------------------
function(byjy_ceshi_jilu)
    byjy_banben_jilu("${CMAKE_BINARY_DIR}")
endfunction()

# -----------------------------------------------------------------------------
# 头快照校验
#   byjy_ceshi_tou_jiaoyan(<标签> <待扫描源码目录>...)
#
# ① 已接入层的头快照目录必须存在（多配置生成器缺一份即硬失败；单配置缺哪份跳过哪份）
# ② 扫描本子工程源码里对已接入层的 #include，必须都能在快照里找到
#    典型失败原因：下层新增 / 重命名公共头但没重建，头一套、库一套。
# -----------------------------------------------------------------------------
function(byjy_ceshi_tou_jiaoyan _label)
    foreach(_cfg IN ITEMS Debug Release)
        foreach(_layer IN ITEMS EngineFoundation EngineSystem EngineRenderer)
            if(NOT DEFINED ${_layer}_SNAPSHOT_${_cfg})
                continue()
            endif()
            set(_snap "${${_layer}_SNAPSHOT_${_cfg}}")
            if("${_snap}" STREQUAL "")
                continue()
            endif()
            # 快照缺失：多配置生成器要求齐备；单配置只校验当前配置，缺的那份跳过
            if(NOT IS_DIRECTORY "${_snap}")
                if(CMAKE_CONFIGURATION_TYPES)
                    message(FATAL_ERROR
                        "${_label}：${_layer} 的 ${_cfg} 头快照目录不存在：${_snap}\n"
                        "  多配置生成器要求两套快照齐备；请先构建 ${_layer} 的 ${_cfg} 树。")
                endif()
                continue()
            endif()
            byjy_jiaoyan_xiacengtou("${_label}(${_cfg})" "${_snap}" "${_layer}" ${ARGN})
        endforeach()
    endforeach()
endfunction()
