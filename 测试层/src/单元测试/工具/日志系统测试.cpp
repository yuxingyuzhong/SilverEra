//日志系统测试：覆盖四级输出的类型前缀与格式化内容、错误码格式化特化与活跃输出流控制
#include <gtest/gtest.h>

//获取日志系统
#include "src/tools/Logging/日志系统.h"
//获取路径字符串转换工具
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"

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
	std::filesystem::remove(engine::string_to_path(file_name), remove_info);

	//设置活跃输出流为该文件
	engine::Log log;
	log.stream_set(file_name);
	//此时输出一条信息日志
	engine::Log::info("落盘探针 {}", 7);

	//文件应被创建
	EXPECT_TRUE(std::filesystem::exists(engine::string_to_path(file_name)));

	//读回文件内容
	std::ifstream reader(engine::string_to_path(file_name));
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
	std::filesystem::remove(engine::string_to_path(file_name), remove_info);
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
