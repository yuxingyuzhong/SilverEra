#include "common/前置头文件包含.h"
//获取 gtest 测试框架
#include <gtest/gtest.h>

//引擎宿主：只起引擎层组件，并把引擎层全部单元测试作为入口运行
//用于在「引擎层」单独打开时充当启动项，改引擎层源码后重编静态库并重新链接
int main(int argc, char** argv)
{
	//输出切到 UTF-8（测试报告仍用控制台输出）
	SetConsoleOutputCP(CP_UTF8);
	//输入切到 UTF-8
	SetConsoleCP(CP_UTF8);

	//初始化 gtest：解析 --gtest_filter / --gtest_list_tests 等命令行参数
	::testing::InitGoogleTest(&argc, argv);

	//运行全部用例，以其结果作为进程退出码（0 表示全部通过）
	return RUN_ALL_TESTS();
}
