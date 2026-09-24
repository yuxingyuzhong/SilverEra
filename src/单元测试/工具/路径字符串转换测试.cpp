//路径字符串转换测试：覆盖 ASCII 与中文路径的双向转换、往返一致性、成分保留与拼接
#include <gtest/gtest.h>

//获取路径字符串转换工具
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"

//路径字符串转换测试夹具
class Path_String_Test : public ::testing::Test
{
};

//ASCII 路径转字符串：原样输出
TEST_F(Path_String_Test, ASCII路径转字符串)
{
	//纯 ASCII 相对路径
	std::filesystem::path path = "assets/config/test.json";
	//转换结果应与原文本一致
	EXPECT_EQ(engine::path_to_string(path), "assets/config/test.json");
}

//ASCII 字符串转路径：转换后可读回原文本
TEST_F(Path_String_Test, ASCII字符串转路径)
{
	//转换为路径对象
	std::filesystem::path path = engine::string_to_path("assets/config/test.json");
	//读回文本应与原文一致
	EXPECT_EQ(engine::path_to_string(path), "assets/config/test.json");
}

//中文字符串转路径：UTF-8 字节被完整保留
TEST_F(Path_String_Test, 中文路径往返一致)
{
	//含中文的相对路径
	const std::string origin = "资产/配置/测试.json";
	//往返转换后应与原文逐字节一致
	EXPECT_EQ(engine::path_to_string(engine::string_to_path(origin)), origin);
}

//中英混排路径：两条转换链路都不丢失字符
TEST_F(Path_String_Test, 中英混排路径往返一致)
{
	//中文与 ASCII 混排的相对路径
	const std::string origin = "关卡_01/地图_02.bin";
	//往返转换后应与原文逐字节一致
	EXPECT_EQ(engine::path_to_string(engine::string_to_path(origin)), origin);
}

//绝对路径往返一致：盘符与全部分隔符保留
TEST_F(Path_String_Test, 绝对路径往返一致)
{
	//含中文的绝对路径
	const std::string origin = "D:/代码存储/白银纪元/测试层/assets";
	//往返转换后应与原文逐字节一致
	EXPECT_EQ(engine::path_to_string(engine::string_to_path(origin)), origin);
}

//空字符串转换：不产生额外字符
TEST_F(Path_String_Test, 空字符串往返为空)
{
	//空字符串转路径再转回
	EXPECT_TRUE(engine::path_to_string(engine::string_to_path("")).empty());
}

//单个中文字符：UTF-8 编码占三字节
TEST_F(Path_String_Test, 中文字符按UTF8编码)
{
	//单个中文字的往返结果
	const std::string round_trip = engine::path_to_string(engine::string_to_path("中"));
	//UTF-8 下单个汉字占三个字节
	EXPECT_EQ(round_trip.size(), 3u);
	//内容与原文一致
	EXPECT_EQ(round_trip, "中");
}

//扩展名成分：转换后仍能解析出中文扩展名
TEST_F(Path_String_Test, 扩展名成分保留)
{
	//含中文目录与扩展名的路径
	std::filesystem::path path = engine::string_to_path("模型/贴图.png");
	//扩展名应可读回
	EXPECT_EQ(engine::path_to_string(path.extension()), ".png");
}

//文件名成分：转换后仍能解析出中文文件名
TEST_F(Path_String_Test, 文件名成分保留)
{
	//含中文文件名与主干名的路径
	std::filesystem::path path = engine::string_to_path("模型/贴图.png");
	//文件名应可读回
	EXPECT_EQ(engine::path_to_string(path.filename()), "贴图.png");
	//主干名应可读回
	EXPECT_EQ(engine::path_to_string(path.stem()), "贴图");
}

//中文路径拼接：目录与文件名在结果中同时保留
TEST_F(Path_String_Test, 中文路径拼接后双保留)
{
	//以中文目录拼接中文文件名
	std::filesystem::path joined = engine::string_to_path("测试目录") /
		engine::string_to_path("文件.txt");
	//转换结果为可读文本
	const std::string text = engine::path_to_string(joined);
	//目录名应出现在结果中
	EXPECT_NE(text.find("测试目录"), std::string::npos);
	//文件名应出现在结果中
	EXPECT_NE(text.find("文件.txt"), std::string::npos);
}

//转换结果可被文件系统接受：拼出的路径能定位真实存在的目录
TEST_F(Path_String_Test, 转换结果可用于文件系统查询)
{
	//取可执行文件目录并转为 UTF-8 文本
	const std::string exe_dir = engine::path_to_string(engine::Engine_Env::exe_dir_get());
	//该文本转回路径后应指向真实存在的目录
	EXPECT_TRUE(std::filesystem::exists(engine::string_to_path(exe_dir)));
}