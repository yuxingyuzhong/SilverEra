//路径字符串转换测试：覆盖 ASCII 与中文路径的双向转换、往返一致性、成分保留与拼接，
//以及路径规范化与路径键规范化（锚点裁剪、分隔符统一、点成分消解、尾分隔符补全）
#include <gtest/gtest.h>

//获取路径操作工具（路径字符串转换 + 路径规范化 + 路径键规范化）
#include "src/tools/Detail/package/路径操作工具.h"
//获取引擎环境
#include "src/tools/Engine_Env/引擎环境.h"

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
	EXPECT_EQ(engine::detail::path_to_string(path), "assets/config/test.json");
}

//ASCII 字符串转路径：转换后可读回原文本
TEST_F(Path_String_Test, ASCII字符串转路径)
{
	//转换为路径对象
	std::filesystem::path path = engine::detail::string_to_path("assets/config/test.json");
	//读回文本应与原文一致
	EXPECT_EQ(engine::detail::path_to_string(path), "assets/config/test.json");
}

//中文字符串转路径：UTF-8 字节被完整保留
TEST_F(Path_String_Test, 中文路径往返一致)
{
	//含中文的相对路径
	const std::string origin = "资产/配置/测试.json";
	//往返转换后应与原文逐字节一致
	EXPECT_EQ(engine::detail::path_to_string(engine::detail::string_to_path(origin)), origin);
}

//中英混排路径：两条转换链路都不丢失字符
TEST_F(Path_String_Test, 中英混排路径往返一致)
{
	//中文与 ASCII 混排的相对路径
	const std::string origin = "关卡_01/地图_02.bin";
	//往返转换后应与原文逐字节一致
	EXPECT_EQ(engine::detail::path_to_string(engine::detail::string_to_path(origin)), origin);
}

//绝对路径往返一致：盘符与全部分隔符保留
TEST_F(Path_String_Test, 绝对路径往返一致)
{
	//含中文的绝对路径
	const std::string origin = "D:/代码存储/白银纪元/测试层/assets";
	//往返转换后应与原文逐字节一致
	EXPECT_EQ(engine::detail::path_to_string(engine::detail::string_to_path(origin)), origin);
}

//空字符串转换：不产生额外字符
TEST_F(Path_String_Test, 空字符串往返为空)
{
	//空字符串转路径再转回
	EXPECT_TRUE(engine::detail::path_to_string(engine::detail::string_to_path("")).empty());
}

//单个中文字符：UTF-8 编码占三字节
TEST_F(Path_String_Test, 中文字符按UTF8编码)
{
	//单个中文字的往返结果
	const std::string round_trip = engine::detail::path_to_string(engine::detail::string_to_path("中"));
	//UTF-8 下单个汉字占三个字节
	EXPECT_EQ(round_trip.size(), 3u);
	//内容与原文一致
	EXPECT_EQ(round_trip, "中");
}

//扩展名成分：转换后仍能解析出中文扩展名
TEST_F(Path_String_Test, 扩展名成分保留)
{
	//含中文目录与扩展名的路径
	std::filesystem::path path = engine::detail::string_to_path("模型/贴图.png");
	//扩展名应可读回
	EXPECT_EQ(engine::detail::path_to_string(path.extension()), ".png");
}

//文件名成分：转换后仍能解析出中文文件名
TEST_F(Path_String_Test, 文件名成分保留)
{
	//含中文文件名与主干名的路径
	std::filesystem::path path = engine::detail::string_to_path("模型/贴图.png");
	//文件名应可读回
	EXPECT_EQ(engine::detail::path_to_string(path.filename()), "贴图.png");
	//主干名应可读回
	EXPECT_EQ(engine::detail::path_to_string(path.stem()), "贴图");
}

//中文路径拼接：目录与文件名在结果中同时保留
TEST_F(Path_String_Test, 中文路径拼接后双保留)
{
	//以中文目录拼接中文文件名
	std::filesystem::path joined = engine::detail::string_to_path("测试目录") /
		engine::detail::string_to_path("文件.txt");
	//转换结果为可读文本
	const std::string text = engine::detail::path_to_string(joined);
	//目录名应出现在结果中
	EXPECT_NE(text.find("测试目录"), std::string::npos);
	//文件名应出现在结果中
	EXPECT_NE(text.find("文件.txt"), std::string::npos);
}

//转换结果可被文件系统接受：拼出的路径能定位真实存在的目录
TEST_F(Path_String_Test, 转换结果可用于文件系统查询)
{
	//取可执行文件目录并转为 UTF-8 文本
	const std::string exe_dir = engine::detail::path_to_string(engine::Engine_Env::exe_dir_get());
	//该文本转回路径后应指向真实存在的目录
	EXPECT_TRUE(std::filesystem::exists(engine::detail::string_to_path(exe_dir)));
}

//路径规范化：正反斜杠书写的同一路径被统一
TEST_F(Path_String_Test, 路径规范化统一分隔符)
{
	//正斜杠书写的路径
	const std::filesystem::path slash = engine::detail::string_to_path("assets/config/test.json");
	//反斜杠书写的同一路径
	const std::filesystem::path backslash = engine::detail::string_to_path("assets\\config\\test.json");
	//规范化后两者应相等
	EXPECT_EQ(engine::detail::path_normalize(slash), engine::detail::path_normalize(backslash));
}

//路径规范化：尾部分隔符被消除
TEST_F(Path_String_Test, 路径规范化消除尾部分隔符)
{
	//带尾部分隔符的路径
	const std::filesystem::path trailing = engine::detail::string_to_path("assets/config/");
	//不带尾部分隔符的同一路径
	const std::filesystem::path plain = engine::detail::string_to_path("assets/config");
	//规范化后两者应相等
	EXPECT_EQ(engine::detail::path_normalize(trailing), engine::detail::path_normalize(plain));
}

//路径规范化：点成分被消解
TEST_F(Path_String_Test, 路径规范化消解点成分)
{
	//含点成分的冗余路径
	const std::filesystem::path messy =
		engine::detail::string_to_path("assets/./config/../config/test.json");
	//等价简洁路径
	const std::filesystem::path clean = engine::detail::string_to_path("assets/config/test.json");
	//规范化后两者应相等
	EXPECT_EQ(engine::detail::path_normalize(messy), engine::detail::path_normalize(clean));
}

//路径规范化：中文路径成分不丢失
TEST_F(Path_String_Test, 路径规范化保留中文成分)
{
	//中文路径
	const std::filesystem::path chinese = engine::detail::string_to_path("资产/配置/测试.json");
	//规范化后文件名成分应保留
	EXPECT_EQ(engine::detail::path_to_string(engine::detail::path_normalize(chinese).filename()),
		"测试.json");
}

//路径规范化：空路径保持为空
TEST_F(Path_String_Test, 路径规范化空路径为空)
{
	//空路径规范化后仍应为空
	EXPECT_TRUE(engine::detail::path_normalize(std::filesystem::path{}).empty());
}

//路径键规范化：项目根锚点及其之前的部分被裁剪，末尾补 '/'（树键恒以 '/' 结尾）
TEST_F(Path_String_Test, 路径键规范化裁剪锚点并补尾斜杠)
{
	//含项目根锚点的绝对调用处路径
	const std::string raw = "D:\\代码存储\\代码仓库\\白银纪元\\引擎层\\src\\a.cpp";
	//裁剪后应只剩项目内相对路径且末尾补 '/'
	EXPECT_EQ(engine::detail::path_key_normalize(raw, "白银纪元/"), "引擎层/src/a.cpp/");
}

//路径键规范化：正反斜杠书写的同一路径得到同一键
TEST_F(Path_String_Test, 路径键规范化统一分隔符)
{
	//正斜杠书写的绝对路径
	const std::string slash = "D:/代码存储/代码仓库/白银纪元/引擎层/src/a.cpp";
	//反斜杠书写的同一路径
	const std::string backslash = "D:\\代码存储\\代码仓库\\白银纪元\\引擎层\\src\\a.cpp";
	//两者应得到同一路径键
	EXPECT_EQ(engine::detail::path_key_normalize(slash, "白银纪元/"),
		engine::detail::path_key_normalize(backslash, "白银纪元/"));
}

//路径键规范化：不含锚点时保留原相对路径并补尾斜杠
TEST_F(Path_String_Test, 路径键规范化无锚点补尾斜杠)
{
	//不含项目根锚点的相对路径
	EXPECT_EQ(engine::detail::path_key_normalize("assets/config", "白银纪元/"), "assets/config/");
}

//路径键规范化：点成分被消解后再裁剪锚点
TEST_F(Path_String_Test, 路径键规范化消解点成分)
{
	//含 '.' 与 '..' 的冗余路径
	const std::string messy = "白银纪元/引擎层/./src/../src/a.cpp";
	//消解后应与简洁路径同键
	EXPECT_EQ(engine::detail::path_key_normalize(messy, "白银纪元/"), "引擎层/src/a.cpp/");
}

//路径键规范化：已带尾分隔符时不重复补
TEST_F(Path_String_Test, 路径键规范化尾斜杠不重复)
{
	//已带尾分隔符的目录路径
	EXPECT_EQ(engine::detail::path_key_normalize("白银纪元/引擎层/", "白银纪元/"), "引擎层/");
}

//路径键规范化：目录键是其后代文件键的前缀（保证前缀比较落在路径段边界）
TEST_F(Path_String_Test, 路径键规范化目录键为文件键前缀)
{
	//目录键与文件键
	const std::string directory = engine::detail::path_key_normalize("白银纪元/引擎层/src", "白银纪元/");
	const std::string file = engine::detail::path_key_normalize("白银纪元/引擎层/src/a.cpp", "白银纪元/");
	//目录键应完整落在文件键开头
	EXPECT_EQ(file.rfind(directory, 0), 0u);
}

//路径键规范化：空路径保持为空（不产生多余 '/'）
TEST_F(Path_String_Test, 路径键规范化空路径为空)
{
	//空路径规范化后仍应为空
	EXPECT_TRUE(engine::detail::path_key_normalize("", "白银纪元/").empty());
}