//路径校验测试：覆盖空路径、缺失路径、目录路径、常规文件与两个重载的一致性
#include <gtest/gtest.h>

//获取文件路径可用性校验工具
#include "Engine/EngineFoundation/src/detail/methods/文件路径可用性校验.h"
//获取文件路径规范化工具（路径前缀判定）
#include "Engine/EngineFoundation/src/detail/methods/文件路径规范化.h"
//获取引擎环境（取可执行文件目录作为临时文件基址）
#include "Engine/EngineFoundation/src/detail/methods/文件路径获取.h"
//获取路径字符串转换工具
#include "Engine/EngineFoundation/src/detail/methods/路径字符串转换.h"

//路径校验测试夹具
class Path_Check_Test : public ::testing::Test
{
protected:
	//临时文件相对可执行文件目录的路径
	const std::string temp_name = "tests/Foundation/src/工具/路径校验临时文件.txt";

	//写入临时文件并在用例结束后删除
	void SetUp() override
	{
		//打开临时文件
		std::ofstream file(temp_absolute());
		//写入少量内容以保证为非空常规文件
		file << "path_check";
		file.close();
	}

	void TearDown() override
	{
		//删除临时文件
		std::error_code ec;
		std::filesystem::remove(temp_absolute(), ec);
	}

public:
	//取得临时文件的绝对路径
	std::filesystem::path temp_absolute(void) const
	{
		return engine::detail::exe_dir_get() / engine::detail::string_to_path(temp_name);
	}
};

//空路径对象：未解析出任何成分，被拒
TEST_F(Path_Check_Test, 空路径对象被拒)
{
	//默认构造的空路径
	const std::filesystem::path empty_path{};
	//空路径校验失败
	EXPECT_FALSE(engine::detail::path_check(empty_path));
}

//空字符串：转换后仍为空路径，被拒
TEST_F(Path_Check_Test, 空字符串被拒)
{
	//空字符串重载
	EXPECT_FALSE(engine::detail::path_check(std::string()));
}

//缺失路径对象：不存在的路径被拒
TEST_F(Path_Check_Test, 缺失路径对象被拒)
{
	//可执行文件目录下的不存在路径
	const std::filesystem::path missing =
		engine::detail::exe_dir_get() / engine::detail::string_to_path("不存在的目录/不存在的文件.txt");
	//缺失路径校验失败
	EXPECT_FALSE(engine::detail::path_check(missing));
}

//缺失路径字符串：不存在的路径被拒
TEST_F(Path_Check_Test, 缺失路径字符串被拒)
{
	//同一个不存在路径的字符串形式
	EXPECT_FALSE(engine::detail::path_check(std::string("不存在的目录/不存在的文件.txt")));
}

//常规文件对象：存在的普通文件校验通过
TEST_F(Path_Check_Test, 常规文件对象通过)
{
	//夹具写入的临时文件
	EXPECT_TRUE(engine::detail::path_check(temp_absolute()));
}

//常规文件字符串：存在的普通文件校验通过
TEST_F(Path_Check_Test, 常规文件字符串通过)
{
	//临时文件的相对路径文本
	EXPECT_TRUE(engine::detail::path_check(temp_name));
}

//目录路径对象：目录不是常规文件，被拒
TEST_F(Path_Check_Test, 目录路径对象被拒)
{
	//可执行文件目录本身
	const std::filesystem::path directory = engine::detail::exe_dir_get();
	//目录校验失败
	EXPECT_FALSE(engine::detail::path_check(directory));
}

//目录路径字符串：目录不是常规文件，被拒
TEST_F(Path_Check_Test, 目录路径字符串被拒)
{
	//以当前工作目录作为目录样本
	EXPECT_FALSE(engine::detail::path_check(std::string(".")));
}

//中文文件名：含中文路径的常规文件校验通过
TEST_F(Path_Check_Test, 中文路径常规文件通过)
{
	//构造一个中文名临时文件
	const std::filesystem::path chinese_path =
		engine::detail::exe_dir_get() / engine::detail::string_to_path("tests/Foundation/src/工具/路径校验临时中文.txt");

	//写入文件
	std::ofstream file(chinese_path);
	file << "chinese";
	file.close();

	//中文路径校验通过
	EXPECT_TRUE(engine::detail::path_check(chinese_path));

	//清理临时文件
	std::error_code ec;
	std::filesystem::remove(chinese_path, ec);
}

//多层缺失目录：父目录不存在时同样被拒
TEST_F(Path_Check_Test, 多层缺失目录被拒)
{
	//多层均不存在的深层路径
	const std::filesystem::path deep =
		engine::detail::exe_dir_get() /
		engine::detail::string_to_path("层一/层二/层三/文件.txt");
	//深层缺失路径校验失败
	EXPECT_FALSE(engine::detail::path_check(deep));
}

//重载一致性：同一路径的对象与字符串重载判定一致
TEST_F(Path_Check_Test, 两个重载判定一致)
{
	//同一路径的两种形式
	const std::filesystem::path as_path = temp_absolute();
	const std::string as_string = engine::detail::path_to_string(as_path);
	//两种重载应给出相同结论
	EXPECT_EQ(engine::detail::path_check(as_path), engine::detail::path_check(as_string));
}

//删除后失效：文件删除后校验转为失败
TEST_F(Path_Check_Test, 删除后校验失败)
{
	//删除夹具写入的临时文件
	std::error_code ec;
	std::filesystem::remove(temp_absolute(), ec);
	//原路径不再指向常规文件
	EXPECT_FALSE(engine::detail::path_check(temp_absolute()));
}

//相对与绝对：同一文件的相对路径与绝对路径均通过
TEST_F(Path_Check_Test, 相对与绝对均通过)
{
	//相对路径形式
	EXPECT_TRUE(engine::detail::path_check(temp_name));
	//绝对路径形式
	EXPECT_TRUE(engine::detail::path_check(temp_absolute()));
}

//静默存在性检查：文件与目录均算存在，缺失路径为假
TEST_F(Path_Check_Test, 静默存在性检查)
{
	//常规文件
	EXPECT_TRUE(engine::detail::path_exists(temp_absolute()));
	//目录也算存在（与 path_check 的常规文件要求不同）
	EXPECT_TRUE(engine::detail::path_exists(engine::detail::exe_dir_get()));
	//缺失路径
	EXPECT_FALSE(engine::detail::path_exists(
		std::string("不存在的目录/不存在的文件.txt")));
}

//文件体量获取：读取正确，缺失路径返回错误码
TEST_F(Path_Check_Test, 文件体量获取)
{
	//体量输出与错误码
	uintmax_t byte_count = 0;
	std::error_code ec;
	//临时文件内容为 "path_check"（10 字节）
	ASSERT_TRUE(engine::detail::file_size_get(temp_absolute(), byte_count, ec));
	EXPECT_FALSE(ec);
	EXPECT_EQ(byte_count, 10u);
	//字符串重载
	ASSERT_TRUE(engine::detail::file_size_get(temp_name, byte_count, ec));
	EXPECT_EQ(byte_count, 10u);
	//缺失路径失败并保留错误码
	EXPECT_FALSE(engine::detail::file_size_get(
		std::string("不存在的目录/不存在的文件.txt"), byte_count, ec));
	EXPECT_TRUE(ec);
}

//整文件读取：字节内容正确，失败时清空输出
TEST_F(Path_Check_Test, 整文件读取)
{
	//读取结果
	std::vector<unsigned char> bytes;
	std::error_code ec;
	ASSERT_TRUE(engine::detail::file_read_whole(temp_absolute(), bytes, ec));
	EXPECT_FALSE(ec);
	ASSERT_EQ(bytes.size(), 10u);
	//按文本比对内容
	EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "path_check");
	//字符串重载
	bytes.clear();
	ASSERT_TRUE(engine::detail::file_read_whole(temp_name, bytes, ec));
	EXPECT_EQ(bytes.size(), 10u);
	//失败时应清空已有输出
	bytes.assign(3u, 0xFFu);
	EXPECT_FALSE(engine::detail::file_read_whole(
		std::string("不存在的目录/不存在的文件.txt"), bytes, ec));
	EXPECT_TRUE(bytes.empty());
	EXPECT_TRUE(ec);
}

//路径越界检查：拒绝父目录返回符号
TEST_F(Path_Check_Test, 路径越界检查)
{
	//正常相对路径
	EXPECT_TRUE(engine::detail::path_traversal_check(std::string("a/b/c")));
	//前导父目录
	EXPECT_FALSE(engine::detail::path_traversal_check(std::string("../a")));
	//中段父目录
	EXPECT_FALSE(engine::detail::path_traversal_check(std::string("a/../b")));
	//对象重载
	EXPECT_TRUE(engine::detail::path_traversal_check(
		engine::detail::string_to_path("a/b")));
}

//路径前缀判定：逐节比较，不做字符串前缀
TEST_F(Path_Check_Test, 路径前缀判定)
{
	//前缀与目标路径
	const std::filesystem::path prefix = engine::detail::string_to_path("a/b");
	EXPECT_TRUE(engine::detail::path_prefix_check(prefix,
		engine::detail::string_to_path("a/b/c")));
	//自身即前缀
	EXPECT_TRUE(engine::detail::path_prefix_check(prefix, prefix));
	//字符串前缀不代表目录前缀
	EXPECT_FALSE(engine::detail::path_prefix_check(prefix,
		engine::detail::string_to_path("a/bc")));
	//目标短于前缀
	EXPECT_FALSE(engine::detail::path_prefix_check(prefix,
		engine::detail::string_to_path("a")));
	//空前缀
	EXPECT_FALSE(engine::detail::path_prefix_check(std::filesystem::path{}, prefix));
}
