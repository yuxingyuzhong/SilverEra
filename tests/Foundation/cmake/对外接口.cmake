# =============================================================================
# tests/Foundation —— 对外接口（对外自描述）
# -----------------------------------------------------------------------------
# 位置：Application/Test/tests/Foundation/cmake/对外接口.cmake
#
# 作用
#     声明「本测试子工程产出什么」，供主测试层（Application/Test）按统一契约
#     接入它的静态库（文件名 / 位置 / 包含根），不依赖任何硬编码路径。
#
# 契约（与各引擎层的对外接口同名同义）
#     BYJY_TEST_FOUNDATION_OUT_LIB     TestFoundation
#     BYJY_TEST_FOUNDATION_OUT_LIBDIR  本子工程根（据此探测 out/build/x64-<配置>/lib）
#     BYJY_TEST_FOUNDATION_OUT_INC     Test 层根（用例头以 Application/Test/... 全路径引用时用）
#     BYJY_TEST_FOUNDATION_OUT_DEF     空
#     BYJY_TEST_FOUNDATION_OUT_LINK    空
#     BYJY_TEST_FOUNDATION_OUT_SYS     空
#
# 注意
#     本子工程的静态库里是用例与注册锚，编译期依赖 EngineFoundation 头快照与 gtest；
#     这些依赖由子工程自己的构建脚本闭合，主层接入时只需「整体收编整个库」
#     （WHOLE_ARCHIVE），不需要再重复声明引擎库或运行库。
# =============================================================================

get_filename_component(BYJY_TEST_FOUNDATION_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(BYJY_TEST_FOUNDATION_OUT_LIB "TestFoundation")
set(BYJY_TEST_FOUNDATION_OUT_LIBDIR "${BYJY_TEST_FOUNDATION_ROOT}")
set(BYJY_TEST_FOUNDATION_OUT_INC "${BYJY_TEST_FOUNDATION_ROOT}/../..")
set(BYJY_TEST_FOUNDATION_OUT_DEF "")
set(BYJY_TEST_FOUNDATION_OUT_LINK "")
set(BYJY_TEST_FOUNDATION_OUT_SYS "")
