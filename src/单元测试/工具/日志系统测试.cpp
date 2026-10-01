//日志系统测试：覆盖四级输出的类型前缀与格式化内容、错误码格式化特化与活跃输出流控制
#include <gtest/gtest.h>

//获取日志系统
#include "src/tools/Logging/日志系统.h"
//获取路径字符串转换工具
#include "src/tools/Detail/路径字符串转换.h"

//控制台输出捕获器
//不用 gtest 的 CaptureStdout：它在临时目录建 .tmp 文件，本机临时目录含中文，
//窄字符 fopen 解析失败会直接 FATAL 打断整个用例进程。
//日志系统写的是 std::cout 对象本身，替换其缓冲区即可完整截获。
class Console_Capture
{
private:
	//捕获缓冲
	std::ostringstream buffer;
	//原输出缓冲区
	std::streambuf* origin = nullptr;
public:
	//开始捕获
	Console_Capture()
	{
		//接管标准输出缓冲区
		origin = std::cout.rdbuf(buffer.rdbuf());
	}
	//结束捕获并还原缓冲区
	~Console_Capture()
	{
		//还原本来的输出缓冲区
		std::cout.rdbuf(origin);
	}
	//取回捕获内容
	std::string text(void) const
	{
		return buffer.str();
	}
};

//日志系统测试夹具
class Log_Test : public ::testing::Test
{
};

//信息输出：带 INFO 前缀并完成格式化
TEST_F(Log_Test, 信息输出带类型前缀)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条信息日志
		engine::Log::info("数值={}", 42);
		//取回输出内容
		output = capture.text();
	}
	//类型前缀应存在
	EXPECT_NE(output.find("[INFO]"), std::string::npos);
	//格式化结果应存在
	EXPECT_NE(output.find("数值=42"), std::string::npos);
}

//警告输出：带 WARN 前缀
TEST_F(Log_Test, 警告输出带类型前缀)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条警告日志
		engine::Log::warn("资源缺失：{}", "贴图");
		//取回输出内容
		output = capture.text();
	}
	//类型前缀应存在
	EXPECT_NE(output.find("[WARN]"), std::string::npos);
	//格式化结果应存在
	EXPECT_NE(output.find("资源缺失：贴图"), std::string::npos);
}

//错误输出：带 ERROR 前缀
TEST_F(Log_Test, 错误输出带类型前缀)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条错误日志
		engine::Log::error("初始化失败：{}", 3);
		//取回输出内容
		output = capture.text();
	}
	//类型前缀应存在
	EXPECT_NE(output.find("[ERROR]"), std::string::npos);
	//格式化结果应存在
	EXPECT_NE(output.find("初始化失败：3"), std::string::npos);
}

//调试输出：带 DEBUG 前缀
TEST_F(Log_Test, 调试输出带类型前缀)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条调试日志
		engine::Log::debug("帧耗时：{}", 16.6);
		//取回输出内容
		output = capture.text();
	}
	//类型前缀应存在
	EXPECT_NE(output.find("[DEBUG]"), std::string::npos);
	//格式化结果应存在
	EXPECT_NE(output.find("帧耗时：16.6"), std::string::npos);
}

//多参数格式化：多个占位符依次填入
TEST_F(Log_Test, 多参数格式化)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出含两个占位符的日志
		engine::Log::info("{}与{}", "甲", 5);
		//取回输出内容
		output = capture.text();
	}
	//两个参数应分别填位
	EXPECT_NE(output.find("甲与5"), std::string::npos);
}

//无占位符输出：原文照录
TEST_F(Log_Test, 无占位符输出原文)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出不含占位符的日志
		engine::Log::info("启动完成");
		//取回输出内容
		output = capture.text();
	}
	//原文应完整出现
	EXPECT_NE(output.find("[INFO]启动完成"), std::string::npos);
}

//错误码格式化：走 std::formatter<std::error_code> 特化
TEST_F(Log_Test, 错误码格式化特化)
{
	//构造标准错误码
	const std::error_code error_info = std::make_error_code(std::errc::invalid_argument);
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//以错误码为参数输出日志
		engine::Log::info("系统调用失败：{}", error_info);
		//取回输出内容
		output = capture.text();
	}
	//错误码数值应被输出
	EXPECT_NE(output.find(std::to_string(error_info.value())), std::string::npos);
	//错误码文本应被输出
	EXPECT_NE(output.find(error_info.message()), std::string::npos);
}

//活跃输出流：stream_set 指定文件后，日志写入该文件
//修复后语义：文件输出重载已整体删除，改由 Log::stream_set(文件名) 指定活跃输出流；
//          文件名留空时回落到控制台。
TEST_F(Log_Test, 设置活跃流后日志落文件)
{
	//日志文件名（纯 ASCII，避开编码问题）
	const std::string file_name = "engine_log_probe.txt";
	//删除可能存在的残留文件
	std::error_code remove_info;
	std::filesystem::remove(engine::detail::string_to_path(file_name), remove_info);

	//设置活跃输出流为该文件
	engine::Log log;
	log.stream_set(file_name);
	//此时输出一条信息日志
	engine::Log::info("落盘探针 {}", 7);

	//文件应被创建
	EXPECT_TRUE(std::filesystem::exists(engine::detail::string_to_path(file_name)));

	//读回文件内容
	std::ifstream reader(engine::detail::string_to_path(file_name));
	std::string content;
	std::string line;
	while (std::getline(reader, line))
		content += line;
	reader.close();
	//类型前缀应落到文件
	EXPECT_NE(content.find("[INFO]"), std::string::npos);
	//格式化结果应落到文件
	EXPECT_NE(content.find("落盘探针 7"), std::string::npos);

	//还原活跃输出流为控制台，避免影响其它用例
	log.stream_set("");
	//清理探针文件
	std::filesystem::remove(engine::detail::string_to_path(file_name), remove_info);
}

//活跃输出流：未设置时日志走控制台
TEST_F(Log_Test, 未设置活跃流时走控制台)
{
	//显式把活跃输出流置空（控制台）
	engine::Log log;
	log.stream_set("");

	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条信息日志
		engine::Log::info("控制台落点 {}", 1);
		//取回输出内容
		output = capture.text();
	}
	//类型前缀与格式化结果都应出现在控制台
	EXPECT_NE(output.find("[INFO]控制台落点 1"), std::string::npos);
}

//四级日志连续写出：各等级依次输出且前缀完整
TEST_F(Log_Test, 四级日志连续写出)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//依次输出四个等级的日志
		engine::Log::info("信息 {}", 1);
		engine::Log::warn("警告 {}", 2);
		engine::Log::error("错误 {}", 3);
		engine::Log::debug("调试 {}", 4);
		//取回输出内容
		output = capture.text();
	}
	//信息等级前缀与内容应存在
	EXPECT_NE(output.find("[INFO]信息 1"), std::string::npos);
	//警告等级前缀与内容应存在
	EXPECT_NE(output.find("[WARN]警告 2"), std::string::npos);
	//错误等级前缀与内容应存在
	EXPECT_NE(output.find("[ERROR]错误 3"), std::string::npos);
	//调试等级前缀与内容应存在
	EXPECT_NE(output.find("[DEBUG]调试 4"), std::string::npos);
}

//混合类型参数替换：整数、浮点与文本依次填位
TEST_F(Log_Test, 混合类型参数替换)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出含三种类型占位符的日志
		engine::Log::info("整数{}浮点{}文本{}", 7, 2.5, std::string("甲"));
		//取回输出内容
		output = capture.text();
	}
	//各类型参数应依次替换占位符
	EXPECT_NE(output.find("整数7浮点2.5文本甲"), std::string::npos);
}

//空字符串消息：可写出且不崩溃
TEST_F(Log_Test, 空字符串消息可写出)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条空消息
		engine::Log::info("");
		//取回输出内容
		output = capture.text();
	}
	//空消息仍应带类型前缀
	EXPECT_NE(output.find("[INFO]"), std::string::npos);
}

//含中文消息：中文明文完整输出
TEST_F(Log_Test, 含中文消息完整输出)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条中文消息
		engine::Log::info("中文消息：{}", "测试");
		//取回输出内容
		output = capture.text();
	}
	//中文明文应完整出现
	EXPECT_NE(output.find("中文消息：测试"), std::string::npos);
}

//超长消息：八千字符长文本可完整输出
TEST_F(Log_Test, 超长消息可完整输出)
{
	//构造八千字符的超长文本
	const std::string long_text(8000, 'x');
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//以超长文本为参数输出日志
		engine::Log::info("{}", long_text);
		//取回输出内容
		output = capture.text();
	}
	//类型前缀应存在
	EXPECT_NE(output.find("[INFO]"), std::string::npos);
	//超长正文应被完整写出
	EXPECT_NE(output.find(long_text), std::string::npos);
}

//连续大量写出：高频输出下首末条日志均应存在
TEST_F(Log_Test, 连续大量写出稳定)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//连续写出两千条日志
		for (int index = 0; index < 2000; ++index)
			engine::Log::info("批量日志 {}", index);
		//取回输出内容
		output = capture.text();
	}
	//首条批日志应存在
	EXPECT_NE(output.find("批量日志 0"), std::string::npos);
	//末条批日志应存在
	EXPECT_NE(output.find("批量日志 1999"), std::string::npos);
}
