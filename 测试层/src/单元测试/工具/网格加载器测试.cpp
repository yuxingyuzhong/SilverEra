//网格加载器测试：覆盖 OBJ 顶点与面解析、多边形三角化、索引写法与各类非法输入
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

//获取网格加载器
#include "src/tools/Mesh_Loader/网格加载器.h"
//获取引擎环境
#include "src/tools/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Auxi_Algorithm/路径字符串转换.h"

//临时网格文件所在目录（相对可执行文件目录）
static const std::string temp_dir = "src/单元测试/工具/";

//写入临时网格文件，返回可读取的相对路径
static std::string mesh_write(const std::string& file_name, const std::string& content)
{
	//相对路径
	const std::string relative_path = temp_dir + file_name;
	//写入文件
	std::ofstream file(engine::Engine_Env::exe_dir_get() /
		engine::string_to_path(relative_path));
	file << content;
	file.close();
	return relative_path;
}

//清理临时网格文件
static void mesh_remove(const std::string& relative_path)
{
	//异常信息记录
	std::error_code ec;
	//删除文件
	std::filesystem::remove(engine::Engine_Env::exe_dir_get() /
		engine::string_to_path(relative_path), ec);
}

//网格加载器测试夹具
class Mesh_Loader_Test : public ::testing::Test
{
};

//三角形网格：顶点与索引被正确解析
TEST_F(Mesh_Loader_Test, 三角形网格解析)
{
	//临时网格文件
	const std::string mesh_path = mesh_write("三角形网格.obj",
		"# 手工三角形\nv 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 0.0 1.0 0.0\nf 1 2 3\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	ASSERT_TRUE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//三个顶点共九个分量
	ASSERT_EQ(mesh_data.vertices.size(), 9u);
	//逐个核对顶点分量
	EXPECT_FLOAT_EQ(mesh_data.vertices[0], 0.0f);
	EXPECT_FLOAT_EQ(mesh_data.vertices[3], 1.0f);
	EXPECT_FLOAT_EQ(mesh_data.vertices[7], 1.0f);
	//一个三角面共三个索引
	ASSERT_EQ(mesh_data.indices.size(), 3u);
	//面索引应换算为自零起算
	EXPECT_EQ(mesh_data.indices[0], 0u);
	EXPECT_EQ(mesh_data.indices[1], 1u);
	EXPECT_EQ(mesh_data.indices[2], 2u);

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//四边形面：以首个顶点为扇形中心三角化为两个三角面
TEST_F(Mesh_Loader_Test, 四边形面被三角化)
{
	//临时网格文件（一个四边形面）
	const std::string mesh_path = mesh_write("四边形网格.obj",
		"v 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 1.0 1.0 0.0\nv 0.0 1.0 0.0\nf 1 2 3 4\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	ASSERT_TRUE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//四个顶点
	ASSERT_EQ(mesh_data.vertices.size(), 12u);
	//两个三角面
	ASSERT_EQ(mesh_data.indices.size(), 6u);
	//第一个三角面
	EXPECT_EQ(mesh_data.indices[0], 0u);
	EXPECT_EQ(mesh_data.indices[1], 1u);
	EXPECT_EQ(mesh_data.indices[2], 2u);
	//第二个三角面
	EXPECT_EQ(mesh_data.indices[3], 0u);
	EXPECT_EQ(mesh_data.indices[4], 2u);
	EXPECT_EQ(mesh_data.indices[5], 3u);

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//面顶点斜杠写法：顶点、纹理与法线索引均可解析
TEST_F(Mesh_Loader_Test, 面顶点斜杠写法可解析)
{
	//临时网格文件（三种面顶点写法）
	const std::string mesh_path = mesh_write("斜杠网格.obj",
		"v 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 0.0 1.0 0.0\n"
		"vt 0.0 0.0\nvt 1.0 0.0\nvt 0.0 1.0\n"
		"vn 0.0 0.0 1.0\n"
		"f 1/1/1 2/2/1 3/3/1\nf 1//1 2//1 3//1\nf 1/1 2/2 3/3\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	ASSERT_TRUE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//三个顶点
	ASSERT_EQ(mesh_data.vertices.size(), 9u);
	//三个三角面
	ASSERT_EQ(mesh_data.indices.size(), 9u);
	//第三个面的首个索引
	EXPECT_EQ(mesh_data.indices[6], 0u);

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//负索引：自顶点末尾起算
TEST_F(Mesh_Loader_Test, 负索引自末尾起算)
{
	//临时网格文件（负索引面）
	const std::string mesh_path = mesh_write("负索引网格.obj",
		"v 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 0.0 1.0 0.0\nf -3 -2 -1\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	ASSERT_TRUE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//负索引应换算为绝对下标
	ASSERT_EQ(mesh_data.indices.size(), 3u);
	EXPECT_EQ(mesh_data.indices[0], 0u);
	EXPECT_EQ(mesh_data.indices[1], 1u);
	EXPECT_EQ(mesh_data.indices[2], 2u);

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//路径非法：不存在与空路径均加载失败
TEST_F(Mesh_Loader_Test, 路径非法时加载失败)
{
	//网格数据
	engine::Mesh_Data mesh_data;
	//不存在的文件
	EXPECT_FALSE(engine::Mesh_Loader::load_obj("src/单元测试/工具/不存在的网格.obj", mesh_data));
	//空路径
	EXPECT_FALSE(engine::Mesh_Loader::load_obj("", mesh_data));
}

//无有效三角面：只有顶点时加载失败且输出被清空
TEST_F(Mesh_Loader_Test, 无有效三角面时加载失败)
{
	//临时网格文件（只有顶点与注释）
	const std::string mesh_path = mesh_write("无面网格.obj",
		"# 只有顶点\nv 0.0 0.0 0.0\nv 1.0 0.0 0.0\n");

	//网格数据（先写入脏数据，验证失败时被清空）
	engine::Mesh_Data mesh_data;
	mesh_data.vertices.push_back(1.0f);
	mesh_data.indices.push_back(0u);

	//加载网格
	EXPECT_FALSE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));
	//失败时输出应被清空
	EXPECT_TRUE(mesh_data.vertices.empty());
	EXPECT_TRUE(mesh_data.indices.empty());

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//面索引越界：加载失败
TEST_F(Mesh_Loader_Test, 面索引越界时加载失败)
{
	//临时网格文件（面索引超出顶点范围）
	const std::string mesh_path = mesh_write("越界索引网格.obj",
		"v 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 0.0 1.0 0.0\nf 1 2 9\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	EXPECT_FALSE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//顶点行分量不足：加载失败
TEST_F(Mesh_Loader_Test, 顶点分量不足时加载失败)
{
	//临时网格文件（顶点缺少 Z 分量）
	const std::string mesh_path = mesh_write("顶点残缺网格.obj",
		"v 0.0 0.0\nf 1 2 3\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	EXPECT_FALSE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//清理临时网格文件
	mesh_remove(mesh_path);
}

//面顶点不足三个：加载失败
TEST_F(Mesh_Loader_Test, 面顶点不足三个时加载失败)
{
	//临时网格文件（面仅有两个顶点）
	const std::string mesh_path = mesh_write("面残缺网格.obj",
		"v 0.0 0.0 0.0\nv 1.0 0.0 0.0\nv 0.0 1.0 0.0\nf 1 2\n");

	//网格数据
	engine::Mesh_Data mesh_data;
	//加载网格
	EXPECT_FALSE(engine::Mesh_Loader::load_obj(mesh_path, mesh_data));

	//清理临时网格文件
	mesh_remove(mesh_path);
}
