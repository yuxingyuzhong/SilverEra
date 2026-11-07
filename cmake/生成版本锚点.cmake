# =============================================================================
# 生成版本锚点（构建期脚本，cmake -P 运行）
# -----------------------------------------------------------------------------
# 目的
#     把「对外头集合 + 构建配置」编码进一个符号名，使「头与静态库不同版次」
#     在链接期直接失败（LNK2019），而不是靠人工约定与目录结构推断。
#
# 做法
#     1) 遍历 MIRROR_DIRS（与头快照镜像范围完全一致）下的 .h/.hpp/.inl；
#        HASH_DIRS 可选：只参与哈希、不进快照镜像（如参与本层编译的第三方头）；
#     2) 按「相对路径排序 + 文件内容 + 构建类型 + 运行库口径」求 SHA256；
#     3) 取前 12 位作为版本标识，生成：
#          OUT_H   —— 本头会被导入目标以 /FI 强灌进**每个**消费 TU（其中包含纯 C 源，
#                     如渲染层就地编入的 SDL3），故 C / C++ 双兼容：
#                       纯 C TU：只留函数声明 ＋ /include 链接指令（C 无命名空间）；
#                       C++ TU：追加 inline 函数指针变量，保证每个 TU 都引用该符号；
#          OUT_CPP —— 定义该符号（.cpp 编译，extern "C"），编入本层静态库；
#     4) 生成内容与磁盘一致时不写盘，保持时间戳稳定（避免无谓重编）。
#
# 参数
#     SRC_ROOT     项目根
#     MIRROR_DIRS  头快照镜像的相对目录列表（分号分隔）
#     HASH_DIRS    可选：只用于计算哈希、不进入快照镜像的额外目录（如内嵌第三方头）
#     BUILD_TYPE   构建类型（Ninja 下为 Debug/Release，VS 下由 $<CONFIG> 传入）
#     OUT_H        生成的锚点头绝对路径
#     OUT_CPP      生成的锚点实现绝对路径
#     LAYER_TOKEN  层标识（ASCII，小写），用于符号名，如 engine_foundation
#
# 说明
#     哈希输入不含时间戳，同一份头 + 同一配置必然得到同一哈希。
#     运行库口径按项目默认映射：Debug → /MDd，其余 → /MD。
# =============================================================================

# 内容不同才写盘（保持时间戳稳定）
function(byjy_anchor_write_if_changed _path _content)
    if(EXISTS "${_path}")
        file(READ "${_path}" _old)
        # 内容一致
        if("${_old}" STREQUAL "${_content}")
            return()
        endif()
    endif()
    file(WRITE "${_path}" "${_content}")
endfunction()

# 生成锚点（供脚本模式调用）
function(byjy_anchor_generate)
    if(NOT SRC_ROOT OR NOT MIRROR_DIRS OR NOT OUT_H OR NOT OUT_CPP OR NOT LAYER_TOKEN)
        message(FATAL_ERROR "生成版本锚点：缺少参数 SRC_ROOT / MIRROR_DIRS / OUT_H / OUT_CPP / LAYER_TOKEN")
    endif()

    # 运行库口径（项目默认：Debug 用 /MDd，其余 /MD）
    if("${BUILD_TYPE}" STREQUAL "Debug")
        set(_runtime "MDd")
    else()
        set(_runtime "MD")
    endif()

    # 哈希目录：默认与镜像范围一致；HASH_DIRS 可单独扩大（不影响快照镜像）
    if(HASH_DIRS)
        set(_hash_dirs "${HASH_DIRS}")
    else()
        set(_hash_dirs "${MIRROR_DIRS}")
    endif()

    # 收集对外头（相对路径）
    set(_headers "")
    foreach(_dir IN LISTS _hash_dirs)
        foreach(_ext IN ITEMS ".h" ".hpp" ".inl")
            file(GLOB_RECURSE _found "${SRC_ROOT}/${_dir}/*${_ext}")
            foreach(_file IN LISTS _found)
                file(RELATIVE_PATH _rel "${SRC_ROOT}" "${_file}")
                list(APPEND _headers "${_rel}")
            endforeach()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _headers)
    list(SORT _headers)

    # 组装哈希输入：固定前缀 + 配置口径 + 每个头的相对路径与内容
    set(_payload "byjy-anchor-v1|config=${BUILD_TYPE}|runtime=${_runtime}")
    foreach(_rel IN LISTS _headers)
        file(READ "${SRC_ROOT}/${_rel}" _content)
        string(APPEND _payload "\n${_rel}\n${_content}")
    endforeach()

    string(SHA256 _hash "${_payload}")
    string(SUBSTRING "${_hash}" 0 12 _short)
    string(TOLOWER "${LAYER_TOKEN}" _token)
    set(_symbol "byjy_stamp_${_token}_${_short}")

    # 锚点头：声明 + 强制引用
    set(_header "// =============================================================================
// ${LAYER_TOKEN} 版本锚点（构建期生成物，勿手改）
// -----------------------------------------------------------------------------
// 哈希覆盖：对外头集合（相对路径 + 内容）、构建类型、运行库口径。
// 上层由导入目标强制包含本头；头与静态库不同版次时链接期报 LNK2019。
//   哈希：${_hash}
//   构建：${BUILD_TYPE} / ${_runtime}
// =============================================================================
#pragma once

/*
C / C++ 双兼容
本头会被导入目标以 /FI 强制包含进**每个**消费 TU，其中包含 C 源（如渲染层就地编入的 SDL3）。
故声明与强制引用必须分开：
  - 纯 C TU：只保留函数声明 ＋ /include 链接指令（C 语法无法承载命名空间）；
  - C++ TU：追加 inline 函数指针，保证每个 TU 都产生对该符号的引用。
*/
#ifdef __cplusplus
extern \"C\"
{
#endif
void ${_symbol}(void);
#ifdef __cplusplus
}

namespace byjy_anchor
{
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((used))
#endif
    inline void (*const ${_token})(void) = &${_symbol};
}
#endif

#if defined(_MSC_VER) && defined(_M_X64)
#pragma comment(linker, \"/include:${_symbol}\")
#endif
")

    # 锚点实现：定义符号
    set(_source "// =============================================================================
// ${LAYER_TOKEN} 版本锚点定义（构建期生成物，勿手改）
// 哈希：${_hash}（${BUILD_TYPE} / ${_runtime}）
// =============================================================================
extern \"C\" void ${_symbol}(void)
{
}
")

    byjy_anchor_write_if_changed("${OUT_H}" "${_header}")
    byjy_anchor_write_if_changed("${OUT_CPP}" "${_source}")

    list(LENGTH _headers _header_count)
    message(STATUS "版本锚点：${LAYER_TOKEN} → ${_symbol}（头 ${_header_count} 个）")
endfunction()

# 脚本模式：直接生成
if(CMAKE_SCRIPT_MODE_FILE)
    byjy_anchor_generate()
endif()
