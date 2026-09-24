//日志系统测试：覆盖四级输出的类型前缀与格式化内容、错误码格式化特化与文件重载可达性
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

//文件输出重载：以字符串字面量为文件名时被控制台重载截走，属已知缺陷
//缺陷位置：日志系统.h 的 info / warn / error / debug 文件输出重载
//成因：文件重载首参为 const std::string&，控制台重载首参为 std::format_string<Args...>。
//     字符串字面量到两者的转换序列都是用户定义转换且前导序列相同，无法分出优劣；
//     比较第二个参数时控制台重载恰好更优，于是整个调用被判给控制台重载。
//     若首参写成 std::string 变量，则文件重载首参为恒等匹配、控制台重载第二参更优，
//     两个候选各有一处更优参数，编译器直接报 C2666 调用不明确。
//结果：文件输出重载在两种传参形式下都不可达 —— 指定文件名不会产生任何文件，
//     文件名反而被当作格式串交给控制台重载输出。本用例固化的是这一现状。
TEST_F(Log_Test, 文件输出重载被控制台截走)
{
	//日志文件名（纯 ASCII，便于字面量传参）
	const std::string file_name = "engine_log_probe.txt";
	//删除可能存在的残留文件
	std::error_code remove_info;
	std::filesystem::remove(engine::string_to_path(file_name), remove_info);

	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//以字面量文件名调用（按当前重载决议落到控制台重载）
		engine::Log::info("engine_log_probe.txt", "内容");
		//取回输出内容
		output = capture.text();
	}

	//控制台输出里出现的是文件名本身，说明它被当成了格式串
	EXPECT_NE(output.find("engine_log_probe.txt"), std::string::npos);
	//指定文件名并未产生文件
	EXPECT_FALSE(std::filesystem::exists(engine::string_to_path(file_name)));
}
