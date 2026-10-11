# =============================================================================
# 生成源文件列表.cmake —— GLOB 扫描结果 → 显式清单（target_sources 形式）
# =============================================================================
# 位置：白银纪元/Engine/EngineCore/cmake
# 用途：由各层 CMakeLists 在每轮配置时 include（五层共享的构建工具，各层按相对路径
#       引用本文件），把 file(GLOB_RECURSE) 的扫描结果同步进**各层根目录**下的显式清单
#       （源文件列表*.cmake，与调用它的 CMakeLists.txt 同级）。
# 动机：用 GLOB 收集源文件时，构建脚本里没有任何「具体文件名」。Visual Studio 的
#       文件视图改名 / 迁移操作，靠 File API codemodel 的 backtrace 找「可改写的
#       引用」；而 backtrace 只落在「把源文件直接加入 target 的那条命令」上。
#       源文件若经变量传入 add_library，那一行没有字面文件名，VS 便报
#       「引用了文件……但无法修改引用」，既不重配也不刷新模型。
#       实测结论：把字面文件名交给 target_sources，且该调用位于被 include 的
#       .cmake 中时，backtrace 落在该 .cmake 的对应行上 —— VS 能改写清单，而清单
#       是配置依赖，改写即触发重新配置。
# 关键约束：
#   1) 清单只在「路径集合有差异」时写回 —— 若每轮配置都无条件写回，清单时间戳会
#      被反复刷新，而它又是配置依赖（被 include），两者咬合会退化成「每次构建都
#      重新配置」。按路径集合（而非逐字节）判定，以免 VS 改写后的排版与脚本产出
#      略有出入时来回写盘。
#   2) 清单是这些文件「唯一」的加入点 —— 同一条文件若既出现在 add_library 的参数
#      里又出现在清单里，backtrace 归先加入者（add_library），VS 仍会报错。
#   3) 清单必须放在「本层根目录」（与调用它的 CMakeLists.txt 同级），路径一律写成
#      「相对本层根、正斜杠」。原因：CMake 解析 target_sources 的相对路径，基准是
#      「被 include 的 CMakeLists 所在目录」＝本层根；而 VS 改写引用时的基准是
#      「被编辑的 .cmake 文件自身所在目录」。两者只有在清单位于本层根时才重合 ——
#      若清单留在 cmake/ 子目录，VS 会写出 ../src/… 这类 CMake 解析不到的路径，
#      配置直接失败（Cannot find source file / No SOURCES given to target），
#      VS 随即取消生成并回滚改名。
# 对外接口：
#       byjy_source_list_sync(<清单绝对路径> <本层根> <target 名>
#                             <源列表变量名> <头列表变量名> <层显示名>)
#       「层显示名」形如 Engine/EngineSystem，用于清单头部说明与本层根提示；
#       源 / 头列表变量名取自调用方作用域（函数可直接读取调用方的变量）。
# 备注：CMake 的命令名 / 参数名只接受 ASCII，故本脚本标识符一律用英文。
# =============================================================================

# 从一段 CMake 文本中提取全部引号字符串，归一化为「相对本层根、正斜杠」形式后排序
function(byjy_list_extract_paths _text _root_dir _out_var)
    set(_paths "")
    # 清单里的路径一律带引号，取全部 "..." 字面量
    string(REGEX MATCHALL "\"([^\"]*)\"" _quoted "${_text}")
    foreach(_q IN LISTS _quoted)
        # 去掉两端引号
        string(REGEX REPLACE "^\"|\"$" "" _p "${_q}")
        # 反斜杠统一成正斜杠
        string(REPLACE "\\" "/" _p "${_p}")
        # 若为绝对路径（带盘符）则折算为相对本层根
        string(REPLACE "${_root_dir}/" "" _p "${_p}")
        # 去掉开头的 "./"
        string(REGEX REPLACE "^\\./" "" _p "${_p}")
        list(APPEND _paths "${_p}")
    endforeach()
    list(SORT _paths)
    set(${_out_var} "${_paths}" PARENT_SCOPE)
endfunction()

# 源文件清单同步：扫描结果的路径集合与清单不一致时，整体写回清单
function(byjy_source_list_sync _list_file _root_dir _target _src_var _hdr_var _layer_label)
    # 本轮扫描结果（取自调用方作用域），折算成相对本层根的路径
    set(_src_rel "")
    foreach(_item IN LISTS ${_src_var})
        # 绝对路径 → 相对本层根
        string(REPLACE "${_root_dir}/" "" _rel "${_item}")
        string(REPLACE "\\" "/" _rel "${_rel}")
        list(APPEND _src_rel "${_rel}")
    endforeach()
    set(_hdr_rel "")
    foreach(_item IN LISTS ${_hdr_var})
        # 绝对路径 → 相对本层根
        string(REPLACE "${_root_dir}/" "" _rel "${_item}")
        string(REPLACE "\\" "/" _rel "${_rel}")
        list(APPEND _hdr_rel "${_rel}")
    endforeach()

    # 组装清单全文：固定说明头 + 源段落 + 头段落
    set(_new_text "#
# =============================================================================
# ${_layer_label} 源文件列表（生成物，勿手改）
# -----------------------------------------------------------------------------
# 生成方式
#     由 Engine/EngineCore/cmake/生成源文件列表.cmake 在每轮配置时自动维护：把
#     file(GLOB_RECURSE) 的扫描结果与本文件比对（按路径集合），仅在集合有差异时
#     整体写回。
#
# 为什么要这个文件
#     CMakeLists 用 GLOB 收集源文件时，构建脚本里没有任何「具体文件名」。
#     Visual Studio 的文件视图改名 / 迁移操作，靠 File API 的 backtrace 找
#     「可改写的引用」；backtrace 只落在「把源文件直接加入 target 的那条命令」
#     上。本文件用 target_sources 直接列出文件名的字面量，于是 VS 的改写就会落到
#     本文件上；而本文件被 CMakeLists include，属配置依赖，被改写即触发重新配置。
#
# 维护
#     - 不要手工编辑。新增 / 删除 / 改名文件后，下一轮配置自动刷新。
#     - 路径一律相对本层根目录（${_layer_label}），与 VS 改写后的写法一致。
#     - 这里是这些文件的「唯一」加入点：CMakeLists 的 add_library 不再传这些源文件，
#       否则 backtrace 会归到 add_library 上，VS 就又无法改写了。
# =============================================================================
#
")
    string(APPEND _new_text "target_sources(${_target} PRIVATE\n")
    foreach(_item IN LISTS _src_rel)
        string(APPEND _new_text "    \"${_item}\"\n")
    endforeach()
    string(APPEND _new_text ")\n\n")

    string(APPEND _new_text "# 头文件不参与编译，只为让 IDE 建立文件索引；同样经 target_sources 列出，\n")
    string(APPEND _new_text "# 使头文件改名 / 迁移时 VS 也能改写引用\n")
    string(APPEND _new_text "target_sources(${_target} PRIVATE\n")
    foreach(_item IN LISTS _hdr_rel)
        string(APPEND _new_text "    \"${_item}\"\n")
    endforeach()
    string(APPEND _new_text ")\n")

    # 读入清单现有内容，提取其路径集合（文件不存在时视为空集合）
    set(_old_paths "")
    if(EXISTS "${_list_file}")
        file(READ "${_list_file}" _old_text)
        byjy_list_extract_paths("${_old_text}" "${_root_dir}" _old_paths)
    endif()

    # 归一化本轮路径集合，与清单现有集合比较
    byjy_list_extract_paths("${_new_text}" "${_root_dir}" _new_paths)

    # 路径集合有差异才写回，保证清单时间戳稳定、不触发无谓的重新配置
    if(NOT "${_old_paths}" STREQUAL "${_new_paths}")
        file(WRITE "${_list_file}" "${_new_text}")
        message(STATUS "源文件清单已刷新：${_list_file}")
    endif()
endfunction()