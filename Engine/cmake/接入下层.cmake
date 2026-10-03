# =============================================================================
# 接入下层 —— 通用接入函数（EngineSystem / Engine / Test / Game 四份逐字节相同）
# -----------------------------------------------------------------------------
# 位置：<层>/cmake/接入下层.cmake
#
# 契约
#     每层各自维护 <层>/cmake/对外接口.cmake 声明本层对外面。
#     「本层对外面 = 本层自有面 + 下层对外面」，并入动作由 对外接口.cmake include
#     下层接口文件完成（纯数据赋值，重复 include 无副作用）。
#     本文件提供三个通用函数，四个副本必须逐字节一致：
#       byjy_jieru_xiaceng()        接入「下层已构建好的静态库」
#       byjy_kuaizhao_lujing()      包含目录列表的「源码树 → 头快照」逐条换算
#       byjy_qiehuan_tou_kuaizhao() 导入目标的「源码树包含目录」改指头快照
#
# 函数名为什么是拼音
#     CMake 命令名只能是 ASCII 标识符；写中文会在调用处直接报
#     「Parse error. Expected a command name, got unquoted argument」。
#
# 用法
#     include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")
#     byjy_jieru_xiaceng(<下层对外接口文件> <对外接口变量前缀> <覆盖库路径变量名>)
#
# 变量前缀对应的六个对外接口变量
#     <前缀>_OUT_LIB     本层对外传递的静态库名列表（分号分隔）；不产出库时留空
#     <前缀>_OUT_LIBDIR  与 OUT_LIB 一一对应的「该库所在层目录」；可选，
#                        省略或少于库数时，缺少的项按「本层目录」补齐
#     <前缀>_OUT_INC     包含目录
#     <前缀>_OUT_DEF     编译定义
#     <前缀>_OUT_LINK    第三方链接库（文件路径）
#     <前缀>_OUT_SYS     系统库
#
# 库定位顺序（任何情况下都不回退编译下层源码）
#     ① 覆盖变量非空 → 按元素下标与 OUT_LIB 逐项对应，元素个数须与 OUT_LIB 等长；
#        指向的文件不存在 → FATAL_ERROR，不静默降级
#     ② 覆盖变量为空 → 扫 <该库所在层目录>/out/build/*/lib/<库名>.lib，取时间戳最新的一份
#     ③ 仍没有 → FATAL_ERROR，并给出构建产出该库的下层命令
#
# 行为
#     按 OUT_LIB 逐库建立 IMPORTED STATIC 目标（GLOBAL），把本层对外接口里的
#     包含目录 / 编译定义 / 链接库挂到其 INTERFACE 上，供本层向上继续传递。
#     OUT_LIB 为空时只校验接口存在，不建目标。
# =============================================================================

include_guard(GLOBAL)

function(byjy_jieru_xiaceng interface_file prefix override_var)
    if(NOT EXISTS "${interface_file}")
        message(FATAL_ERROR
            "接入下层：找不到下层对外接口文件\n"
            "  期望位置：${interface_file}")
    endif()

    # 读入下层对外面（含其自身已并入的更下层对外面）
    include("${interface_file}")

    set(_name_lib    "${prefix}_OUT_LIB")
    set(_name_libdir "${prefix}_OUT_LIBDIR")
    set(_name_inc    "${prefix}_OUT_INC")
    set(_name_def    "${prefix}_OUT_DEF")
    set(_name_link   "${prefix}_OUT_LINK")
    set(_name_sys    "${prefix}_OUT_SYS")

    # OUT_LIBDIR 为可选扩展项，其余五项是契约必需项
    foreach(_name IN ITEMS "${_name_lib}" "${_name_inc}" "${_name_def}" "${_name_link}" "${_name_sys}")
        if(NOT DEFINED ${_name})
            message(FATAL_ERROR
                "接入下层：${interface_file} 未定义变量 ${_name}\n"
                "  契约要求对外接口定义 <变量前缀>_OUT_LIB / _OUT_INC / _OUT_DEF / _OUT_LINK / _OUT_SYS（_OUT_LIBDIR 可选）。")
        endif()
    endforeach()

    set(_lib_names    "${${_name_lib}}")
    set(_include_dirs "${${_name_inc}}")
    set(_compile_defs "${${_name_def}}")
    set(_link_libs    "${${_name_link}}")
    set(_system_libs  "${${_name_sys}}")

    # 下层目录：对外接口固定位于 <下层>/cmake/对外接口.cmake
    get_filename_component(_layer_dir "${interface_file}/../.." ABSOLUTE)
    get_filename_component(_layer_name "${_layer_dir}" NAME)

    if("${_lib_names}" STREQUAL "")
        message(STATUS "接入下层：${_layer_name} 不产出静态库，跳过库定位")
        return()
    endif()

    # ---------- OUT_LIBDIR 逐项补齐（缺省 = 本层目录） ----------
    set(_lib_dirs "")
    if(DEFINED ${_name_libdir})
        set(_lib_dirs "${${_name_libdir}}")
    endif()

    set(_lib_dirs_filled "")
    set(_fill_idx 0)
    foreach(_lib_name IN LISTS _lib_names)
        list(LENGTH _lib_dirs _dir_count)
        if(_fill_idx LESS _dir_count)
            list(GET _lib_dirs ${_fill_idx} _this_dir)
        else()
            set(_this_dir "${_layer_dir}")
        endif()
        list(APPEND _lib_dirs_filled "${_this_dir}")
        math(EXPR _fill_idx "${_fill_idx}+1")
    endforeach()
    set(_lib_dirs "${_lib_dirs_filled}")

    # ---------- 覆盖变量展开（须与 OUT_LIB 等长） ----------
    set(_override_list "${${override_var}}")
    list(LENGTH _lib_names _lib_count)
    list(LENGTH _override_list _override_count)
    if(_override_count GREATER 0 AND NOT _override_count EQUAL _lib_count)
        message(FATAL_ERROR
            "接入下层：${override_var} 的元素个数与 ${prefix}_OUT_LIB 不一致\n"
            "  ${override_var}：${_override_list}（${_override_count} 项）\n"
            "  ${prefix}_OUT_LIB：${_lib_names}（${_lib_count} 项）\n"
            "  该变量一旦非空就不再自动探测；请给出与库数等长的分号列表，或把它置空。")
    endif()

    # ---------- 逐库定位并建立导入目标 ----------
    set(_idx 0)
    foreach(_lib_name IN LISTS _lib_names)
        list(GET _lib_dirs ${_idx} _lib_dir)

        set(_lib_file "")

        # ① 覆盖变量优先
        if(_override_count GREATER 0)
            list(GET _override_list ${_idx} _override_one)
            if(EXISTS "${_override_one}")
                set(_lib_file "${_override_one}")
            else()
                message(FATAL_ERROR
                    "接入下层：${override_var} 第 ${_idx} 项指向的文件不存在\n"
                    "  取值：${_override_one}\n"
                    "  该变量一旦非空就不再自动探测，也不会退回编译下层源码。\n"
                    "  请改成正确路径，或把它置空后改用自动探测。")
            endif()
        endif()

        # ② 自动探测最新的一份
        if("${_lib_file}" STREQUAL "")
            file(GLOB _candidates "${_lib_dir}/out/build/*/lib/${_lib_name}.lib")
            set(_newest_stamp "0")
            foreach(_candidate IN LISTS _candidates)
                file(TIMESTAMP "${_candidate}" _candidate_stamp "%Y%m%d%H%M%S")
                if(_candidate_stamp GREATER _newest_stamp)
                    set(_newest_stamp "${_candidate_stamp}")
                    set(_lib_file "${_candidate}")
                endif()
            endforeach()
        endif()

        # ③ 都没有：硬失败，不回退编源码
        if("${_lib_file}" STREQUAL "")
            message(FATAL_ERROR
                "接入下层：找不到 ${_lib_name} 的静态库 ${_lib_name}.lib\n"
                "  查找位置：${_lib_dir}/out/build/<配置>/lib/\n"
                "  本函数不会退回编译下层源码，请先构建产出该库的下层：\n"
                "      cmake -S \"${_lib_dir}\" -B \"${_lib_dir}/out/build/x64-Debug\" -G Ninja\n"
                "      cmake --build \"${_lib_dir}/out/build/x64-Debug\"\n"
                "  或显式指定库路径：\n"
                "      cmake -S <本层目录> -B <构建目录> -D${override_var}=<与库数等长的分号列表>")
        endif()

        if(NOT TARGET ${_lib_name})
            add_library(${_lib_name} STATIC IMPORTED GLOBAL)
        endif()
        set_target_properties(${_lib_name} PROPERTIES IMPORTED_LOCATION "${_lib_file}")
        set_target_properties(${_lib_name} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_include_dirs}"
            INTERFACE_COMPILE_DEFINITIONS "${_compile_defs}"
            INTERFACE_LINK_LIBRARIES "${_link_libs};${_system_libs}"
        )

        message(STATUS "接入下层：${_layer_name} → ${_lib_name}\n"
                       "          库文件：${_lib_file}")
        math(EXPR _idx "${_idx}+1")
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# 下层头快照根解析 —— 由「已接入的导入目标」反推其构建目录里的头快照根
# -----------------------------------------------------------------------------
# 用法
#     byjy_tou_kuaizhao_gen(<结果变量> <导入目标名>)
#
# 行为
#     取 <导入目标> 的 IMPORTED_LOCATION（<下层构建目录>/lib/<库名>.lib），
#     上溯两级得构建目录，拼接 /include 即头快照根；目录不存在即 FATAL_ERROR
#     （提示先构建产出该快照的下层），不静默退回源码树。
#
# 为什么消费层要把快照根显式列进自己的包含目录，而不是改导入目标的对外面
#     对外接口里的「项目根」是 Engine/... 的前缀解析根，但它同时也能解析到
#     引擎源码树里的同名头。若把它交给「导入目标」传递，CMake 会把它排进
#     /external:I 组；而目标自身的包含目录（-I）恒在该组之前 —— 所以只要把
#     快照根作为自身包含目录列出，它就一定先于「项目根」命中，无需改动导入目标。
#     反过来，若改成「把导入目标里的项目根换成快照根」，项目根就只剩自身包含
#     目录这一个来源、反而排到了快照之前，快照会被旁路。
# -----------------------------------------------------------------------------
function(byjy_tou_kuaizhao_gen out_var imported_target)
    if(NOT TARGET ${imported_target})
        message(FATAL_ERROR "头快照解析：导入目标 ${imported_target} 不存在")
    endif()

    get_target_property(_lib_file ${imported_target} IMPORTED_LOCATION)
    get_filename_component(_build_dir "${_lib_file}/../.." ABSOLUTE)
    set(_snapshot "${_build_dir}/include")

    if(NOT IS_DIRECTORY "${_snapshot}")
        message(FATAL_ERROR
            "头快照解析：找不到 ${imported_target} 的头快照目录\n"
            "  期望位置：${_snapshot}\n"
            "  该目录由下层构建时导出，请先构建产出它的下层：\n"
            "      cmake --build <下层目录>/out/build/x64-Debug")
    endif()

    set(${out_var} "${_snapshot}" PARENT_SCOPE)
    message(STATUS "头快照解析：${imported_target} → ${_snapshot}")
endfunction()