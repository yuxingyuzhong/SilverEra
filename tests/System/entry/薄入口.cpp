//薄入口 —— TestSystem.exe 的 main
//============================================================================
// 位置：Application/Test/tests/System/entry/薄入口.cpp
// 职责：只跑 EngineSystem 一层的用例，不打开图形选择器。
//============================================================================
#include <gtest/gtest.h>

#include <windows.h>

//测试注册锚：由 TestSystem.lib 提供
extern "C" void byjy_test_anchor_system();

int main(int argc, char** argv)
{
    //控制台切 UTF-8，保证中文用例名可读
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);

    //交给 googletest 解析参数
    ::testing::InitGoogleTest(&argc, argv);

    //强制引用注册锚：测试库未被整体收编时在这里链接失败
    byjy_test_anchor_system();

    //执行本层全部用例
    return RUN_ALL_TESTS();
}
