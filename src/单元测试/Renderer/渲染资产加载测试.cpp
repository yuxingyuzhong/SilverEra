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
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
//异步用例：轮询等待
#include <chrono>
#include <thread>

#include "Engine/EngineRenderer/src/render/Asset/资产加载.h"
#include "Engine/EngineSystem/src/scene/Manifest/场景清单.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"
#include "Engine/EngineRenderer/src/render/Asset/资产缓存.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"

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

	//资产文件的 UTF-8 窄路径（供加载器入参；不做任何编码转换）
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

	//写一张 24 位 BMP（测试贴图；stb_image 可直接解码）
	void write_bmp(const std::filesystem::path& path, uint32_t width, uint32_t height)
	{
		//行字节数按 4 字节对齐
		const uint32_t row_bytes = ((width * 3 + 3) / 4) * 4;
		const uint32_t pixel_bytes = row_bytes * height;
		const uint32_t file_bytes = 14 + 40 + pixel_bytes;
		std::vector<uint8_t> bytes(file_bytes, 0);
		//文件头：类型 'BM' ＋ 文件大小 ＋ 保留 ＋ 数据偏移
		bytes[0] = 'B';
		bytes[1] = 'M';
		const uint32_t size = file_bytes;
		bytes[2] = static_cast<uint8_t>(size & 0xFF);
		bytes[3] = static_cast<uint8_t>((size >> 8) & 0xFF);
		bytes[4] = static_cast<uint8_t>((size >> 16) & 0xFF);
		bytes[5] = static_cast<uint8_t>((size >> 24) & 0xFF);
		const uint32_t offset = 54;
		bytes[10] = static_cast<uint8_t>(offset & 0xFF);
		bytes[11] = static_cast<uint8_t>((offset >> 8) & 0xFF);
		//信息头：40 字节 / 宽 / 高 / 平面数 1 / 24 位 / 无压缩
		const uint32_t header_size = 40;
		bytes[14] = static_cast<uint8_t>(header_size & 0xFF);
		bytes[18] = static_cast<uint8_t>(width & 0xFF);
		bytes[19] = static_cast<uint8_t>((width >> 8) & 0xFF);
		bytes[22] = static_cast<uint8_t>(height & 0xFF);
		bytes[23] = static_cast<uint8_t>((height >> 8) & 0xFF);
		bytes[26] = 1;
		bytes[28] = 24;
		//像素：每行 BGR 平铺，行序自下而上（全部填白色 255/255/255）
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				const std::size_t base = 54 + static_cast<std::size_t>(y) * row_bytes + x * 3;
				bytes[base] = 255;
				bytes[base + 1] = 255;
				bytes[base + 2] = 255;
			}
		}
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
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

/*
MTL 完整字段解析（第四百二十三轮）
口径：两个 newmtl；第一项显式 Kd / Ks / Ke / Ns / d / map_Kd / map_Bump（含 -bm 选项），
第二项全缺省 → 默认白 / 黑 / Ns 0 / d 1 / 无贴图。
*/
TEST(Renderer_Asset_Load_Test, MTL完整字段解析)
{
	write_text(asset_dir() / L"完整字段.mtl",
		"# 两个材质\n"
		"newmtl 红\n"
		"Kd 0.5 0.25 0.125\n"
		"Ks 1 0.5 0\n"
		"Ke 0.1 0.2 0.3\n"
		"Ns 96\n"
		"d 0.5\n"
		"map_Kd tex.bmp\n"
		"map_Bump -bm 1.0 nrm.bmp\n"
		"newmtl 蓝\n"
		"illum 2\n");

	std::vector<engine::Mtl_Material> 材质;
	ASSERT_TRUE(engine::mtl_materials_load(asset_utf8("完整字段.mtl"), 材质));
	ASSERT_EQ(材质.size(), 2u);
	//第一项逐字段核对
	EXPECT_EQ(材质[0].name, "红");
	EXPECT_FLOAT_EQ(材质[0].diffuse[0], 0.5f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[1], 0.25f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[2], 0.125f);
	EXPECT_FLOAT_EQ(材质[0].specular[0], 1.0f);
	EXPECT_FLOAT_EQ(材质[0].shininess, 96.0f);
	EXPECT_FLOAT_EQ(材质[0].opacity, 0.5f);
	EXPECT_EQ(材质[0].diffuse_map, "tex.bmp");
	EXPECT_EQ(材质[0].normal_map, "nrm.bmp");
	//第二项缺省
	EXPECT_EQ(材质[1].name, "蓝");
	EXPECT_FLOAT_EQ(材质[1].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(材质[1].specular[0], 0.0f);
	EXPECT_FLOAT_EQ(材质[1].shininess, 0.0f);
	EXPECT_FLOAT_EQ(材质[1].opacity, 1.0f);
	EXPECT_TRUE(材质[1].diffuse_map.empty());
	EXPECT_TRUE(材质[1].normal_map.empty());
	std::cout << "[资产加载] MTL 完整字段：材质 " << 材质.size()
		<< " 首项 Kd=(" << 材质[0].diffuse[0] << "," << 材质[0].diffuse[1] << ","
		<< 材质[0].diffuse[2] << ") Ns=" << 材质[0].shininess << std::endl;
}

/*
MTL 缺省值与 Tr 换算（第四百二十三轮）
口径：Tr 0.25 → opacity 0.75；数值越界钳到 [0,1]；Ns 超过 1000 钳到 1000。
*/
TEST(Renderer_Asset_Load_Test, MTL缺省值与Tr换算)
{
	write_text(asset_dir() / L"缺省值.mtl",
		"newmtl 甲\n"
		"Tr 0.25\n"
		"Kd 2 -1 0.4\n"
		"Ns 5000\n");

	std::vector<engine::Mtl_Material> 材质;
	ASSERT_TRUE(engine::mtl_materials_load(asset_utf8("缺省值.mtl"), 材质));
	ASSERT_EQ(材质.size(), 1u);
	EXPECT_FLOAT_EQ(材质[0].opacity, 0.75f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[0], 1.0f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[1], 0.0f);
	EXPECT_FLOAT_EQ(材质[0].diffuse[2], 0.4f);
	EXPECT_FLOAT_EQ(材质[0].shininess, 1000.0f);
}

/*
MTL 非法输入拒绝（第四百二十三轮）
口径：不存在 / 空文件 / 只有注释（无 newmtl）→ 返回假且输出为空。
*/
TEST(Renderer_Asset_Load_Test, MTL非法输入被拒绝)
{
	std::vector<engine::Mtl_Material> 材质;
	EXPECT_FALSE(engine::mtl_materials_load(asset_utf8("不存在.mtl"), 材质));
	EXPECT_TRUE(材质.empty());

	write_text(asset_dir() / L"空.mtl", "");
	EXPECT_FALSE(engine::mtl_materials_load(asset_utf8("空.mtl"), 材质));

	write_text(asset_dir() / L"无材质.mtl", "# 只有注释\nKd 1 1 1\n");
	EXPECT_FALSE(engine::mtl_materials_load(asset_utf8("无材质.mtl"), 材质));
	EXPECT_TRUE(材质.empty());
}

/*
MTL 端到端创建材质（第四百二十三轮）
口径：写一张白色 BMP ＋ 引用它的 MTL（两个材质：一个带 map_Kd，一个纯色），
门面 `material_load_mtl` 应创建 2 个非零材质句柄；卸载后不残留（无 VMA 断言）。
*/
TEST(Renderer_Asset_Load_Test, MTL端到端创建材质)
{
	write_bmp(asset_dir() / L"tex.bmp", 4, 4);
	write_text(asset_dir() / L"贴图材质.mtl",
		"newmtl 贴图甲\n"
		"Kd 0.8 0.8 0.8\n"
		"Ns 64\n"
		"map_Kd tex.bmp\n"
		"newmtl 纯色乙\n"
		"Kd 0.2 0.4 0.9\n"
		"d 0.6\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	std::vector<engine::Material_Handle> 材质;
	ASSERT_TRUE(renderer.material_load_mtl(材质, asset_utf8("贴图材质.mtl")));
	ASSERT_EQ(材质.size(), 2u);
	EXPECT_NE(材质[0], 0u);
	EXPECT_NE(材质[1], 0u);
	std::cout << "[资产加载] MTL 端到端：材质 " << 材质.size() << " 个" << std::endl;

	//卸载（含贴图）后再次查询材质数量应递减
	for (engine::Material_Handle 句柄 : 材质)
		renderer.material_unload(句柄);
}

/*
场景清单纯解析（第四百二十四轮）
口径：mesh / mtl 各一条、place 两条（第二条省略材质列）→ 字段与位置正确。
*/
TEST(Renderer_Asset_Load_Test, 场景清单纯解析)
{
	write_text(asset_dir() / L"清单.txt",
		"# 单个方盒\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 0 2 0 1 1 1\n");

	engine::Scene_Manifest 清单;
	ASSERT_TRUE(engine::scene_manifest_parse(asset_utf8("清单.txt"), 清单));
	ASSERT_EQ(清单.meshes.size(), 1u);
	ASSERT_EQ(清单.materials.size(), 1u);
	ASSERT_EQ(清单.places.size(), 2u);
	EXPECT_EQ(清单.meshes[0].id, "方盒");
	EXPECT_EQ(清单.meshes[0].path, "box.obj");
	EXPECT_EQ(清单.materials[0].id, "方盒材质");
	EXPECT_EQ(清单.materials[0].path, "box.mtl");
	//第一条：带材质、位置原点、缩放 1
	EXPECT_EQ(清单.places[0].material_id, "方盒材质");
	EXPECT_FLOAT_EQ(清单.places[0].position[0], 0.0f);
	EXPECT_FLOAT_EQ(清单.places[0].scale[0], 1.0f);
	//第二条：省略材质列，位置 y = 2
	EXPECT_TRUE(清单.places[1].material_id.empty());
	EXPECT_FLOAT_EQ(清单.places[1].position[1], 2.0f);
	std::cout << "[资产加载] 清单解析：网格 " << 清单.meshes.size() << " 材质 " << 清单.materials.size()
		<< " 摆放 " << 清单.places.size() << std::endl;
}

/*
场景清单非法输入被拒绝（第四百二十四轮）
口径：未知指令 / 引用未知网格 / ID 重复 / 无 place → 全部返回假。
*/
TEST(Renderer_Asset_Load_Test, 场景清单非法被拒绝)
{
	engine::Scene_Manifest 清单;
	//未知指令
	write_text(asset_dir() / L"未知指令.txt", "mesh 甲 box.obj\nobject 乙\nplace 甲 0 0 0 1 1 1\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("未知指令.txt"), 清单));
	//引用未知网格
	write_text(asset_dir() / L"未知网格.txt", "mesh 甲 box.obj\nplace 乙 0 0 0 1 1 1\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("未知网格.txt"), 清单));
	//网格 ID 重复
	write_text(asset_dir() / L"重复ID.txt", "mesh 甲 box.obj\nmesh 甲 box2.obj\nplace 甲 0 0 0 1 1 1\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("重复ID.txt"), 清单));
	//无 place
	write_text(asset_dir() / L"无摆放.txt", "mesh 甲 box.obj\n");
	EXPECT_FALSE(engine::scene_manifest_parse(asset_utf8("无摆放.txt"), 清单));
	EXPECT_TRUE(清单.meshes.empty());
	EXPECT_TRUE(清单.places.empty());
}

/*
场景清单端到端装载（第四百二十四轮）
口径：写方盒 OBJ ＋ 单材质 MTL ＋ 清单（2 个 place，一个带材质一个不带）；
`scene_load` 应建立 1 个网格、1 个材质、2 个对象；带材质对象可用 `mesh_draw_sections` 绘制。
*/
TEST(Renderer_Asset_Load_Test, 场景清单端到端装载)
{
	//方盒几何（2×2 四边形，朝 +Z）
	write_text(asset_dir() / L"box.obj",
		"v -1 -1 0\n"
		"v 1 -1 0\n"
		"v 1 1 0\n"
		"v -1 1 0\n"
		"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
		"vn 0 0 1\n"
		"f 1/1/1 2/2/1 3/3/1\n"
		"f 1/1/1 3/3/1 4/4/1\n");
	//单材质
	write_text(asset_dir() / L"box.mtl", "newmtl 甲\nKd 0.8 0.2 0.2\nNs 64\n");
	//清单：两个摆放（第二个省略材质列）
	write_text(asset_dir() / L"场景.txt",
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 0 2 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//基线（空场景）
	std::vector<uint8_t> reference(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	engine::Asset_Scene 场景;
	ASSERT_TRUE(scene_bundle_load(renderer, 场景, asset_utf8("场景.txt")));
	ASSERT_EQ(场景.meshes.size(), 1u);
	ASSERT_EQ(场景.materials.size(), 1u);
	ASSERT_EQ(场景.objects.size(), 2u);
	EXPECT_NE(场景.objects[0].mesh, 0u);
	ASSERT_EQ(场景.objects[0].materials.size(), 1u);
	EXPECT_TRUE(场景.objects[1].materials.empty());
	EXPECT_FLOAT_EQ(场景.objects[1].position[1], 2.0f);

	//用带材质对象绘制（逐段绑材质入口）
	const engine::Material_Handle* 材质 = 场景.objects[0].materials.data();
	ASSERT_TRUE(renderer.mesh_draw_sections(场景.objects[0].mesh, nullptr,
		材质, static_cast<uint32_t>(场景.objects[0].materials.size())));
	std::vector<uint8_t> shown(ASSET_WIDTH * ASSET_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
	const uint32_t covered = changed_pixels(reference, shown);
	std::cout << "[资产加载] 清单端到端覆盖 " << covered << std::endl;
	EXPECT_GE(covered, ASSET_MIN_PIXELS) << "场景清单装载的网格没有画出来";

	//释放（幂等）
	level_bridge_unload(renderer, 场景);
	EXPECT_TRUE(场景.objects.empty());
	EXPECT_TRUE(场景.meshes.empty());
}

/*
共享网格复用与引用计数（第四百二十五轮）
口径：同路径两次共享加载 → 句柄相同、缓存条目 1；释放一次仍可绘制，释放归零后绘制失败且条目 0。
*/
TEST(Renderer_Asset_Load_Test, 共享网格复用与引用计数)
{
	write_text(asset_dir() / L"shared.obj",
		"v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
		"vn 0 0 1\n"
		"f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Mesh_Handle 甲 = 0;
	engine::Mesh_Handle 乙 = 0;
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 甲, asset_utf8("shared.obj")));
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 乙, asset_utf8("shared.obj")));
	EXPECT_EQ(甲, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	//大小写不同但归一化后同键 → 仍是同一资源
	engine::Mesh_Handle 丙 = 0;
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 丙, "out/资产测试/SHARED.OBJ"));
	EXPECT_EQ(丙, 甲);
	EXPECT_EQ(asset_cache_count(renderer), 1u);

	//释放一次：仍可绘制
	asset_mesh_release(renderer, 甲);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	EXPECT_TRUE(renderer.mesh_draw(乙, nullptr));

	//释放归零：真正卸载，缓存条目清零
	asset_mesh_release(renderer, 乙);
	asset_mesh_release(renderer, 丙);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
	EXPECT_FALSE(renderer.mesh_draw(丙, nullptr)) << "引用计数归零后网格仍可用";
	std::cout << "[资产加载] 共享网格：复用 3 次、释放归零后条目 0" << std::endl;
}

/*
共享加载的隔离与失败（第四百二十五轮）
口径：不同路径各自一份（条目 2）；不存在的路径加载失败且不新增条目。
*/
TEST(Renderer_Asset_Load_Test, 共享加载隔离与失败)
{
	write_text(asset_dir() / L"iso1.obj",
		"v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n");
	write_text(asset_dir() / L"iso2.obj",
		"v -1 -1 0\nv 1 -1 0\nv 0 1 0\nv 0 -1 0\nvn 0 0 1\nf 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Mesh_Handle 甲 = 0;
	engine::Mesh_Handle 乙 = 0;
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 甲, asset_utf8("iso1.obj")));
	ASSERT_TRUE(asset_mesh_shared_load(renderer, 乙, asset_utf8("iso2.obj")));
	EXPECT_NE(甲, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 2u);

	//失败不入缓存
	engine::Mesh_Handle 缺失 = 0;
	EXPECT_FALSE(asset_mesh_shared_load(renderer, 缺失, asset_utf8("不存在.obj")));
	EXPECT_EQ(缺失, 0u);
	EXPECT_EQ(asset_cache_count(renderer), 2u);

	asset_mesh_release(renderer, 甲);
	asset_mesh_release(renderer, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
}

/*
共享纹理复用（第四百二十五轮）
口径：同一 BMP 两次共享加载 → 同一句柄、条目不重复；释放两次后条目清零。
*/
TEST(Renderer_Asset_Load_Test, 共享纹理复用与引用计数)
{
	write_bmp(asset_dir() / L"shared_tex.bmp", 4, 4);

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	uint64_t 甲 = 0;
	uint64_t 乙 = 0;
	ASSERT_TRUE(asset_texture_shared_load(renderer, 甲, asset_utf8("shared_tex.bmp")));
	ASSERT_TRUE(asset_texture_shared_load(renderer, 乙, asset_utf8("shared_tex.bmp")));
	EXPECT_EQ(甲, 乙);
	EXPECT_NE(甲, 0u);
	EXPECT_EQ(asset_cache_count(renderer), 1u);

	asset_texture_release(renderer, 甲);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	asset_texture_release(renderer, 乙);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
	std::cout << "[资产加载] 共享纹理：复用 2 次、释放归零后条目 0" << std::endl;
}

/*
纹理中文目录加载（第四百二十八轮）
口径：贴图放在中文目录（`out/资产测试/`），`texture_load_file` 与共享入口均应成功；释放后缓存条目归零。
*/
TEST(Renderer_Asset_Load_Test, 纹理中文目录加载)
{
	write_bmp(asset_dir() / L"中文贴图.bmp", 4, 4);

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	//普通入口
	uint64_t 句柄 = 0;
	ASSERT_TRUE(renderer.texture_load_file(句柄, asset_utf8("中文贴图.bmp")));
	ASSERT_NE(句柄, 0u);
	renderer.texture_unload(句柄);

	//共享入口（同一中文路径）
	uint64_t 共享 = 0;
	ASSERT_TRUE(asset_texture_shared_load(renderer, 共享, asset_utf8("中文贴图.bmp")));
	ASSERT_NE(共享, 0u);
	EXPECT_EQ(asset_cache_count(renderer), 1u);
	asset_texture_release(renderer, 共享);
	EXPECT_EQ(asset_cache_count(renderer), 0u);
	std::cout << "[资产加载] 中文目录贴图可加载（句柄 " << 共享 << "）" << std::endl;
}

/*
OBJ 解析吞吐（第四百四十二轮 · 性能）
口径：现场生成 50000 三角形（150000 独立顶点、无 vt / vn）的 OBJ，仅测解析（不建 GPU 资源）；断言不高于 5000 ms。
*/
TEST(Renderer_Asset_Load_Test, OBJ解析吞吐)
{
	const uint32_t 三角形数 = 50000u;
	std::string 文本;
	文本.reserve(static_cast<std::size_t>(三角形数) * 3 * 28 + static_cast<std::size_t>(三角形数) * 20);
	//顶点（每个三角形三个独立顶点，坐标落在一个网格上）
	for (uint32_t 序号 = 0; 序号 < 三角形数 * 3; ++序号)
	{
		char 行[64] = {};
		std::snprintf(行, sizeof(行), "v %d %d %d\n",
			static_cast<int>(序号 % 97), static_cast<int>((序号 / 97) % 97), static_cast<int>(序号 % 13));
		文本 += 行;
	}
	//面（扇形三角化不触发，全为三角形）
	for (uint32_t 序号 = 0; 序号 < 三角形数; ++序号)
	{
		char 行[64] = {};
		std::snprintf(行, sizeof(行), "f %u %u %u\n", 序号 * 3 + 1, 序号 * 3 + 2, 序号 * 3 + 3);
		文本 += 行;
	}
	write_text(asset_dir() / L"吞吐.obj", 文本);

	engine::Mesh_File_Data 文件;
	const std::chrono::steady_clock::time_point 开始 = std::chrono::steady_clock::now();
	ASSERT_TRUE(engine::obj_mesh_load(asset_utf8("吞吐.obj"), 文件));
	const std::chrono::steady_clock::time_point 结束 = std::chrono::steady_clock::now();
	const double 毫秒 = std::chrono::duration<double, std::milli>(结束 - 开始).count();
	EXPECT_EQ(文件.data.indices.size(), static_cast<std::size_t>(三角形数) * 3);
	std::cout << "[资产加载] 50000 三角形解析 " << 毫秒 << " ms" << std::endl;
	EXPECT_LT(毫秒, 5000.0);
}

/*
异步加载完成后可绘制（第四百二十六轮）
口径：提交后主线程轮询 ≤ 5 秒应就绪；句柄非 0 且可绘制；再次轮询返回同一句柄（不重建）。
*/
TEST(Renderer_Asset_Load_Test, 异步加载完成后可绘制)
{
	write_text(asset_dir() / L"async.obj",
		"v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
		"vn 0 0 1\n"
		"f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n");

	engine::Renderer_Config config;
	config.width = ASSET_WIDTH;
	config.height = ASSET_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Asset_Async_Task 任务;
	ASSERT_TRUE(asset_mesh_async_load(renderer, 任务, asset_utf8("async.obj")));
	//在途任务不允许被覆盖
	EXPECT_FALSE(asset_mesh_async_load(renderer, 任务, asset_utf8("async.obj")));

	engine::Mesh_Handle 句柄 = 0;
	bool 就绪 = false;
	//最长等待 5 秒（每 2 ms 轮询一次）
	for (uint32_t 尝试 = 0; 尝试 < 2500 && !就绪; ++尝试)
	{
		就绪 = asset_mesh_async_poll(renderer, 任务, 句柄);
		if (!就绪)
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	ASSERT_TRUE(就绪) << "异步解析在 5 秒内未完成";
	ASSERT_NE(句柄, 0u);
	EXPECT_TRUE(renderer.mesh_draw(句柄, nullptr));
	//幂等：再次轮询同一句柄
	engine::Mesh_Handle 再次 = 0;
	EXPECT_TRUE(asset_mesh_async_poll(renderer, 任务, 再次));
	EXPECT_EQ(再次, 句柄);
	std::cout << "[资产加载] 异步加载就绪，句柄 " << 句柄 << std::endl;
	renderer.mesh_unload(句柄);
}

/*
异步加载失败（第四百二十六轮）
口径：不存在路径 → 提交成功（任务建立），轮询始终返回假、句柄保持 0。
*/
TEST(Renderer_Asset_Load_Test, 异步加载失败)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Asset_Async_Task 任务;
	ASSERT_TRUE(asset_mesh_async_load(renderer, 任务, asset_utf8("不存在_async.obj")));
	engine::Mesh_Handle 句柄 = 0;
	bool 就绪 = false;
	//最长等待 5 秒
	for (uint32_t 尝试 = 0; 尝试 < 2500 && !就绪; ++尝试)
	{
		就绪 = asset_mesh_async_poll(renderer, 任务, 句柄);
		if (!就绪)
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	EXPECT_FALSE(就绪) << "不存在的路径不应异步加载成功";
	EXPECT_EQ(句柄, 0u);
}

/*
异步提交的入参保护（第四百二十六轮）
口径：空路径 → 提交失败；已有任务 → 再次提交失败。
*/
TEST(Renderer_Asset_Load_Test, 异步提交入参保护)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Asset_Async_Task 任务;
	//空路径
	EXPECT_FALSE(asset_mesh_async_load(renderer, 任务, ""));
	//未建立任务时轮询返回假
	engine::Mesh_Handle 句柄 = 0;
	EXPECT_FALSE(asset_mesh_async_poll(renderer, 任务, 句柄));
	EXPECT_EQ(句柄, 0u);
}
