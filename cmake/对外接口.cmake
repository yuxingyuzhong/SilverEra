# =============================================================================
# 测试层 —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：测试层/cmake/对外接口.cmake
#
# 作用
#     声明「本层向上层提供什么」。上层通过本文件读取本层对外面，据此建立
#     IMPORTED 静态库目标（不使用 add_subdirectory 回退编源码）。
#
# 契约（变量名与含义是层间契约的一部分，改名必须同步上层）
#     BYJY_<P>_OUT_LIB    本层静态库文件名（不含扩展名）；叶层无库时留空
#     BYJY_<P>_OUT_INC    使用本层公共头文件所需的包含目录
#     BYJY_<P>_OUT_DEF    使用本层公共头文件所需的编译定义
#     BYJY_<P>_OUT_LINK   本层对外传递的第三方库（文件路径）
#     BYJY_<P>_OUT_SYS    本层对外传递的系统库
#     本层前缀 <P> = TEST
#
# 并入下层
#     测试层依赖系统层，因此本层对外面 = 本层自有面 + 系统层对外面。
#     并入方式就是下面这一句 include：它把系统层的 BYJY_SYSTEM_OUT_* 赋值进来
#     （系统层又并入了引擎层的对外面），本层再在自己的列表里直接引用。
#     纯数据赋值，被重复 include 无副作用。
# =============================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../系统层/cmake/对外接口.cmake")

get_filename_component(BYJY_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_TEST_OUT_LIB "TestCore")

# 本层自有包含目录
#     根目录：用例以 "src/core/..." 形式引用被测头文件
# 之后接系统层对外面：本层用例引用的引擎层头文件由它一并带出
set(BYJY_TEST_OUT_INC
    "${BYJY_TEST_ROOT}"
    ${BYJY_SYSTEM_OUT_INC}
)

set(BYJY_TEST_OUT_DEF  ${BYJY_SYSTEM_OUT_DEF})
set(BYJY_TEST_OUT_LINK ${BYJY_SYSTEM_OUT_LINK})
set(BYJY_TEST_OUT_SYS  ${BYJY_SYSTEM_OUT_SYS})

# 备注：TestCore 只在 src/ 下有源文件时才产出（当前 src/ 为空，尚未产出库），
#       此时若真有上层来链接它，会自动探测不到并硬失败 —— 属于预期行为。
#       EngineTests.exe 不写进对外面：它是本层的运行产物，不是供上层链接的东西。
