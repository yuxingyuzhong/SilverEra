# =============================================================================
# 引擎层对外头快照导出（以 cmake -P 脚本方式运行，由 CMakeLists 的常驻目标每轮调用）
# -----------------------------------------------------------------------------
# 目的
#     上层（EngineSystem / EngineRenderer / Test）改为「按与本层静态库同版次的头文件」编译，
#     不再实时读取本层源码树，避免出现「下层头已改、静态库未重建」时新头配旧库地编译链接。
#
# 做法
#     把本层对外包含目录下的头文件镜像到 <构建目录>/include/：
#       - 只复制内容有变化的文件（ONLY_IF_DIFFERENT），未变头保持原 mtime，
#         从而不会触发上层无谓重编；
#       - 清理快照中源侧已删除的头，避免已删头残留在快照里被包含到。
#
# 参数
#     SRC_ROOT    本层源码根
#     DST_ROOT    快照根（通常为 <构建目录>/include）
#     MIRROR_DIRS 需镜像的相对目录列表（与本层对外包含目录一一对应的子路径）
#     ANCHOR_SRC  可选：版次锚点头的源路径（构建期生成物）
#     ANCHOR_REL  可选：锚点头在快照中的相对路径（如 Engine/EngineFoundation/版本锚点.h）
# =============================================================================

if(NOT SRC_ROOT OR NOT DST_ROOT OR NOT MIRROR_DIRS)
    message(FATAL_ERROR "导出头快照：缺少参数 SRC_ROOT / DST_ROOT / MIRROR_DIRS")
endif()

# 快照根先建出来，即使本轮无需复制也保证目录存在
file(MAKE_DIRECTORY "${DST_ROOT}")

# 收集源侧全部头文件（记录相对 SRC_ROOT 的路径）
set(SRC_HEADERS "")
foreach(mirror_dir IN LISTS MIRROR_DIRS)
    foreach(header_ext IN ITEMS ".h" ".hpp" ".inl")
        file(GLOB_RECURSE found_headers "${SRC_ROOT}/${mirror_dir}/*${header_ext}")
        foreach(header_file IN LISTS found_headers)
            file(RELATIVE_PATH header_rel "${SRC_ROOT}" "${header_file}")
            list(APPEND SRC_HEADERS "${header_rel}")
        endforeach()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES SRC_HEADERS)
list(SORT SRC_HEADERS)

# 版次锚点头：由构建期生成，不在源码树里，需单独搬进快照
# 说明：把它登记进 SRC_HEADERS，后面的「失效清理」才不会把它当残留删掉。
if(ANCHOR_SRC)
    if(NOT ANCHOR_REL)
        message(FATAL_ERROR "导出头快照：给出了 ANCHOR_SRC 却缺少 ANCHOR_REL")
    endif()
    if(NOT EXISTS "${ANCHOR_SRC}")
        message(FATAL_ERROR "导出头快照：锚点头不存在 ${ANCHOR_SRC}")
    endif()
    set(_anchor_dst "${DST_ROOT}/${ANCHOR_REL}")
    get_filename_component(_anchor_dir "${_anchor_dst}" DIRECTORY)
    file(MAKE_DIRECTORY "${_anchor_dir}")
    file(COPY_FILE "${ANCHOR_SRC}" "${_anchor_dst}" ONLY_IF_DIFFERENT)
    list(APPEND SRC_HEADERS "${ANCHOR_REL}")
    list(REMOVE_DUPLICATES SRC_HEADERS)
endif()

# 复制有变化的文件（仅内容不同才落盘，未变文件保持原时间戳）
foreach(header_rel IN LISTS SRC_HEADERS)
    # 锚点头已在上面从构建目录单独复制，源码树下没有同名文件，跳过
    if(ANCHOR_REL AND "${header_rel}" STREQUAL "${ANCHOR_REL}")
        continue()
    endif()
    set(header_src "${SRC_ROOT}/${header_rel}")
    set(header_dst "${DST_ROOT}/${header_rel}")
    # 目标缺失或源更新时才进入复制分支
    if(NOT EXISTS "${header_dst}" OR "${header_src}" IS_NEWER_THAN "${header_dst}")
        get_filename_component(header_dst_dir "${header_dst}" DIRECTORY)
        file(MAKE_DIRECTORY "${header_dst_dir}")
        file(COPY_FILE "${header_src}" "${header_dst}" ONLY_IF_DIFFERENT)
    endif()
endforeach()

# 清理快照中源侧已不存在的头
set(stale_count 0)
foreach(header_ext IN ITEMS ".h" ".hpp" ".inl")
    file(GLOB_RECURSE snapshot_headers "${DST_ROOT}/*${header_ext}")
    foreach(snapshot_file IN LISTS snapshot_headers)
        file(RELATIVE_PATH snapshot_rel "${DST_ROOT}" "${snapshot_file}")
        if(NOT snapshot_rel IN_LIST SRC_HEADERS)
            file(REMOVE "${snapshot_file}")
            math(EXPR stale_count "${stale_count}+1")
        endif()
    endforeach()
endforeach()

list(LENGTH SRC_HEADERS header_count)
message(STATUS "导出头快照：头文件 ${header_count} 个 / 清理失效 ${stale_count} 个 → ${DST_ROOT}")