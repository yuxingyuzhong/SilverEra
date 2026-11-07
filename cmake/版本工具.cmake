# =============================================================================
# 版本工具（共享构建工具，由各层 CMakeLists 在配置期 include）
# -----------------------------------------------------------------------------
# 位置：Engine/EngineFoundation/cmake/版本工具.cmake
#
# 提供四件事
#     1) byjy_banben_duqu     读某棵构建树 / 缓存目录里本层的版本哈希
#     2) byjy_banben_jiexi    按版本锁解析某层的「库 + 头根 + 版本」
#     3) byjy_banben_hedui    校验「被消费层构建时用的依赖版本」与本次解析结果一致
#     4) byjy_jiaoyan_xiacengtou  兜底：本层源码引用的下层头必须都在下层快照里
#
# 版本是什么
#     每层构建期生成的 版本锚点.h 里有符号 byjy_stamp_<层标识>_<12 位哈希>，
#     该哈希 = 「对外头集合 + 构建类型 + 运行库口径」，即版本号。Debug / Release 不同。
#
# 目录布局（版本缓存）
#     Engine/版本缓存/<层>/x64-<Debug|Release>/<版本哈希>/
#         include/Engine/<层>/...        与构建树快照逐字节一致的对外头
#         lib/<层>.lib                   与上述头同版次的静态库
#         依赖版本.cmake                 该层构建时使用的各下层版本
#     由各层构建成功后的 发布版本缓存 自定义目标写入；空锁时不会用到。
#
# 为什么锁空走构建树、锁非空走缓存
#     锁空 = 「跟随最新」：直接读下层构建树，天然与下层当前库同版次，且不需要维护副本。
#     锁非空 = 「停在旧版」：只有缓存里才有旧版本的头与库，故必须从缓存取。
# =============================================================================

include_guard(GLOBAL)

# 版本锁目录 / 版本缓存根：均以本文件为基准上溯到 Engine/
get_filename_component(BYJY_BANBEN_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(BYJY_BANBEN_CACHE_ROOT "${BYJY_BANBEN_ENGINE_ROOT}/版本缓存")
set(BYJY_BANBEN_LOCK_FILE "${BYJY_BANBEN_ENGINE_ROOT}/版本锁.cmake")

# -----------------------------------------------------------------------------
# 读版本哈希
#   byjy_banben_duqu(<头快照根> <层名> <层标识> <输出变量>)
#   头快照根即包含 Engine/<层>/... 的那一层目录；目录里没有锚点头时输出空串。
# -----------------------------------------------------------------------------
function(byjy_banben_duqu _include_root _layer_name _token _out_var)
    set(_header "${_include_root}/Engine/${_layer_name}/版本锚点.h")
    set(_version "")
    if(EXISTS "${_header}")
        file(STRINGS "${_header}" _lines REGEX "byjy_stamp_")
        foreach(_line IN LISTS _lines)
            # 只认本层标识对应的符号，避免把别的层的锚点误当成本层
            if(_line MATCHES "byjy_stamp_${_token}_([0-9a-f]+)")
                set(_version "${CMAKE_MATCH_1}")
                break()
            endif()
        endforeach()
    endif()
    set(${_out_var} "${_version}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# 最新构建树定位
#   byjy_banben_zuixin(<层名> <层目录> <Debug|Release> <输出根> <输出包含根>)
#   锁空时用它决定「用哪棵树」；锁非空时用它做「锁是否滞后」的对照。
# -----------------------------------------------------------------------------
function(byjy_banben_zuixin _layer_name _layer_dir _cfg_label _out_root _out_inc)
    set(_tree "x64-${_cfg_label}")

    # 候选①：Ninja 双预设约定 —— <层>/out/build/x64-<配置>/{lib/<库>.lib, include/}
    set(_root "${_layer_dir}/out/build/${_tree}")
    set(_lib "${_root}/lib/${_layer_name}.lib")
    set(_inc "${_root}/include")

    # 候选②：多配置生成器（VS）约定 —— <层>/out/build/<任意树>/lib/<配置>/<库>.lib，
    #        头快照在 <该树>/include/<配置>/。多棵树并存时取库文件时间戳最新的一棵。
    if(NOT EXISTS "${_lib}" OR NOT IS_DIRECTORY "${_inc}")
        set(_best_tree "")
        set(_best_stamp "")
        file(GLOB _trees LIST_DIRECTORIES true "${_layer_dir}/out/build/*")
        foreach(_cand_tree IN LISTS _trees)
            set(_cand_lib "${_cand_tree}/lib/${_cfg_label}/${_layer_name}.lib")
            if(EXISTS "${_cand_lib}")
                file(TIMESTAMP "${_cand_lib}" _cand_stamp "%Y%m%d%H%M%S")
                if("${_best_tree}" STREQUAL "" OR "${_cand_stamp}" GREATER "${_best_stamp}")
                    set(_best_tree "${_cand_tree}")
                    set(_best_stamp "${_cand_stamp}")
                endif()
            endif()
        endforeach()
        if(NOT "${_best_tree}" STREQUAL "")
            set(_root "${_best_tree}")
            set(_lib "${_root}/lib/${_cfg_label}/${_layer_name}.lib")
            set(_inc "${_root}/include/${_cfg_label}")
        endif()
    endif()

    set(${_out_root} "${_root}" PARENT_SCOPE)
    set(${_out_inc} "${_inc}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# 按版本锁解析某层
#   byjy_banben_jiexi(<层名> <层标识> <层目录> <Debug|Release>
#                     <输出库路径> <输出包含根> <输出版本>)
#   并在调用方作用域写 BYJY_RESOLVED_VERSION_<层名> / BYJY_RESOLVED_ROOT_<层名>，
#   供后续一致性校验与依赖版本记录使用。
# -----------------------------------------------------------------------------
function(byjy_banben_jiexi _layer_name _token _layer_dir _cfg_label
        _out_lib _out_inc _out_version)
    # 读锁（纯数据赋值，重复 include 无副作用）
    if(EXISTS "${BYJY_BANBEN_LOCK_FILE}")
        include("${BYJY_BANBEN_LOCK_FILE}")
    endif()

    set(_lock "${BYJY_LOCK_${_layer_name}_${_cfg_label}}")
    set(_tree "x64-${_cfg_label}")

    # 锁空 → 跟随最新构建树；锁非空 → 锁定版本缓存
    if("${_lock}" STREQUAL "")
        byjy_banben_zuixin("${_layer_name}" "${_layer_dir}" "${_cfg_label}" _root _inc)
        set(_lib "${_root}/lib/${_layer_name}.lib")
        if(NOT EXISTS "${_lib}" AND EXISTS "${_root}/lib/${_cfg_label}/${_layer_name}.lib")
            set(_lib "${_root}/lib/${_cfg_label}/${_layer_name}.lib")
        endif()
        message(STATUS "版本：${_layer_name} / ${_cfg_label} → 跟随最新（${_root}）")
    else()
        set(_root "${BYJY_BANBEN_CACHE_ROOT}/${_layer_name}/${_tree}/${_lock}")
        set(_lib "${_root}/lib/${_layer_name}.lib")
        set(_inc "${_root}/include")
        if(NOT EXISTS "${_lib}" OR NOT IS_DIRECTORY "${_inc}")
            message(FATAL_ERROR
                "版本锁：找不到 ${_layer_name} 的锁定版本 ${_lock}（${_cfg_label}）\n"
                "  期望目录：${_root}\n"
                "  该目录由 ${_layer_name} 构建成功后的「发布版本缓存」目标产出。\n"
                "  处理方式：① 先构建 ${_layer_name} 的 ${_cfg_label} 树以产出该版本；\n"
                "            ② 或把 Engine/版本锁.cmake 里对应条目清空（改为跟随最新）；\n"
                "            ③ 或改成缓存里确实存在的版本哈希。")
        endif()
        message(STATUS "版本：${_layer_name} / ${_cfg_label} → 锁定 ${_lock}（${_root}）")

        # 滞后告警：锁值落后于当前构建树时不阻断（停在旧版本是合法选择），但必须可见，
        # 否则「锁了旧版却以为在测最新」会一直静默成立。
        byjy_banben_zuixin("${_layer_name}" "${_layer_dir}" "${_cfg_label}" _latest_root _latest_inc)
        if(IS_DIRECTORY "${_latest_inc}")
            byjy_banben_duqu("${_latest_inc}" "${_layer_name}" "${_token}" _latest_version)
            if(NOT "${_latest_version}" STREQUAL "" AND NOT "${_latest_version}" STREQUAL "${_lock}")
                message(WARNING
                    "版本锁滞后：${_layer_name} / ${_cfg_label} 锁定 ${_lock}，当前构建树是 ${_latest_version}\n"
                    "  锁定版本仍可用（取自 Engine/版本缓存），但已不是最新构建结果。\n"
                    "  升级请成套修改 Engine/版本锁.cmake（Foundation / System / Renderer × 两配置），\n"
                    "  或运行工作站脚本 升级测试锁.ps1。")
            endif()
        endif()
    endif()

    byjy_banben_duqu("${_inc}" "${_layer_name}" "${_token}" _version)

    set(${_out_lib} "${_lib}" PARENT_SCOPE)
    set(${_out_inc} "${_inc}" PARENT_SCOPE)
    set(${_out_version} "${_version}" PARENT_SCOPE)
    set(BYJY_RESOLVED_VERSION_${_layer_name}_${_cfg_label} "${_version}" PARENT_SCOPE)
    set(BYJY_RESOLVED_ROOT_${_layer_name}_${_cfg_label} "${_root}" PARENT_SCOPE)
    set(BYJY_RESOLVED_INC_${_layer_name}_${_cfg_label} "${_inc}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# 按版本锁解析某层（双配置）
#   byjy_banben_jiexi_shuang(<层名> <层标识> <层目录>
#                            <输出库路径基名> <输出包含根基名> <输出版本基名>)
#   对 Debug / Release 各解析一次，输出 <基名>_Debug / <基名>_Release，
#   并写 BYJY_RESOLVED_VERSION_<层>_<配置> / BYJY_RESOLVED_ROOT_<层>_<配置>。
#   多配置生成器下由调用方用 $<CONFIG> 选取；单配置生成器等价于只取了当前配置。
# -----------------------------------------------------------------------------
function(byjy_banben_jiexi_shuang _layer_name _token _layer_dir _out_lib _out_inc _out_version)
    foreach(_cfg IN ITEMS Debug Release)
        byjy_banben_jiexi("${_layer_name}" "${_token}" "${_layer_dir}" "${_cfg}"
            _byjy_lib_${_cfg} _byjy_inc_${_cfg} _byjy_ver_${_cfg})

        set(${_out_lib}_${_cfg} "${_byjy_lib_${_cfg}}" PARENT_SCOPE)
        set(${_out_inc}_${_cfg} "${_byjy_inc_${_cfg}}" PARENT_SCOPE)
        set(${_out_version}_${_cfg} "${_byjy_ver_${_cfg}}" PARENT_SCOPE)
        set(BYJY_RESOLVED_VERSION_${_layer_name}_${_cfg} "${_byjy_ver_${_cfg}}" PARENT_SCOPE)
        set(BYJY_RESOLVED_ROOT_${_layer_name}_${_cfg}
            "${BYJY_RESOLVED_ROOT_${_layer_name}_${_cfg}}" PARENT_SCOPE)
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# 一致性校验（按配置）
#   byjy_banben_hedui(<消费方显示名> <被校验的已解析层名> <Debug|Release>)
#   读该层对应配置目录里的 依赖版本.cmake（它构建时用的各下层版本），与本次解析结果逐项比对：
#   不一致说明「该层的静态库是用另一版下层编出来的」，链接期必然锚点失配，故提前报错。
# -----------------------------------------------------------------------------
function(byjy_banben_hedui _label _dep_name _cfg_label)
    # 该层该配置未参与本次解析（例如对应构建树不存在）
    if(NOT DEFINED BYJY_RESOLVED_ROOT_${_dep_name}_${_cfg_label})
        return()
    endif()
    # 该配置的下层构建树 / 缓存条目不存在：说明本配置根本没参与本次构建，静默跳过
    if(NOT IS_DIRECTORY "${BYJY_RESOLVED_ROOT_${_dep_name}_${_cfg_label}}")
        return()
    endif()

    set(_record "${BYJY_RESOLVED_ROOT_${_dep_name}_${_cfg_label}}/依赖版本.cmake")
    if(NOT EXISTS "${_record}")
        message(WARNING
            "${_label}：${_dep_name}（${_cfg_label}）缺少依赖版本记录（${_record}），跳过全链一致性校验。\n"
            "          该记录由对应层配置期写出；若为旧构建树，重建一次即可生成。")
        return()
    endif()

    include("${_record}")
    foreach(_other IN ITEMS EngineFoundation EngineSystem EngineRenderer)
        set(_built "${BYJY_BUILD_AGAINST_${_other}_${_cfg_label}}")
        set(_used  "${BYJY_RESOLVED_VERSION_${_other}_${_cfg_label}}")
        # 两边都有值且不相等 → 版本错配
        if(NOT "${_built}" STREQUAL "" AND NOT "${_used}" STREQUAL "" AND NOT "${_built}" STREQUAL "${_used}")
            message(FATAL_ERROR
                "${_label}：版本错配（${_cfg_label}），拒绝继续配置。\n"
                "  ${_dep_name} 构建时使用的是 ${_other} = ${_built}\n"
                "  本次解析到的      ${_other} = ${_used}\n"
                "  两者不同会导致 ${_dep_name}.lib 引用的锚点符号在链接期找不到（LNK2019）。\n"
                "  处理方式（任选其一）：\n"
                "    ① 重建 ${_dep_name} 的 ${_cfg_label} 树，让它跟上 ${_other} 的当前版本；\n"
                "    ② 在 Engine/版本锁.cmake 里把 ${_other} 的 ${_cfg_label} 条目锁到 ${_built}（全链同版）。")
        endif()
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# 记录依赖版本（按配置）
#   byjy_banben_jilu(<本层构建目录>)
#   把本次解析到的各下层版本（Debug / Release 各一份）写进 <构建目录>/依赖版本.cmake；
#   上层据此做一致性校验，发布版本缓存时该文件也会一并拷入缓存条目。
# -----------------------------------------------------------------------------
function(byjy_banben_jilu _build_dir)
    set(_text "# 本层构建时使用的下层版本（构建期生成物，勿手改）\n")
    foreach(_other IN ITEMS EngineFoundation EngineSystem EngineRenderer)
        foreach(_cfg IN ITEMS Debug Release)
            if(DEFINED BYJY_RESOLVED_VERSION_${_other}_${_cfg})
                string(APPEND _text
                    "set(BYJY_BUILD_AGAINST_${_other}_${_cfg} \"${BYJY_RESOLVED_VERSION_${_other}_${_cfg}}\")\n")
            endif()
        endforeach()
    endforeach()
    file(WRITE "${_build_dir}/依赖版本.cmake" "${_text}")
endfunction()

# -----------------------------------------------------------------------------
# 兜底：下层头完整性校验
#   byjy_jiaoyan_xiacengtou(<消费方显示名> <下层包含根> <下层名> <待扫描目录>...)
#
#   快照只在下层「构建成功」后才导出，因此下层新增 / 重命名的公共头不会出现在快照里；
#   此时上层的 #include 会继续向下搜到项目根的源码树并被编进去（头一套、库一套）。
#   本函数以快照为「合法头集合」，扫描本层源码里对该层的全部 #include，缺失即硬失败。
#
#   只匹配「行首的 #include」（允许前置空白），避免把注释里提到的路径误判。
# -----------------------------------------------------------------------------
function(byjy_jiaoyan_xiacengtou _label _inc_root _layer_name)
    # 快照里该层的全部公共头（相对包含根）
    file(GLOB_RECURSE _published
        "${_inc_root}/Engine/${_layer_name}/*.h"
        "${_inc_root}/Engine/${_layer_name}/*.hpp"
        "${_inc_root}/Engine/${_layer_name}/*.inl")
    set(_known "")
    foreach(_file IN LISTS _published)
        file(RELATIVE_PATH _rel "${_inc_root}" "${_file}")
        string(REPLACE "\\" "/" _rel "${_rel}")
        list(APPEND _known "${_rel}")
    endforeach()

    # 扫描本层源码（ARGN = 目录列表）
    set(_missing "")
    foreach(_dir IN LISTS ARGN)
        if(NOT IS_DIRECTORY "${_dir}")
            continue()
        endif()
        file(GLOB_RECURSE _sources
            "${_dir}/*.h" "${_dir}/*.hpp" "${_dir}/*.inl"
            "${_dir}/*.cpp" "${_dir}/*.c")
        foreach(_src IN LISTS _sources)
            file(STRINGS "${_src}" _lines)
            foreach(_line IN LISTS _lines)
                # 行首（允许缩进）的 #include，且路径属于被校验的下层
                if(_line MATCHES "^[ \t]*#[ \t]*include[ \t]*[<\"](Engine/${_layer_name}/[^>\"]+)[>\"]")
                    set(_target "${CMAKE_MATCH_1}")
                    string(REPLACE "\\" "/" _target "${_target}")
                    if(NOT _target IN_LIST _known)
                        list(APPEND _missing "${_target}  ←  ${_src}")
                    endif()
                endif()
            endforeach()
        endforeach()
    endforeach()

    if(_missing)
        list(REMOVE_DUPLICATES _missing)
        list(LENGTH _missing _count)
        message(FATAL_ERROR
            "${_label}：引用了 ${_count} 个不在 ${_layer_name} 快照里的头文件。\n"
            "  这通常意味着「${_layer_name} 新增 / 重命名了公共头，但没有重新构建」。\n"
            "  快照根：${_inc_root}\n"
            "  缺失清单：\n    ${_missing}\n"
            "  处理方式：先构建 ${_layer_name} 的对应配置树（快照会随库一起导出），再重配本层。")
    endif()
    list(LENGTH _known _known_count)
    message(STATUS "${_label}：${_layer_name} 头完整性校验通过（快照含 ${_known_count} 个公共头）")
endfunction()
