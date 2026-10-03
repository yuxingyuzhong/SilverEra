# =============================================================================
# 接入下层 —— 通用接入函数（系统层 / 测试层 / 游戏层 三份逐字节相同）
# -----------------------------------------------------------------------------
# 位置：<层>/cmake/接入下层.cmake
#
# 契约
#     每层各自维护 <层>/cmake/对外接口.cmake，声明本层对外面。
#     「本层对外面 = 本层自有面 + 下层对外面」，并入动作由 <层>/cmake/对外接口.cmake
#     直接 include 下层接口文件完成（纯数据赋值，重复 include 无副作用）。
#     本文件只提供 byjy_jieru_xiaceng()，把「下层已经构建好的静态库」接进当前层。
#     三个副本必须保持一致，改动时同步三层。
#
# 函数名为什么是拼音
#     CMake 的命令名只能是 ASCII 标识符，写成 byjy_接入下层 会在调用处直接报
#     「Parse error. Expected a command name, got unquoted argument」。
#     所以这里用拼音拼写，读作「byjy 接入下层」。变量名同理，全部使用 ASCII。
#
# 用法
#     include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/接入下层.cmake")
#     byjy_jieru_xiaceng(<下层对外接口文件> <对外接口变量前缀> <覆盖库路径变量名>)
#
#     例（系统层接引擎层）：
#       byjy_jieru_xiaceng("${PROJECT_ROOT_DIR}/../引擎层/cmake/对外接口.cmake"
#                          "BYJY_ENGINE" "BYJY_ENGINE_LIB_PATH")
#
#     变量前缀对应的五个对外接口变量：
#       <前缀>_OUT_LIB   本层静态库文件名（不含扩展名）；叶层不产出库时留空
#       <前缀>_OUT_INC   包含目录
#       <前缀>_OUT_DEF   编译定义
#       <前缀>_OUT_LINK  第三方链接库（文件路径）
#       <前缀>_OUT_SYS   系统库
#
# 库定位顺序（任何情况下都不回退编译下层源码）
#     ① 覆盖变量非空且指向已存在的文件 → 用它
#        覆盖变量非空但文件不存在       → FATAL_ERROR，不静默降级
#     ② 覆盖变量为空 → 扫 <下层>/out/build/<配置>/lib/<库名>.lib，
#        多个候选取时间戳最新的一份
#     ③ 仍没有 → FATAL_ERROR，并给出构建下层的命令
#
# 行为
#     以下层对外接口里声明的库名为目标名，建立 IMPORTED STATIC 静态库（GLOBAL），
#     把包含目录 / 编译定义 / 链接库挂到它的 INTERFACE 上，供本层向上继续传递。
#     对外接口声明库名为空时（叶层不产出静态库）只校验接口存在，不建目标。
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

    set(_name_lib "${prefix}_OUT_LIB")
    set(_name_inc "${prefix}_OUT_INC")
    set(_name_def "${prefix}_OUT_DEF")
    set(_name_link "${prefix}_OUT_LINK")
    set(_name_sys "${prefix}_OUT_SYS")

    foreach(_name IN ITEMS "${_name_lib}" "${_name_inc}" "${_name_def}" "${_name_link}" "${_name_sys}")
        if(NOT DEFINED ${_name})
            message(FATAL_ERROR
                "接入下层：${interface_file} 未定义变量 ${_name}\n"
                "  契约要求对外接口定义 <变量前缀>_OUT_LIB / _OUT_INC / _OUT_DEF / _OUT_LINK / _OUT_SYS。")
        endif()
    endforeach()

    set(_lib_name "${${_name_lib}}")
    set(_include_dirs "${${_name_inc}}")
    set(_compile_defs "${${_name_def}}")
    set(_link_libs "${${_name_link}}")
    set(_system_libs "${${_name_sys}}")

    # 下层目录：对外接口固定位于 <下层>/cmake/对外接口.cmake
    get_filename_component(_layer_dir "${interface_file}/../.." ABSOLUTE)
    get_filename_component(_layer_name "${_layer_dir}" NAME)

    if(_lib_name STREQUAL "")
        message(STATUS "接入下层：${_layer_name} 不产出静态库，跳过库定位")
        return()
    endif()

    # ---------- ① 覆盖变量优先 ----------
    set(_lib_file "")
    set(_override_value "${${override_var}}")
    if(NOT "${_override_value}" STREQUAL "")
        if(EXISTS "${_override_value}")
            set(_lib_file "${_override_value}")
        else()
            message(FATAL_ERROR
                "接入下层：${override_var} 指向的文件不存在\n"
                "  取值：${_override_value}\n"
                "  该变量一旦非空就不再自动探测，也不会退回编译下层源码。\n"
                "  请改成正确路径，或把它置空后改用自动探测。")
        endif()
    endif()

    # ---------- ② 自动探测最新的一份 ----------
    if("${_lib_file}" STREQUAL "")
        file(GLOB _candidates "${_layer_dir}/out/build/*/lib/${_lib_name}.lib")
        set(_newest_stamp "0")
        foreach(_candidate IN LISTS _candidates)
            file(TIMESTAMP "${_candidate}" _candidate_stamp "%Y%m%d%H%M%S")
            if(_candidate_stamp GREATER _newest_stamp)
                set(_newest_stamp "${_candidate_stamp}")
                set(_lib_file "${_candidate}")
            endif()
        endforeach()
    endif()

    # ---------- ③ 都没有：硬失败，不回退编源码 ----------
    if("${_lib_file}" STREQUAL "")
        message(FATAL_ERROR
            "接入下层：找不到 ${_layer_name} 的静态库 ${_lib_name}.lib\n"
            "  查找位置：${_layer_dir}/out/build/<配置>/lib/\n"
            "  本函数不会退回编译下层源码，请先构建下层：\n"
            "      cmake -S \"${_layer_dir}\" -B \"${_layer_dir}/out/build/x64-Debug\" -G Ninja\n"
            "      cmake --build \"${_layer_dir}/out/build/x64-Debug\"\n"
            "  或显式指定库路径：\n"
            "      cmake -S <本层目录> -B <构建目录> -D${override_var}=<${_lib_name}.lib 绝对路径>")
    endif()

    # ---------- 建立导入目标并挂上对外的包含目录 / 编译定义 / 链接库 ----------
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
endfunction()

# -----------------------------------------------------------------------------
# 头快照路径映射 —— 把一串包含目录中「位于源根之下」的条目改指快照根
# -----------------------------------------------------------------------------
# 用法
#     byjy_kuaizhao_lujing(<结果变量> <快照根> <源根> <包含目录列表>)
#
# 行为
#     逐条换算：等于 <源根> → <快照根>；位于 <源根>/ 之下 → <快照根>/<其余部分>；
#     其余目录原样返回。结果经 PARENT_SCOPE 写回 <结果变量>，不做存在性校验。
#
# 用途
#     产出层改造自己的 target_include_directories(PUBLIC ...)：普通库目标的包含目录由
#     INCLUDE_DIRECTORIES 决定，只改 INTERFACE_INCLUDE_DIRECTORIES 不会改变它自身的编译
#     包含路径，故此处按列表换算后再传给 target_include_directories。
#     消费层改造「导入目标」请用 byjy_qiehuan_tou_kuaizhao()（含快照缺失的硬失败）。
# -----------------------------------------------------------------------------
function(byjy_kuaizhao_lujing out_var snapshot_root source_root inc_dirs)
    set(_mapped_dirs "")
    foreach(_dir IN LISTS inc_dirs)
        # 源根本身：换成快照根
        if(_dir STREQUAL "${source_root}")
            list(APPEND _mapped_dirs "${snapshot_root}")
        else()
            # 判断是否位于源根之下
            string(FIND "${_dir}/" "${source_root}/" _pos)
            if(_pos EQUAL 0)
                # 取源根之后的剩余部分，拼到快照根下
                string(LENGTH "${source_root}/" _source_root_len)
                string(SUBSTRING "${_dir}" ${_source_root_len} -1 _rest)
                list(APPEND _mapped_dirs "${snapshot_root}/${_rest}")
            else()
                # 非源根目录原样保留
                list(APPEND _mapped_dirs "${_dir}")
            endif()
        endif()
    endforeach()
    set(${out_var} "${_mapped_dirs}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# 头快照切换 —— 把导入目标的「下层源码树包含目录」改指该下层的构建目录头快照
# -----------------------------------------------------------------------------
# 用法
#     byjy_qiehuan_tou_kuaizhao(<导入目标名> <快照根> <源根>)
#
# 行为
#     按 byjy_kuaizhao_lujing() 的规则换算该目标的 INTERFACE_INCLUDE_DIRECTORIES，
#     其余目录（如本层自有包含目录）原样保留。
#     快照根不存在 → FATAL_ERROR，提示先构建产出该快照的下层；不静默退回源码树。
#
# 用途
#     上层据此按「与下层静态库同版次」的头文件编译，消除「新头配旧库」的编译链接风险。
#     只有需要该语义的层才调用（当前为系统层与测试层）；未调用者维持原有源码树包含目录。
# -----------------------------------------------------------------------------
function(byjy_qiehuan_tou_kuaizhao target_name snapshot_root source_root)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "头快照切换：导入目标 ${target_name} 不存在")
    endif()

    if(NOT IS_DIRECTORY "${snapshot_root}")
        message(FATAL_ERROR
            "头快照切换：找不到头快照目录\n"
            "  期望位置：${snapshot_root}\n"
            "  该目录由下层构建时导出，请先构建产出它的下层：\n"
            "      cmake --build <下层目录>/out/build/x64-Debug")
    endif()

    get_target_property(_inc_dirs ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
    byjy_kuaizhao_lujing(_new_inc_dirs "${snapshot_root}" "${source_root}" "${_inc_dirs}")

    set_target_properties(${target_name} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_new_inc_dirs}")
    message(STATUS "头快照切换：${target_name} 的包含目录改指 ${snapshot_root}")
endfunction()
