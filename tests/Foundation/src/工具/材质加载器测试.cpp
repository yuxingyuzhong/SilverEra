//材质加载器测试：覆盖 MTL 字段解析、缺省值、钳位、贴图选项与各类非法输入
#include <gtest/gtest.h>

//获取材质加载器
#include "Engine/EngineFoundation/src/loader/assets/Mtl_Loader/材质加载器.h"
//获取引擎环境
#include "Engine/EngineFoundation/src/detail/methods/文件路径获取.h"
//获取路径字符串转化方法
#include "Engine/EngineFoundation/src/detail/methods/路径字符串转换.h"

//临时材质文件所在目录（相对可执行文件目录）
static const std::string mtl_temp_dir = "tests/Foundation/src/工具/";

//写入临时材质文件，返回可读取的相对路径
static std::string mtl_write(const std::string& file_name, const std::string& content)
{
	//相对路径
	const std::string relative_path = mtl_temp_dir + file_name;
	//写入文件
	std::ofstream file(engine::detail::exe_dir_get() /
		engine::detail::string_to_path(relative_path));
	file << content;
	file.close();
	return relative_path;
}

//清理临时材质文件
static void mtl_remove(const std::string& relative_path)
{
	//异常信息记录
	std::error_code ec;
	//删除文件
	std::filesystem::remove(engine::detail::exe_dir_get() /
		engine::detail::string_to_path(relative_path), ec);
}

//材质加载器测试夹具
class Mtl_Loader_Test : public ::testing::Test
{
};

//完整字段：材质名与全部支持字段被正确解析
TEST_F(Mtl_Loader_Test, 完整字段解析)
{
	//临时材质文件
	const std::string mtl_path = mtl_write("完整字段.mtl",
		"newmtl  材质 甲\n"
		"Kd 0.8 0.4 0.2\n"
		"Ks 0.9 0.9 0.9\n"
		"Ke 0.1 0.0 0.0\n"
		"Ns 64\n"
		"d 0.5\n"
		"map_Kd 漫反射.png\n"
		"map_Bump 法线.png\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, materials));

	//只应有一个材质
	ASSERT_EQ(materials.size(), 1u);
	//材质名去掉首尾空白
	EXPECT_EQ(materials[0].name, "材质 甲");
	//漫反射颜色
	EXPECT_FLOAT_EQ(materials[0].diffuse[0], 0.8f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[1], 0.4f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[2], 0.2f);
	//镜面颜色
	EXPECT_FLOAT_EQ(materials[0].specular[0], 0.9f);
	//自发光颜色
	EXPECT_FLOAT_EQ(materials[0].emissive[0], 0.1f);
	//镜面指数
	EXPECT_FLOAT_EQ(materials[0].shininess, 64.0f);
	//不透明度
	EXPECT_FLOAT_EQ(materials[0].opacity, 0.5f);
	//贴图路径
	EXPECT_EQ(materials[0].diffuse_map, "漫反射.png");
	EXPECT_EQ(materials[0].normal_map, "法线.png");

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//缺省值：只写 newmtl 时各字段取结构默认值
TEST_F(Mtl_Loader_Test, 缺省值正确)
{
	//临时材质文件（只有材质名）
	const std::string mtl_path = mtl_write("缺省值.mtl", "newmtl 默认材质\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, materials));

	//只应有一个材质
	ASSERT_EQ(materials.size(), 1u);
	//漫反射缺省白
	EXPECT_FLOAT_EQ(materials[0].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[1], 1.0f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[2], 1.0f);
	//镜面与自发光缺省黑
	EXPECT_FLOAT_EQ(materials[0].specular[0], 0.0f);
	EXPECT_FLOAT_EQ(materials[0].emissive[0], 0.0f);
	//镜面指数缺省 0
	EXPECT_FLOAT_EQ(materials[0].shininess, 0.0f);
	//不透明度缺省 1
	EXPECT_FLOAT_EQ(materials[0].opacity, 1.0f);
	//贴图缺省为空
	EXPECT_TRUE(materials[0].diffuse_map.empty());
	EXPECT_TRUE(materials[0].normal_map.empty());

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//透明度换算：Tr 按 1 − Tr 写入不透明度
TEST_F(Mtl_Loader_Test, 透明度按反比换算)
{
	//临时材质文件
	const std::string mtl_path = mtl_write("透明度.mtl",
		"newmtl 半透明\nTr 0.25\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, materials));

	//不透明度应为 1 − 0.25
	ASSERT_EQ(materials.size(), 1u);
	EXPECT_FLOAT_EQ(materials[0].opacity, 0.75f);

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//数值钳位：颜色与指数越界后被钳到合法区间
TEST_F(Mtl_Loader_Test, 数值越界被钳位)
{
	//临时材质文件（越界颜色 / 不透明度 / 指数）
	const std::string mtl_path = mtl_write("钳位.mtl",
		"newmtl 越界\nKd 1.5 -0.2 0.3\nKs 2.0 2.0 2.0\nNs 2000\nd -0.5\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, materials));

	ASSERT_EQ(materials.size(), 1u);
	//Kd 钳到 [0,1]
	EXPECT_FLOAT_EQ(materials[0].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[1], 0.0f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[2], 0.3f);
	//Ks 钳到 1
	EXPECT_FLOAT_EQ(materials[0].specular[0], 1.0f);
	//Ns 钳到 1000
	EXPECT_FLOAT_EQ(materials[0].shininess, 1000.0f);
	//d 钳到 0
	EXPECT_FLOAT_EQ(materials[0].opacity, 0.0f);

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//贴图选项：带参数的 map_Bump 取最后一个非选项标记作为路径
TEST_F(Mtl_Loader_Test, 贴图选项参数被跳过)
{
	//临时材质文件（法线贴图带 -bm 选项）
	const std::string mtl_path = mtl_write("贴图选项.mtl",
		"newmtl 带选项\nmap_Bump -bm 1.0 法线.png\nmap_Kd -s 1.0 1.0 漫反射.png\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, materials));

	ASSERT_EQ(materials.size(), 1u);
	EXPECT_EQ(materials[0].normal_map, "法线.png");
	EXPECT_EQ(materials[0].diffuse_map, "漫反射.png");

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//多材质：各材质字段互不串扰
TEST_F(Mtl_Loader_Test, 多材质字段独立)
{
	//临时材质文件（两个材质）
	const std::string mtl_path = mtl_write("多材质.mtl",
		"newmtl 甲\nKd 1.0 0.0 0.0\n"
		"newmtl 乙\nKd 0.0 1.0 0.0\nNs 32\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, materials));

	//两个材质
	ASSERT_EQ(materials.size(), 2u);
	EXPECT_EQ(materials[0].name, "甲");
	EXPECT_FLOAT_EQ(materials[0].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(materials[0].diffuse[1], 0.0f);
	EXPECT_EQ(materials[1].name, "乙");
	EXPECT_FLOAT_EQ(materials[1].diffuse[0], 0.0f);
	EXPECT_FLOAT_EQ(materials[1].diffuse[1], 1.0f);
	EXPECT_FLOAT_EQ(materials[1].shininess, 32.0f);

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//路径非法：不存在与空路径均加载失败且输出被清空
TEST_F(Mtl_Loader_Test, 路径非法时加载失败)
{
	//材质库（先写入脏数据，验证失败时被清空）
	std::vector<engine::Mtl_Material> materials;
	materials.push_back(engine::Mtl_Material{});
	//不存在的文件
	EXPECT_FALSE(engine::Mtl_Loader::load_file("tests/Foundation/src/工具/不存在的材质.mtl", materials));
	EXPECT_TRUE(materials.empty());
	//空路径
	EXPECT_FALSE(engine::Mtl_Loader::load_file("", materials));
	EXPECT_TRUE(materials.empty());
}

//空文件：不含任何内容时加载失败
TEST_F(Mtl_Loader_Test, 空文件加载失败)
{
	//写入空内容的临时材质文件
	const std::string mtl_path = mtl_write("空材质.mtl", "");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库应失败
	EXPECT_FALSE(engine::Mtl_Loader::load_file(mtl_path, materials));
	//失败后输出为空
	EXPECT_TRUE(materials.empty());

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//无材质声明：只有注释与属性行时加载失败
TEST_F(Mtl_Loader_Test, 无材质声明时加载失败)
{
	//临时材质文件（缺少 newmtl）
	const std::string mtl_path = mtl_write("无材质.mtl",
		"# 只有注释\nKd 1.0 1.0 1.0\n");

	//材质库
	std::vector<engine::Mtl_Material> materials;
	//加载材质库应失败
	EXPECT_FALSE(engine::Mtl_Loader::load_file(mtl_path, materials));
	EXPECT_TRUE(materials.empty());

	//清理临时材质文件
	mtl_remove(mtl_path);
}

//重复加载：同一文件两次加载结果一致
TEST_F(Mtl_Loader_Test, 重复加载同一文件结果一致)
{
	//临时材质文件
	const std::string mtl_path = mtl_write("重复加载.mtl",
		"newmtl 甲\nKd 0.5 0.6 0.7\nNs 10\nmap_Kd 贴图.png\n");

	//第一次加载结果
	std::vector<engine::Mtl_Material> first;
	//第二次加载结果
	std::vector<engine::Mtl_Material> second;
	//两次加载均应成功
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, first));
	ASSERT_TRUE(engine::Mtl_Loader::load_file(mtl_path, second));

	//两次结果数量一致
	ASSERT_EQ(first.size(), second.size());
	//名字与贴图路径一致
	EXPECT_EQ(first[0].name, second[0].name);
	EXPECT_EQ(first[0].diffuse_map, second[0].diffuse_map);
	//颜色与指数一致
	EXPECT_FLOAT_EQ(first[0].diffuse[1], second[0].diffuse[1]);
	EXPECT_FLOAT_EQ(first[0].shininess, second[0].shininess);

	//清理临时材质文件
	mtl_remove(mtl_path);
}
