//日志系统测试：覆盖四级输出的类型前缀与格式化内容、错误码格式化特化，
//以及链表树流分流的路径节点绑定、目录继承、子节点覆盖、多流全量输出与清空回落
//注意：主程序会把引擎日志屏蔽到文件，本套件要验证「未绑定即走控制台」与屏幕捕获，
//故夹具在用例前关闭屏蔽、用例后恢复屏蔽（见 src/主调/引擎日志屏蔽.h）
#include <gtest/gtest.h>

//获取日志系统
#include "src/tools/Logging/日志系统运行包.h"
//获取路径字符串转换工具
#include "src/tools/Detail/路径字符串转换.h"
//获取引擎日志屏蔽（用例期间需临时关闭）
#include "src/主调/引擎日志屏蔽.h"

//本测试文件路径（current() 位于本文件，故返回本文件路径，与日志调用处路径一致）
static std::string log_test_path()
{
	//取本文件路径
	return std::source_location::current().file_name();
}
//本测试文件所在目录（兼容 '/' 与 '\\' 两种分隔符）
static std::string log_test_dir()
{
	//取本文件路径
	const std::string file = log_test_path();
	//取最后一级分隔符位置
	const size_t pos = file.find_last_of("/\\");
	//返回目录部分
	return pos == std::string::npos ? file : file.substr(0, pos);
}
//读回日志文件全文（仅拼接各行内容，便于文本查找）
static std::string log_read(const std::string& file_name)
{
	//先把日志系统缓冲落盘（文件输出目标按阈值批量落盘，读取前需显式刷新）
	engine::logger.stream_flush();
	//以字符串路径打开文件（兼容中文路径）
	std::ifstream reader(engine::detail::string_to_path(file_name));
	//拼接各行内容
	std::string content;
	std::string line;
	while (std::getline(reader, line))
		content += line;
	//返回文件全文
	return content;
}
//清理探针日志文件
static void log_remove(const std::string& file_name)
{
	//删除残留探针文件
	std::error_code remove_info;
	std::filesystem::remove(engine::detail::string_to_path(file_name), remove_info);
}
//全部探针日志文件名（用例前后统一清理，避免残留文件）
static const char* const log_probe_files[] =
{
	"engine_log_probe.txt",
	"engine_log_dir_probe.txt",
	"engine_log_parent.txt",
	"engine_log_child.txt",
	"engine_log_fall_parent.txt",
	"engine_log_fall_child.txt",
	"engine_log_multi_one.txt",
	"engine_log_multi_two.txt",
	"engine_log_merge_child.txt",
	"engine_log_merge_parent.txt",
	"engine_log_root_probe.txt",
};
//清理全部探针日志文件（须先清除绑定以释放文件流，否则 Windows 下删除失败）
static void log_remove_all()
{
	//逐个删除探针文件
	for (const char* file_name : log_probe_files)
		log_remove(file_name);
}

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
//用例前后清理本文件路径及其所在目录的既有绑定，避免用例之间互相干扰
class Log_Test : public ::testing::Test
{
protected:
	//用例前清理本文件路径节点与本文件目录节点的自有流
	void SetUp() override
	{
		//关闭引擎日志屏蔽：本套件要验证「未绑定 → 走控制台」的兜底行为
		engine::日志屏蔽_关闭();
		//清除本文件路径节点自有流
		engine::logger.stream_clear(log_test_path());
		//清除本文件目录节点自有流
		engine::logger.stream_clear(log_test_dir());
		//清理上次运行可能残留的探针文件
		log_remove_all();
	}
	//用例后清理本文件路径节点、目录节点与根节点的自有流
	void TearDown() override
	{
		//清除本文件路径节点自有流
		engine::logger.stream_clear(log_test_path());
		//清除本文件目录节点自有流
		engine::logger.stream_clear(log_test_dir());
		//清除根节点自有流
		engine::logger.stream_clear("");
		//绑定清除后文件流已释放，此时清理探针文件
		log_remove_all();
		//恢复引擎日志屏蔽：其余套件不应把引擎日志混进 googletest 的输出
		engine::日志屏蔽_开启();
	}
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
		engine::logger.info("数值={}", 42);
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
		engine::logger.warn("资源缺失：{}", "贴图");
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
		engine::logger.error("初始化失败：{}", 3);
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
		engine::logger.debug("帧耗时：{}", 16.6);
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
		engine::logger.info("{}与{}", "甲", 5);
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
		engine::logger.info("启动完成");
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
		engine::logger.info("系统调用失败：{}", error_info);
		//取回输出内容
		output = capture.text();
	}
	//错误码数值应被输出
	EXPECT_NE(output.find(std::to_string(error_info.value())), std::string::npos);
	//错误码文本应被输出
	EXPECT_NE(output.find(error_info.message()), std::string::npos);
}

//路径绑定：把本文件路径节点绑定到文件后，本文件调用处的日志写入该文件
TEST_F(Log_Test, 文件节点绑定后日志落文件)
{
	//日志文件名（纯 ASCII，避开编码问题）
	const std::string file_name = "engine_log_probe.txt";

	//把本文件路径节点绑定到该文件
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), file_name));
	//此时输出一条信息日志
	engine::logger.info("落盘探针 {}", 7);

	//文件应被创建
	ASSERT_TRUE(std::filesystem::exists(engine::detail::string_to_path(file_name)));

	//读回文件内容
	const std::string content = log_read(file_name);
	//类型前缀应落到文件
	EXPECT_NE(content.find("[INFO]"), std::string::npos);
	//格式化结果应落到文件
	EXPECT_NE(content.find("落盘探针 7"), std::string::npos);
}

//无绑定：未绑定任何路径节点时日志走控制台
TEST_F(Log_Test, 未绑定路径时走控制台)
{
	//捕获内容
	std::string output;
	{
		//开始捕获控制台输出
		Console_Capture capture;
		//输出一条信息日志
		engine::logger.info("控制台落点 {}", 1);
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
		engine::logger.info("信息 {}", 1);
		engine::logger.warn("警告 {}", 2);
		engine::logger.error("错误 {}", 3);
		engine::logger.debug("调试 {}", 4);
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
		engine::logger.info("整数{}浮点{}文本{}", 7, 2.5, std::string("甲"));
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
		engine::logger.info("");
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
		engine::logger.info("中文消息：{}", "测试");
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
		engine::logger.info("{}", long_text);
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
			engine::logger.info("批量日志 {}", index);
		//取回输出内容
		output = capture.text();
	}
	//首条批日志应存在
	EXPECT_NE(output.find("批量日志 0"), std::string::npos);
	//末条批日志应存在
	EXPECT_NE(output.find("批量日志 1999"), std::string::npos);
}

//目录节点继承：目录节点绑定输出流后，其后代文件调用处共享该输出流
TEST_F(Log_Test, 目录节点绑定后后代继承)
{
	//日志文件名（纯 ASCII，避开编码问题）
	const std::string file_name = "engine_log_dir_probe.txt";

	//把本文件所在目录节点绑定到该文件
	ASSERT_TRUE(engine::logger.stream_bind(log_test_dir(), file_name));
	//此时输出一条信息日志
	engine::logger.info("目录继承 {}", 1);

	//日志应经目录节点继承落到文件
	const std::string content = log_read(file_name);
	EXPECT_NE(content.find("[INFO]目录继承 1"), std::string::npos);
}

//子节点覆盖：文件节点自有输出流覆盖目录节点继承输出流
TEST_F(Log_Test, 子节点自有流覆盖父节点流)
{
	//目录流文件名与文件流文件名
	const std::string parent_name = "engine_log_parent.txt";
	const std::string child_name = "engine_log_child.txt";
	//先绑定目录节点，再绑定文件节点
	ASSERT_TRUE(engine::logger.stream_bind(log_test_dir(), parent_name));
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), child_name));
	//此时输出一条信息日志
	engine::logger.info("子节点覆盖 {}", 2);

	//日志应落到文件节点自有输出流
	EXPECT_NE(log_read(child_name).find("[INFO]子节点覆盖 2"), std::string::npos);
	//日志不应落到目录节点输出流
	EXPECT_EQ(log_read(parent_name).find("子节点覆盖"), std::string::npos);
}

//清空回落：清空文件节点自有输出流后回落共享目录节点输出流
TEST_F(Log_Test, 清空子节点后回落父节点流)
{
	//目录流文件名与文件流文件名
	const std::string parent_name = "engine_log_fall_parent.txt";
	const std::string child_name = "engine_log_fall_child.txt";
	//绑定目录节点与文件节点
	ASSERT_TRUE(engine::logger.stream_bind(log_test_dir(), parent_name));
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), child_name));
	//清空文件节点自有输出流
	ASSERT_TRUE(engine::logger.stream_clear(log_test_path()));
	//此时输出一条信息日志
	engine::logger.info("回落父流 {}", 3);

	//日志应回落到目录节点输出流
	EXPECT_NE(log_read(parent_name).find("[INFO]回落父流 3"), std::string::npos);
	//文件节点输出流不应再收到日志
	EXPECT_EQ(log_read(child_name).find("回落父流"), std::string::npos);
}

//多流全量输出：同一节点绑定多个输出流时日志写入全部输出流
TEST_F(Log_Test, 同节点多流全量输出)
{
	//两个日志文件名
	const std::string first_name = "engine_log_multi_one.txt";
	const std::string second_name = "engine_log_multi_two.txt";
	//对同一路径节点连续绑定两个输出流
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), first_name));
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), second_name));
	//此时输出一条信息日志
	engine::logger.info("多流输出 {}", 4);

	//两个文件都应收到同一条日志
	EXPECT_NE(log_read(first_name).find("[INFO]多流输出 4"), std::string::npos);
	EXPECT_NE(log_read(second_name).find("[INFO]多流输出 4"), std::string::npos);
}

//公共前缀归并：先绑定文件节点、后绑定目录节点时，依公共前缀建立父子链表
TEST_F(Log_Test, 目录节点后绑定按公共前缀归并)
{
	//文件流文件名与目录流文件名
	const std::string child_name = "engine_log_merge_child.txt";
	const std::string parent_name = "engine_log_merge_parent.txt";
	//先绑定文件节点（此时树上仅存在该文件路径节点）
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), child_name));
	//输出一条日志，应落到文件节点输出流
	engine::logger.info("先绑文件 {}", 5);
	EXPECT_NE(log_read(child_name).find("[INFO]先绑文件 5"), std::string::npos);

	//后绑定目录节点（与既存文件节点共享公共前缀而成为其父节点）
	ASSERT_TRUE(engine::logger.stream_bind(log_test_dir(), parent_name));
	//文件节点自有输出流优先，日志仍落文件节点输出流
	engine::logger.info("后绑目录 {}", 6);
	EXPECT_NE(log_read(child_name).find("[INFO]后绑目录 6"), std::string::npos);
	EXPECT_EQ(log_read(parent_name).find("后绑目录"), std::string::npos);

	//清空文件节点后日志回落目录节点输出流
	ASSERT_TRUE(engine::logger.stream_clear(log_test_path()));
	engine::logger.info("回落目录 {}", 7);
	EXPECT_NE(log_read(parent_name).find("[INFO]回落目录 7"), std::string::npos);
}

//根节点继承：绑定根路径后，未设自有流的调用处全部继承根节点输出流
TEST_F(Log_Test, 根节点绑定后全体继承)
{
	//日志文件名
	const std::string file_name = "engine_log_root_probe.txt";

	//把根路径节点绑定到该文件
	ASSERT_TRUE(engine::logger.stream_bind("", file_name));
	//此时输出一条信息日志
	engine::logger.info("根流继承 {}", 8);

	//日志应经根节点继承落到文件
	EXPECT_NE(log_read(file_name).find("[INFO]根流继承 8"), std::string::npos);
}

//流对象重载：可把外部输出流（此处为内存流）绑定到路径节点
TEST_F(Log_Test, 流对象重载绑定内存流)
{
	//内存输出流（禁用异常的分配形式）
	std::shared_ptr<std::ostringstream> memory(new(std::nothrow) std::ostringstream());
	ASSERT_TRUE(memory);
	//把内存流绑定到本文件路径节点
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), memory));
	//此时输出一条信息日志
	engine::logger.info("内存流输出 {}", 9);

	//内存流应收到该日志
	EXPECT_NE(memory->str().find("[INFO]内存流输出 9"), std::string::npos);
}

//空流拒绝：绑定空输出流应失败
TEST_F(Log_Test, 空输出流绑定被拒绝)
{
	//绑定空输出流应返回失败
	EXPECT_FALSE(engine::logger.stream_bind(log_test_path(), engine::Log::Stream()));
}

//清空失败：清空不存在路径节点应返回失败
TEST_F(Log_Test, 清空不存在节点返回失败)
{
	//清空不存在的路径节点应返回失败
	EXPECT_FALSE(engine::logger.stream_clear("不存在的路径/不存在的文件.hpp"));
}

//多线程保护：多线程并发输出时日志行完整不交错
TEST_F(Log_Test, 多线程并发输出不交错)
{
	//内存输出流（禁用异常的分配形式）
	std::shared_ptr<std::ostringstream> memory(new(std::nothrow) std::ostringstream());
	ASSERT_TRUE(memory);
	//把内存流绑定到本文件路径节点
	ASSERT_TRUE(engine::logger.stream_bind(log_test_path(), memory));

	//四个线程各写一百条带序号的日志
	std::vector<std::thread> workers;
	for (int index = 0; index < 4; ++index)
		workers.emplace_back([index]
			{
				//逐条写出带线程号与序号的日志
				for (int seq = 0; seq < 100; ++seq)
					engine::logger.info("并发标记 {}:{}", index, seq);
			});
	//等待全部线程结束
	for (std::thread& worker : workers)
		worker.join();

	//逐行校验日志完整性
	std::istringstream reader(memory->str());
	std::string line;
	//总行数与完整行数
	size_t total = 0;
	size_t intact = 0;
	while (std::getline(reader, line))
	{
		//累计总行数
		++total;
		//若该行以级别前缀完整开头则计为完整行
		if (line.rfind("[INFO]并发标记 ", 0) == 0)
			++intact;
	}
	//总行数与完整行数都应等于四百
	EXPECT_EQ(total, 400u);
	EXPECT_EQ(intact, 400u);
}
