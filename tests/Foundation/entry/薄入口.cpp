//薄入口 —— TestFoundation.exe 的 main
//============================================================================
// 位置：Application/Test/tests/Foundation/entry/薄入口.cpp
// 职责：只跑 EngineFoundation 一层的用例。改 Foundation 的人不必等 System /
//       Renderer 的测试库编过，也不必打开图形选择器。
// 说明：不链接 gtest_main，注册锚必须在 RUN_ALL_TESTS 前调用（漏链即链接失败）。
//============================================================================
#include <gtest/gtest.h>

#include <windows.h>

//测试注册锚：由 TestFoundation.lib 提供
extern "C" void byjy_test_anchor_foundation();

int main(int argc, char** argv)
{
    //控制台切 UTF-8，保证中文用例名可读
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);

    //交给 googletest 解析参数（必须在枚举用例前完成）
    ::testing::InitGoogleTest(&argc, argv);

    //强制引用注册锚：测试库未被整体收编时在这里链接失败
    byjy_test_anchor_foundation();

    //执行本层全部用例
    return RUN_ALL_TESTS();
}
