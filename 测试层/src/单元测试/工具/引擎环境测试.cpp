//引擎环境测试：覆盖可执行文件路径与目录获取、结果稳定性、绝对路径拼接两种重载
#include <gtest/gtest.h>

//获取引擎环境
#include "src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转换工具
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"

//引擎环境测试夹具
class Engine_Env_Test : public ::testing::Test
{
};

//可执行文件路径：非空
TEST_F(Engine_Env_Test, 可执行文件路径非空)
{
	//获取可执行文件路径
	EXPECT_FALSE(engine::Engine_Env::exe_path_get().empty());
}

//可执行文件路径：为绝对路径
TEST_F(Engine_Env_Test, 可执行文件路径为绝对路径)
{
	//绝对路径检查
	EXPECT_TRUE(engine::Engine_Env::exe_path_get().is_absolute());
}

//可执行文件路径：带有文件名与可执行扩展名
TEST_F(Engine_Env_Test, 可执行文件路径含文件名)
{
	//文件名不应为空
	EXPECT_FALSE(engine::Engine_Env::exe_path_get().filename().empty());
	//扩展名应为可执行文件后缀
	EXPECT_EQ(engine::path_to_string(engine::Engine_Env::exe_path_get().extension()), ".exe");
}

//可执行文件目录：为路径的父目录
TEST_F(Engine_Env_Test, 目录为路径的父目录)
{
	//目录应等于路径的父目录
	EXPECT_EQ(engine::Engine_Env::exe_dir_get(),
		engine::Engine_Env::exe_path_get().parent_path());
}

//可执行文件目录：真实存在于文件系统
TEST_F(Engine_Env_Test, 目录真实存在)
{
	//目录存在性检查
	EXPECT_TRUE(std::filesystem::exists(engine::Engine_Env::exe_dir_get()));
}

//可执行文件目录：为绝对路径
TEST_F(Engine_Env_Test, 目录为绝对路径)
{
	//绝对路径检查
	EXPECT_TRUE(engine::Engine_Env::exe_dir_get().is_absolute());
}

//结果稳定性：两次获取返回同一静态对象
TEST_F(Engine_Env_Test, 重复获取返回同一对象)
{
	//取两次路径地址
	const std::filesystem::path* first = &engine::Engine_Env::exe_path_get();
	const std::filesystem::path* second = &engine::Engine_Env::exe_path_get();
	//地址应一致
	EXPECT_EQ(first, second);
}

//绝对路径拼接：路径重载以可执行文件目录为基准
TEST_F(Engine_Env_Test, 绝对路径拼接路径重载)
{
	//相对路径
	const std::filesystem::path relative = "assets/config/test.json";
	//拼接结果应等于目录与相对路径的组合
	EXPECT_EQ(engine::Engine_Env::absolute_path_get(relative),
		engine::Engine_Env::exe_dir_get() / relative);
}

//绝对路径拼接：拼接结果为绝对路径
TEST_F(Engine_Env_Test, 绝对路径拼接结果为绝对路径)
{
	//相对路径拼接后应为绝对路径
	//字面量会引发重载歧义（C2668），故显式构造 std::string
	EXPECT_TRUE(engine::Engine_Env::absolute_path_get(std::string("assets")).is_absolute());
}

//绝对路径拼接字符串重载：中文相对路径被保留
TEST_F(Engine_Env_Test, 字符串重载保留中文)
{
	//中文相对路径（UTF-8 文本）
	const std::string relative = "资产/配置/测试.json";
	//拼接并转回文本
	const std::string joined =
		engine::path_to_string(engine::Engine_Env::absolute_path_get(relative));
	//中文成分不应丢失
	EXPECT_NE(joined.find("资产"), std::string::npos);
	//文件名部分应完整出现
	EXPECT_NE(joined.find("测试.json"), std::string::npos);
}

//绝对路径拼接字符串重载：结果以可执行文件目录开头
TEST_F(Engine_Env_Test, 字符串重载以目录为基准)
{
	//拼接待转换的相对路径
	//字面量会引发重载歧义（C2668），故显式构造 std::string
	const std::string joined =
		engine::path_to_string(engine::Engine_Env::absolute_path_get(std::string("assets/config")));
	//目录文本
	const std::string exe_dir = engine::path_to_string(engine::Engine_Env::exe_dir_get());
	//拼接结果应以目录文本开头
	EXPECT_EQ(joined.compare(0, exe_dir.size(), exe_dir), 0);
}

//两种重载一致性：同一相对路径得到相同结果
TEST_F(Engine_Env_Test, 两种重载结果一致)
{
	//ASCII 相对路径
	const std::string relative = "assets/config/test.json";
	//路径重载结果
	const std::filesystem::path by_path =
		engine::Engine_Env::absolute_path_get(std::filesystem::path(relative));
	//字符串重载结果
	const std::filesystem::path by_string = engine::Engine_Env::absolute_path_get(relative);
	//两者应一致
	EXPECT_EQ(by_path, by_string);
}