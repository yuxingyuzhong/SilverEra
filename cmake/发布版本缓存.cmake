# =============================================================================
# 发布版本缓存（cmake -P 脚本；由各层构建成功后的常驻目标调用）
# -----------------------------------------------------------------------------
# 作用
#     把本层「刚构建好」的对外头快照与静态库，按版本哈希存进共享缓存：
#       Engine/版本缓存/<层>/x64-<配置>/<版本哈希>/{include, lib/<层>.lib, 依赖版本.cmake}
#     版本锁（Engine/版本锁.cmake）里填了这个哈希，上层就会从该目录取头与库。
#
# 参数
#     SRC_BUILD_DIR  本层构建目录（含 include/ 与 lib/）
#     LAYER_NAME     层名（EngineFoundation / EngineSystem / EngineRenderer）
#     LAYER_TOKEN    层标识（engine_foundation / engine_system / renderer）
#     CONFIG_TREE    构建树目录名（x64-Debug / x64-Release）
#     CACHE_ROOT     版本缓存根（Engine/版本缓存）
#     PROJECT_ROOT   项目根（用于读取版本锁，保护被锁版本不被清理）
#     KEEP           每个 <层>/<配置> 下保留的版本数（默认 8；被锁版本永不清理）
#
# 幂等性
#     头按「内容不同才覆盖」同步（保留未变文件的 mtime，避免上层无谓重编），并清理
#     源侧已删除的头；库按内容不同才覆盖。同一版本重复发布不会产生额外变更。
# =============================================================================

if(NOT SRC_BUILD_DIR OR NOT LAYER_NAME OR NOT LAYER_TOKEN OR NOT CONFIG_TREE OR NOT CACHE_ROOT)
    message(FATAL_ERROR "发布版本缓存：缺少参数 SRC_BUILD_DIR / LAYER_NAME / LAYER_TOKEN / CONFIG_TREE / CACHE_ROOT")
endif()

if(NOT KEEP)
    set(KEEP 8)
endif()

# 头快照根 / 库文件：由调用方按当前配置给出（兼容 Ninja 的 <树>/include 与
# VS 多配置的 <树>/include/<配置>、<树>/lib/<配置>）；缺省回退到 Ninja 布局。
if(NOT SNAPSHOT_ROOT)
    set(SNAPSHOT_ROOT "${SRC_BUILD_DIR}/include")
endif()
if(NOT LIB_FILE)
    set(LIB_FILE "${SRC_BUILD_DIR}/lib/${LAYER_NAME}.lib")
endif()

# ---------- 读本层版本（锚点符号里的 12 位哈希） ----------
set(_anchor "${SNAPSHOT_ROOT}/Engine/${LAYER_NAME}/版本锚点.h")
if(NOT EXISTS "${_anchor}")
    message(FATAL_ERROR "发布版本缓存：找不到锚点头 ${_anchor}（本层库或头快照尚未构建完成）")
endif()
set(_version "")
file(STRINGS "${_anchor}" _lines REGEX "byjy_stamp_")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "byjy_stamp_${LAYER_TOKEN}_([0-9a-f]+)")
        set(_version "${CMAKE_MATCH_1}")
        break()
    endif()
endforeach()
if("${_version}" STREQUAL "")
    message(FATAL_ERROR "发布版本缓存：无法从 ${_anchor} 解析出版本哈希")
endif()

set(_dest "${CACHE_ROOT}/${LAYER_NAME}/${CONFIG_TREE}/${_version}")
file(MAKE_DIRECTORY "${_dest}/lib")

# ---------- 头快照镜像同步 ----------
set(_count 0)
set(_expected "")
file(GLOB_RECURSE _src_headers
    "${SNAPSHOT_ROOT}/Engine/${LAYER_NAME}/*.h"
    "${SNAPSHOT_ROOT}/Engine/${LAYER_NAME}/*.hpp"
    "${SNAPSHOT_ROOT}/Engine/${LAYER_NAME}/*.inl")
foreach(_src IN LISTS _src_headers)
    file(RELATIVE_PATH _rel "${SNAPSHOT_ROOT}" "${_src}")
    list(APPEND _expected "${_rel}")
    set(_dst "${_dest}/include/${_rel}")
    get_filename_component(_dir "${_dst}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    file(COPY_FILE "${_src}" "${_dst}" ONLY_IF_DIFFERENT)
    math(EXPR _count "${_count}+1")
endforeach()

# 清理本版本目录中源侧已不存在的头
file(GLOB_RECURSE _dst_headers
    "${_dest}/include/Engine/${LAYER_NAME}/*.h"
    "${_dest}/include/Engine/${LAYER_NAME}/*.hpp"
    "${_dest}/include/Engine/${LAYER_NAME}/*.inl")
foreach(_dst IN LISTS _dst_headers)
    file(RELATIVE_PATH _rel "${_dest}/include" "${_dst}")
    if(NOT _rel IN_LIST _expected)
        file(REMOVE "${_dst}")
    endif()
endforeach()

# ---------- 静态库 ----------
file(COPY_FILE "${LIB_FILE}" "${_dest}/lib/${LAYER_NAME}.lib" ONLY_IF_DIFFERENT)

# ---------- 附属运行期目录（可选；如渲染层的 shaders/） ----------
# 锁定版本时，上层只会从缓存取该版本；着色器等构建期产物必须一并进来，
# 否则 Test 按 IMPORTED_LOCATION 推导出的 shaders 目录会落空。
if(EXTRA_DIR AND IS_DIRECTORY "${SRC_BUILD_DIR}/${EXTRA_DIR}")
    file(GLOB_RECURSE _extra_files "${SRC_BUILD_DIR}/${EXTRA_DIR}/*")
    foreach(_extra_src IN LISTS _extra_files)
        file(RELATIVE_PATH _extra_rel "${SRC_BUILD_DIR}" "${_extra_src}")
        set(_extra_dst "${_dest}/${_extra_rel}")
        get_filename_component(_extra_dir "${_extra_dst}" DIRECTORY)
        file(MAKE_DIRECTORY "${_extra_dir}")
        file(COPY_FILE "${_extra_src}" "${_extra_dst}" ONLY_IF_DIFFERENT)
    endforeach()
endif()

# ---------- 依赖版本记录（供上层做全链一致性校验） ----------
if(EXISTS "${SRC_BUILD_DIR}/依赖版本.cmake")
    file(COPY_FILE "${SRC_BUILD_DIR}/依赖版本.cmake"
        "${_dest}/依赖版本.cmake" ONLY_IF_DIFFERENT)
endif()

message(STATUS "版本缓存：${LAYER_NAME} / ${CONFIG_TREE} / ${_version}（头 ${_count} 个）→ ${_dest}")

# ---------- 清理旧版本（保留最近 KEEP 个；被版本锁引用的永不清理） ----------
set(_locked "")
if(PROJECT_ROOT AND EXISTS "${PROJECT_ROOT}/Engine/版本锁.cmake")
    include("${PROJECT_ROOT}/Engine/版本锁.cmake")
    foreach(_layer IN ITEMS EngineFoundation EngineSystem EngineRenderer)
        foreach(_cfg IN ITEMS Debug Release)
            if(NOT "${BYJY_LOCK_${_layer}_${_cfg}}" STREQUAL "")
                list(APPEND _locked "${BYJY_LOCK_${_layer}_${_cfg}}")
            endif()
        endforeach()
    endforeach()
endif()

file(GLOB _version_dirs LIST_DIRECTORIES true "${CACHE_ROOT}/${LAYER_NAME}/${CONFIG_TREE}/*")
set(_entries "")
foreach(_dir IN LISTS _version_dirs)
    if(NOT IS_DIRECTORY "${_dir}")
        continue()
    endif()
    get_filename_component(_name "${_dir}" NAME)
    set(_lib "${_dir}/lib/${LAYER_NAME}.lib")
    if(NOT EXISTS "${_lib}")
        continue()
    endif()
    # 用库文件的时间戳作为「该版本的发布时间」
    file(TIMESTAMP "${_lib}" _stamp "%Y%m%d%H%M%S" UTC)
    list(APPEND _entries "${_stamp}|${_name}|${_dir}")
endforeach()
list(SORT _entries)

# 从最旧的开始清理；被锁版本跳过（保留），直到剩余数量不超过 KEEP
list(LENGTH _entries _total)
set(_index 0)
while(_total GREATER KEEP AND _index LESS _total)
    list(GET _entries ${_index} _oldest)
    string(REPLACE "|" ";" _parts "${_oldest}")
    list(GET _parts 1 _old_name)
    list(GET _parts 2 _old_dir)
    # 被锁版本不清理，直接看下一个
    if(_old_name IN_LIST _locked)
        math(EXPR _index "${_index}+1")
        continue()
    endif()
    file(REMOVE_RECURSE "${_old_dir}")
    message(STATUS "版本缓存：清理旧版本 ${_old_dir}")
    list(REMOVE_AT _entries ${_index})
    math(EXPR _total "${_total}-1")
endwhile()
