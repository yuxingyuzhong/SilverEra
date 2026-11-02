/*
渲染资产加载测试（第四百二十二轮 · 方向五：资产加载 · 第一阶段 OBJ）
覆盖：完整属性（v / vt / vn ＋ usemtl 分段）解析、缺法线自动补全、负索引与多边形三角化、
非法输入拒绝（不存在 / 索引越界 / 无三角面 / 空文件），以及端到端「加载 → 绘制 → 回读」覆盖。
口径：解析用例直接调 `engine::obj_mesh_load`（纯 CPU，不建 GPU 资源）；端到端用例用隐藏窗口
      与「同相机空场景」基线比较差像素数。
临时文件写在 `out/资产测试/`：写盘用宽字符路径、加载器入参用 UTF-8 窄字符串，两者都不做编码转换。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Application/Renderer/src/render/Asset/资产加载.h"
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//端到端窗口尺寸
	constexpr uint32_t ASSET_WIDTH = 128;
	constexpr uint32_t ASSET_HEIGHT = 72;
	//端到端覆盖下限（2×2 四边形在默认相机下占屏比例远大于此）
	constexpr uint32_t ASSET_MIN_PIXELS = 800;

	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path asset_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"资产测试");
	}

	//OBJ 的 UTF-8 窄路径（供加载器入参；不做任何编码转换）
	std::string asset_utf8(const char* name)
	{
		return std::string("out/资产测试/") + name;
	}

	//写一个 UTF-8 文本文件（目录自动创建）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//整帧平均亮度（Rec.709 加权）
	float average_luma(const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
		return static_cast<float>(sum / static_cast<double>(ASSET_WIDTH * ASSET_HEIGHT));
	}

	//两帧之间逐通道差 ≥ 6 的像素数
	uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				if (delta < 0)
					delta = -delta;
				if (maximum < delta)
					maximum = delta;
			}
			if (maximum >= 6)
				++count;
		}
		return count;
	}

	//第 index 个顶点的法线长度
	float normal_length(const engine::Mesh_Data& data, std::size_t index)
	{
		const engine::Vertex& vertex = data.vertices[index];
		return std::sqrt(vertex.normal[0] * vertex.normal[0]
			+ vertex.normal[1] * vertex.normal[1] + vertex.normal[2] * vertex.normal[2]);
	}
}

/*
完整属性 ＋ usemtl 分段（第四百二十二轮）
口径：4 顶点 / 4 纹理坐标 / 1 法线；两个 usemtl 各一个三角形 → 4 个唯一顶点、6 条索引、2 段。
*/
TEST(Renderer_Asset_Load_Test, OBJ完整属性与分段)
{
	write_text(asset_dir() / L"完整属性.obj",
		"# 两段材质的最小 OBJ\n"
		"v 0 0 0\n"
		"v 1 0 0\n"
		"v 1 1 0\n"
		"v 0 1 0\n"
		"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
		"vn 0 0 1\n"
		"usemtl 甲\n"
		"f 1/1/1 2/2/1 3/3/1\n"
		"usemtl 乙\n"
		"f 1/1/1 3/3/1 4/4/1\n");

	engine::Mesh_File_Data file;
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("完整属性.obj"), file));
	EXPECT_EQ(file.data.vertices.size(), 4u);
	EXPECT_EQ(file.data.indices.size(), 6u);
	ASSERT_EQ(file.sections.size(), 2u);
	ASSERT_EQ(file.names.size(), 2u);
	EXPECT_EQ(file.sections[0].first_index, 0u);
	EXPECT_EQ(file.sections[0].index_count, 3u);
	EXPECT_EQ(file.sections[0].material, 0u);
	EXPECT_EQ(file.sections[1].first_index, 3u);
	EXPECT_EQ(file.sections[1].index_count, 3u);
	EXPECT_EQ(file.sections[1].material, 1u);
	EXPECT_EQ(file.names[0], "甲");
	EXPECT_EQ(file.names[1], "乙");
	//属性落到顶点：法线为 +Z、第二个顶点 uv = (1,0)
	EXPECT_FLOAT_EQ(file.data.vertices[0].normal[2], 1.0f);
	EXPECT_FLOAT_EQ(file.data.vertices[1].uv[0], 1.0f);
	std::cout << "[资产加载] 完整属性：顶点 " << file.data.vertices.size()
		<< " 索引 " << file.data.indices.size() << " 段 " << file.sections.size() << std::endl;
}

/*
缺法线自动补全（第四百二十二轮）
口径：只有 v 与 f（无 vn）的四边形 → 每个顶点法线单位化且为 +Z（面朝相机）。
*/
TEST(Renderer_Asset_Load_Test, OBJ缺法线自动生成)
{
	write_text(asset_dir() / L"缺法线.obj",
		"v 0 0 0\n"
		"v 1 0 0\n"
		"v 1 1 0\n"
		"v 0 1 0\n"
		"f 1 2 3\n"
		"f 1 3 4\n");

	engine::Mesh_File_Data file;
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("缺法线.obj"), file));
	EXPECT_EQ(file.data.vertices.size(), 4u);
	EXPECT_EQ(file.data.indices.size(), 6u);
	//每顶点法线：单位长度且朝 +Z
	for (std::size_t index = 0; index < file.data.vertices.size(); ++index)
	{
		EXPECT_NEAR(normal_length(file.data, index), 1.0f, 1.0e-5f);
		EXPECT_NEAR(file.data.vertices[index].normal[0], 0.0f, 1.0e-5f);
		EXPECT_NEAR(file.data.vertices[index].normal[1], 0.0f, 1.0e-5f);
		EXPECT_GE(file.data.vertices[index].normal[2], 0.999f);
	}
	//无 usemtl：只有一段，材质槽 0，名字为空
	ASSERT_EQ(file.sections.size(), 1u);
	EXPECT_EQ(file.sections[0].index_count, 6u);
	EXPECT_EQ(file.sections[0].material, 0u);
	EXPECT_TRUE(file.names[0].empty());
}

/*
负索引与多边形三角化（第四百二十二轮）
口径：四边形面用负索引书写 → 4 顶点（按位置去重）、6 条索引（扇形三角化）。
*/
TEST(Renderer_Asset_Load_Test, OBJ负索引与多边形三角化)
{
	write_text(asset_dir() / L"负索引.obj",
		"v 0 0 0\n"
		"v 1 0 0\n"
		"v 1 1 0\n"
		"v 0 1 0\n"
		"f -4 -3 -2 -1\n");

	engine::Mesh_File_Data file;
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("负索引.obj"), file));
	EXPECT_EQ(file.data.vertices.size(), 4u);
	EXPECT_EQ(file.data.indices.size(), 6u);
	//三角化顺序：0-1-2、0-2-3
	EXPECT_EQ(file.data.indices[0], 0u);
	EXPECT_EQ(file.data.indices[1], 1u);
	EXPECT_EQ(file.data.indices[2], 2u);
	EXPECT_EQ(file.data.indices[3], 0u);
	EXPECT_EQ(file.data.indices[4], 2u);
	EXPECT_EQ(file.data.indices[5], 3u);
}

/*
非法输入拒绝（第四百二十二轮）
口径：不存在的路径 / 索引越界 / 无三角面 / 空文件，均返回假且输出为空。
*/
TEST(Renderer_Asset_Load_Test, OBJ非法输入被拒绝)
{
	engine::Mesh_File_Data file;
	//不存在的文件
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("不存在.obj"), file));
	EXPECT_TRUE(file.data.vertices.empty());
	EXPECT_TRUE(file.data.indices.empty());
	EXPECT_TRUE(file.sections.empty());

	//索引越界（只有 3 个顶点，面里引用第 4 个）
	write_text(asset_dir() / L"越界.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 1 2 4\n");
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("越界.obj"), file));

	//无三角面（只有顶点与注释）
	write_text(asset_dir() / L"无面.obj", "# 只有顶点\nv 0 0 0\n");
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("无面.obj"), file));

	//空文件
	write_text(asset_dir() / L"空.obj", "");
	EXPECT_FALSE(engine::obj_mesh_load(asset_utf8("空.obj"), file));
}

/*
端到端加载并绘制（第四百二十二轮）
口径：写一个覆盖视口的 2×2 四边形 OBJ（含 vt / vn），`mesh_load_obj` 后用 `mesh_draw` 绘制，
与「同相机空场景」基线比较，差像素数 ≥ 800 视为真的画出来了。
*/
TEST(Renderer_Asset_Load_Test, OBJ端到端绘制)
{
	write_text(asset_dir() / L"四边形.obj",
		"v -1 -1 0\n"
		"v 1 -1 0\n"
		"v 1 1 0\n"
		"v -1 1 0\n"
		"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
		"vn 0 0 1\n"
		"f 1/1/1 2/2/1 3/3/1\n"
		"f 1/1/1 3/3/1 4/4/1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//基线：同相机、不登记绘制（只有清屏 ＋ 天空）
	std::vector<uint8_t> reference(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//加载 OBJ 并绘制
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load_obj(mesh, asset_utf8("四边形.obj")));
	ASSERT_NE(mesh, 0u);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));

	std::vector<uint8_t> shown(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
	const uint32_t covered = changed_pixels(reference, shown);
	const float luma = average_luma(shown);
	RecordProperty("asset_covered_pixels", static_cast<int>(covered));
	std::cout << "[资产加载] 端到端覆盖 " << covered << " 亮度 " << luma << std::endl;
	EXPECT_GE(covered, ASSET_MIN_PIXELS) << "OBJ 网格没有真正画出来";
	EXPECT_GT(luma, 0.0f);
	renderer.mesh_unload(mesh);
}
