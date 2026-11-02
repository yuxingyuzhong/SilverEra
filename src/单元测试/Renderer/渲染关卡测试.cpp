/*
渲染关卡测试（第四百二十九轮 · 方向六：场景序列化与关卡装载 · 阶段 A）
覆盖：关卡文本往返（全字段逐位一致）、缺省补齐、非法文本拒绝（未知段 / 未知键 / 字段数不符 /
非有限值 / 开关非 0-1 / 点光超 8 盏 / 资产行缺字段）、资产声明行原样保留、
材质实例行（往返 / 缺省 / 数值越界 / 结构非法 / 重名；第四百六十六轮）、
材质实例装配（拒绝 / 等价；第四百六十七轮）、材质实例覆盖（tint / 自发光；第四百六十九轮）、
材质实例卡通（往返 / 越界 / 结构 / 生效；第四百七十一 / 472 轮）、
聚光灯阴影（渲染 / 开关文本；第四百七十四 / 475 轮）、写入与读取失败路径。
口径：只测文本层（`engine::level_text_read` / `engine::level_text_write`），不建任何 GPU 资源。
临时文件写在 `out/关卡测试/`：写盘用宽字符路径，加载 / 写出入参用 UTF-8 窄字符串。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Application/Renderer/src/render/Level/关卡文件.h"
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//端到端窗口尺寸
	constexpr uint32_t LEVEL_WIDTH = 128;
	constexpr uint32_t LEVEL_HEIGHT = 72;
	//端到端覆盖下限（2×2 四边形在默认相机下占屏比例远大于此）
	constexpr uint32_t LEVEL_MIN_PIXELS = 800;

	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path level_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"关卡测试");
	}

	//关卡文件的 UTF-8 窄路径（供读写入参）
	std::string level_utf8(const char* name)
	{
		return std::string("out/关卡测试/") + name;
	}

	//写一个 UTF-8 文本文件（目录自动创建；宽路径写盘）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//浮点逐位相等（往返口径：%.9g 必须能唯一还原二进制 32 位浮点）
	bool bit_equal(float left, float right)
	{
		return std::memcmp(&left, &right, sizeof(float)) == 0;
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

	//写方盒几何 ＋ 单材质（关卡端到端用）
	void write_box_assets(void)
	{
		write_text(level_dir() / L"box.obj",
			"v -1 -1 0\n"
			"v 1 -1 0\n"
			"v 1 1 0\n"
			"v -1 1 0\n"
			"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
			"vn 0 0 1\n"
			"f 1/1/1 2/2/1 3/3/1\n"
			"f 1/1/1 3/3/1 4/4/1\n");
		write_text(level_dir() / L"box.mtl", "newmtl 甲\nKd 0.8 0.2 0.2\nNs 64\n");
	}

	/*
	写立方体几何 ＋ 单材质（第四百七十二轮；卡通用例需要多变法线）
	六个面各自独立法线，供逐物体卡通色阶产生可见差异；正面 / 右侧 / 顶面按外法线逆时针绕序。
	*/
	void write_cube_assets(void)
	{
		write_text(level_dir() / L"cube.obj",
			"v -1 -1 -1\n"
			"v 1 -1 -1\n"
			"v 1 1 -1\n"
			"v -1 1 -1\n"
			"v -1 -1 1\n"
			"v 1 -1 1\n"
			"v 1 1 1\n"
			"v -1 1 1\n"
			"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
			"vn 0 0 -1\nvn 0 0 1\nvn -1 0 0\nvn 1 0 0\nvn 0 -1 0\nvn 0 1 0\n"
			"f 5/1/2 6/2/2 7/3/2\n"
			"f 5/1/2 7/3/2 8/4/2\n"
			"f 6/1/4 7/2/4 3/3/4\n"
			"f 6/1/4 3/3/4 2/4/4\n"
			"f 8/1/6 7/2/6 3/3/6\n"
			"f 8/1/6 3/3/6 4/4/6\n"
			"f 2/1/1 1/2/1 4/3/1\n"
			"f 2/1/1 4/3/1 3/4/1\n"
			"f 5/1/3 1/2/3 4/3/3\n"
			"f 5/1/3 4/3/3 8/4/3\n"
			"f 1/1/5 2/2/5 6/3/5\n"
			"f 1/1/5 6/3/5 5/4/5\n");
		write_text(level_dir() / L"cube.mtl", "newmtl 甲\nKd 0.7 0.5 0.4\nNs 64\n");
	}

	//三通道浮点逐位相等
	bool triple_bit_equal(const float* left, const float* right)
	{
		return bit_equal(left[0], right[0]) && bit_equal(left[1], right[1]) && bit_equal(left[2], right[2]);
	}

	//逐位比较两个关卡的全字段（失败时输出首个差异字段）
	void expect_level_equal(const engine::Level_Data& left, const engine::Level_Data& right)
	{
		EXPECT_EQ(left.name, right.name);
		EXPECT_EQ(left.version, right.version) << "格式版本不一致";

		//相机组
		EXPECT_TRUE(triple_bit_equal(left.camera.position, right.camera.position)) << "相机位置不一致";
		EXPECT_TRUE(triple_bit_equal(left.camera.target, right.camera.target)) << "相机注视点不一致";
		EXPECT_TRUE(triple_bit_equal(left.camera.up, right.camera.up)) << "相机上方向不一致";
		EXPECT_TRUE(bit_equal(left.camera.fov, right.camera.fov)) << "视场角不一致";
		EXPECT_TRUE(bit_equal(left.camera.near_plane, right.camera.near_plane)) << "近裁剪面不一致";
		EXPECT_TRUE(bit_equal(left.camera.far_plane, right.camera.far_plane)) << "远裁剪面不一致";
		EXPECT_EQ(left.camera.ortho, right.camera.ortho) << "正交开关不一致";
		EXPECT_TRUE(bit_equal(left.camera.ortho_height, right.camera.ortho_height)) << "正交高度不一致";
		EXPECT_TRUE(bit_equal(left.camera.aspect, right.camera.aspect)) << "宽高比不一致";
		for (std::size_t index = 0; index < 4; ++index)
			EXPECT_TRUE(bit_equal(left.camera.clear_color[index], right.camera.clear_color[index])) << "清屏色不一致";

		//光照组（标量 / 颜色）
		EXPECT_TRUE(triple_bit_equal(left.light.direction, right.light.direction)) << "光照方向不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.color, right.light.color)) << "光照颜色不一致";
		EXPECT_TRUE(bit_equal(left.light.intensity, right.light.intensity)) << "光照强度不一致";
		EXPECT_TRUE(bit_equal(left.light.ambient, right.light.ambient)) << "环境光强度不一致";
		EXPECT_TRUE(bit_equal(left.light.specular_intensity, right.light.specular_intensity)) << "高光强度不一致";
		EXPECT_TRUE(bit_equal(left.light.specular_shininess, right.light.specular_shininess)) << "高光光泽度不一致";
		EXPECT_EQ(left.light.attenuation_mode, right.light.attenuation_mode) << "衰减模式不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.ambient_color, right.light.ambient_color)) << "环境光颜色不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.ambient_ground, right.light.ambient_ground)) << "环境光地面色不一致";
		EXPECT_TRUE(bit_equal(left.light.ambient_hemisphere, right.light.ambient_hemisphere)) << "半球强度不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.fill_direction, right.light.fill_direction)) << "补光方向不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.fill_color, right.light.fill_color)) << "补光颜色不一致";
		EXPECT_TRUE(bit_equal(left.light.fill_intensity, right.light.fill_intensity)) << "补光强度不一致";
		EXPECT_TRUE(bit_equal(left.light.directional_specular, right.light.directional_specular)) << "方向光高光缩放不一致";
		EXPECT_TRUE(bit_equal(left.light.fill_specular, right.light.fill_specular)) << "补光高光缩放不一致";

		//阴影 / PBR / 卡通 / 描边
		EXPECT_EQ(left.light.shadow_enabled, right.light.shadow_enabled) << "阴影开关不一致";
		EXPECT_TRUE(bit_equal(left.light.shadow_bias, right.light.shadow_bias)) << "阴影偏置不一致";
		EXPECT_TRUE(bit_equal(left.light.shadow_softness, right.light.shadow_softness)) << "阴影软度不一致";
		EXPECT_TRUE(bit_equal(left.light.shadow_strength, right.light.shadow_strength)) << "阴影强度不一致";
		EXPECT_EQ(left.light.shadow_resolution, right.light.shadow_resolution) << "阴影分辨率不一致";
		EXPECT_EQ(left.light.pbr_enabled, right.light.pbr_enabled) << "PBR 开关不一致";
		EXPECT_TRUE(bit_equal(left.light.pbr_roughness, right.light.pbr_roughness)) << "PBR 粗糙度不一致";
		EXPECT_TRUE(bit_equal(left.light.pbr_metallic, right.light.pbr_metallic)) << "PBR 金属度不一致";
		EXPECT_EQ(left.light.toon_steps, right.light.toon_steps) << "卡通色阶不一致";
		EXPECT_TRUE(bit_equal(left.light.toon_rim, right.light.toon_rim)) << "边缘光不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.toon_shadow_tint, right.light.toon_shadow_tint)) << "暗部色调不一致";
		EXPECT_TRUE(bit_equal(left.light.toon_specular, right.light.toon_specular)) << "卡通高光阈值不一致";
		EXPECT_TRUE(bit_equal(left.light.outline_width, right.light.outline_width)) << "描边宽度不一致";
		EXPECT_TRUE(triple_bit_equal(left.light.outline_color, right.light.outline_color)) << "描边色不一致";

		//点光源 / 聚光灯
		EXPECT_EQ(left.light.point_count, right.light.point_count) << "点光源数量不一致";
		EXPECT_EQ(left.light.spot_count, right.light.spot_count) << "聚光灯数量不一致";
		for (uint32_t index = 0; index < left.light.point_count && index < 8u; ++index)
		{
			EXPECT_TRUE(triple_bit_equal(left.light.point_position[index], right.light.point_position[index])) << "点光位置不一致";
			EXPECT_TRUE(triple_bit_equal(left.light.point_color[index], right.light.point_color[index])) << "点光颜色不一致";
			EXPECT_TRUE(bit_equal(left.light.point_radius[index], right.light.point_radius[index])) << "点光半径不一致";
			EXPECT_TRUE(bit_equal(left.light.point_intensity[index], right.light.point_intensity[index])) << "点光强度不一致";
			EXPECT_EQ(left.light.point_attenuation[index], right.light.point_attenuation[index]) << "点光衰减模式不一致";
			EXPECT_TRUE(bit_equal(left.light.point_specular[index], right.light.point_specular[index])) << "点光高光缩放不一致";
			EXPECT_TRUE(bit_equal(left.light.point_shininess[index], right.light.point_shininess[index])) << "点光光泽度不一致";
		}
		for (uint32_t index = 0; index < left.light.spot_count && index < 8u; ++index)
		{
			EXPECT_TRUE(triple_bit_equal(left.light.spot_position[index], right.light.spot_position[index])) << "聚光位置不一致";
			EXPECT_TRUE(triple_bit_equal(left.light.spot_direction[index], right.light.spot_direction[index])) << "聚光方向不一致";
			EXPECT_TRUE(triple_bit_equal(left.light.spot_color[index], right.light.spot_color[index])) << "聚光颜色不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_radius[index], right.light.spot_radius[index])) << "聚光半径不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_intensity[index], right.light.spot_intensity[index])) << "聚光强度不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_inner_deg[index], right.light.spot_inner_deg[index])) << "聚光内锥不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_outer_deg[index], right.light.spot_outer_deg[index])) << "聚光外锥不一致";
			EXPECT_EQ(left.light.spot_attenuation[index], right.light.spot_attenuation[index]) << "聚光衰减模式不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_specular[index], right.light.spot_specular[index])) << "聚光高光缩放不一致";
			EXPECT_TRUE(bit_equal(left.light.spot_shininess[index], right.light.spot_shininess[index])) << "聚光光泽度不一致";
		}

		//雾组
		EXPECT_TRUE(triple_bit_equal(left.fog.color, right.fog.color)) << "雾色不一致";
		EXPECT_TRUE(bit_equal(left.fog.density, right.fog.density)) << "雾密度不一致";
		EXPECT_TRUE(bit_equal(left.fog.start, right.fog.start)) << "雾起点不一致";
		EXPECT_TRUE(bit_equal(left.fog.end, right.fog.end)) << "雾终点不一致";
		EXPECT_EQ(left.fog.mode, right.fog.mode) << "雾模式不一致";
		EXPECT_TRUE(bit_equal(left.fog.height_falloff, right.fog.height_falloff)) << "雾高度衰减不一致";

		//资产段落
		ASSERT_EQ(left.asset_lines.size(), right.asset_lines.size());
		for (std::size_t index = 0; index < left.asset_lines.size(); ++index)
			EXPECT_EQ(left.asset_lines[index], right.asset_lines[index]) << "资产行不一致";
	}

	//构造一份「全字段非默认」的关卡
	engine::Level_Data rich_level(void)
	{
		engine::Level_Data level;
		level.name = "测试关卡 一号";

		//相机
		level.camera.position[0] = 1.5f;
		level.camera.position[1] = -2.25f;
		level.camera.position[2] = 7.0f;
		level.camera.target[0] = 0.25f;
		level.camera.target[1] = 0.5f;
		level.camera.target[2] = -1.0f;
		level.camera.fov = 0.9f;
		level.camera.near_plane = 0.05f;
		level.camera.far_plane = 250.0f;
		level.camera.ortho = true;
		level.camera.ortho_height = 3.5f;
		level.camera.aspect = 1.777f;
		level.camera.clear_color[0] = 0.1f;
		level.camera.clear_color[1] = 0.2f;
		level.camera.clear_color[2] = 0.3f;
		level.camera.clear_color[3] = 1.0f;

		//光照
		level.light.direction[0] = 0.3f;
		level.light.direction[1] = -1.0f;
		level.light.direction[2] = 0.2f;
		level.light.color[0] = 1.0f;
		level.light.color[1] = 0.9f;
		level.light.color[2] = 0.8f;
		level.light.intensity = 1.25f;
		level.light.ambient = 0.15f;
		level.light.specular_intensity = 0.6f;
		level.light.specular_shininess = 42.0f;
		level.light.attenuation_mode = 1;
		level.light.ambient_color[0] = 0.9f;
		level.light.ambient_color[1] = 0.95f;
		level.light.ambient_color[2] = 1.0f;
		level.light.ambient_ground[0] = 0.2f;
		level.light.ambient_ground[1] = 0.25f;
		level.light.ambient_ground[2] = 0.3f;
		level.light.ambient_hemisphere = 0.4f;
		level.light.fill_direction[0] = -0.5f;
		level.light.fill_direction[1] = -0.2f;
		level.light.fill_direction[2] = -1.0f;
		level.light.fill_color[0] = 0.5f;
		level.light.fill_color[1] = 0.6f;
		level.light.fill_color[2] = 0.7f;
		level.light.fill_intensity = 0.35f;
		level.light.directional_specular = 0.8f;
		level.light.fill_specular = 0.0f;
		level.light.shadow_enabled = 1;
		level.light.shadow_bias = 0.004f;
		level.light.shadow_softness = 0.5f;
		level.light.shadow_strength = 0.8f;
		level.light.shadow_resolution = 2048;
		level.light.pbr_enabled = 1;
		level.light.pbr_roughness = 0.6f;
		level.light.pbr_metallic = 0.2f;
		level.light.toon_steps = 3;
		level.light.toon_rim = 0.25f;
		level.light.toon_shadow_tint[0] = 0.9f;
		level.light.toon_shadow_tint[1] = 0.85f;
		level.light.toon_shadow_tint[2] = 1.0f;
		level.light.toon_specular = 0.4f;
		level.light.outline_width = 0.02f;
		level.light.outline_color[0] = 0.1f;
		level.light.outline_color[1] = 0.2f;
		level.light.outline_color[2] = 0.3f;

		//两盏点光
		level.light.point_count = 2;
		level.light.point_position[0][0] = 1.0f;
		level.light.point_position[0][1] = 2.0f;
		level.light.point_position[0][2] = 3.0f;
		level.light.point_color[0][0] = 1.0f;
		level.light.point_color[0][1] = 0.5f;
		level.light.point_color[0][2] = 0.25f;
		level.light.point_radius[0] = 4.0f;
		level.light.point_intensity[0] = 2.0f;
		level.light.point_attenuation[0] = 2;
		level.light.point_specular[0] = 0.5f;
		level.light.point_shininess[0] = 10.0f;
		level.light.point_position[1][0] = -1.0f;
		level.light.point_position[1][1] = -2.0f;
		level.light.point_position[1][2] = -3.0f;
		level.light.point_color[1][0] = 0.2f;
		level.light.point_color[1][1] = 0.3f;
		level.light.point_color[1][2] = 0.4f;
		level.light.point_radius[1] = 6.0f;
		level.light.point_intensity[1] = 0.5f;
		level.light.point_attenuation[1] = 0;
		level.light.point_specular[1] = 1.0f;
		level.light.point_shininess[1] = 0.0f;

		//一盏聚光
		level.light.spot_count = 1;
		level.light.spot_position[0][0] = 0.0f;
		level.light.spot_position[0][1] = 5.0f;
		level.light.spot_position[0][2] = 0.0f;
		level.light.spot_direction[0][0] = 0.0f;
		level.light.spot_direction[0][1] = -1.0f;
		level.light.spot_direction[0][2] = 0.0f;
		level.light.spot_color[0][0] = 1.0f;
		level.light.spot_color[0][1] = 1.0f;
		level.light.spot_color[0][2] = 0.9f;
		level.light.spot_radius[0] = 8.0f;
		level.light.spot_intensity[0] = 1.5f;
		level.light.spot_inner_deg[0] = 12.0f;
		level.light.spot_outer_deg[0] = 25.0f;
		level.light.spot_attenuation[0] = 1;
		level.light.spot_specular[0] = 0.7f;
		level.light.spot_shininess[0] = 100.0f;

		//雾
		level.fog.color[0] = 0.4f;
		level.fog.color[1] = 0.5f;
		level.fog.color[2] = 0.6f;
		level.fog.density = 0.35f;
		level.fog.start = 2.0f;
		level.fog.end = 40.0f;
		level.fog.mode = 1;
		level.fog.height_falloff = 0.12f;

		//资产段落
		level.asset_lines.push_back("mesh 甲 box.obj");
		level.asset_lines.push_back("mtl 乙 甲.mtl");
		level.asset_lines.push_back("place 甲 乙 0 0 0 1 1 1");
		return level;
	}
}

/*
关卡文本往返（第四百二十九轮）
口径：写出后再读回，全字段（含浮点）逐位相等。
*/
TEST(Renderer_Level_Test, 关卡文本往返逐位一致)
{
	const engine::Level_Data source = rich_level();
	ASSERT_TRUE(engine::level_text_write(source, level_utf8("往返.level")));

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("往返.level"), loaded));
	expect_level_equal(source, loaded);
	std::cout << "[关卡] 往返：点光 " << loaded.light.point_count
		<< " 盏 / 聚光 " << loaded.light.spot_count
		<< " 盏 / 资产行 " << loaded.asset_lines.size() << std::endl;
}

/*
缺省补齐（第四百二十九轮）
口径：只写关卡名与相机位置，其余字段等于结构默认值。
*/
TEST(Renderer_Level_Test, 关卡缺省补齐)
{
	write_text(level_dir() / L"极简.level", "level 极简关卡\ncamera position 1 2 3\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("极简.level"), loaded));
	EXPECT_EQ(loaded.name, "极简关卡");
	EXPECT_EQ(loaded.camera.position[0], 1.0f);
	EXPECT_EQ(loaded.camera.position[1], 2.0f);
	EXPECT_EQ(loaded.camera.position[2], 3.0f);
	//相机其余字段
	EXPECT_EQ(loaded.camera.target[2], 0.0f);
	EXPECT_EQ(loaded.camera.fov, 1.0472f);
	EXPECT_EQ(loaded.camera.near_plane, 0.1f);
	EXPECT_EQ(loaded.camera.far_plane, 100.0f);
	EXPECT_FALSE(loaded.camera.ortho);
	EXPECT_EQ(loaded.camera.clear_color[0], 0.05f);
	//光照与雾默认
	EXPECT_EQ(loaded.light.intensity, 1.0f);
	EXPECT_EQ(loaded.light.direction[2], -1.0f);
	EXPECT_EQ(loaded.light.point_count, 0u);
	EXPECT_EQ(loaded.light.spot_count, 0u);
	EXPECT_EQ(loaded.fog.density, 0.0f);
	EXPECT_EQ(loaded.fog.end, 10.0f);
	EXPECT_TRUE(loaded.asset_lines.empty());
}

/*
非法文本拒绝（第四百二十九轮）
口径：任何一条非法记录 → 读取失败且输出清空（名称空、相机位置回落默认 0,0,3）。
*/
TEST(Renderer_Level_Test, 关卡非法被拒绝)
{
	//点光超限：9 盏
	std::string too_many_points = "level 超限\n";
	for (uint32_t index = 0; index < 9; ++index)
		too_many_points += "light point 0 0 0 1 1 1 5 1 0 1 0\n";

	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "未知段", "level 测试\nunknown_section 1 2 3\n" },
		{ "未知相机键", "level 测试\ncamera zoom 2\n" },
		{ "相机位置字段数不符", "level 测试\ncamera position 1 2\n" },
		{ "非有限值", "level 测试\ncamera fov nan\n" },
		{ "正交开关非 0/1", "level 测试\ncamera ortho 2\n" },
		{ "资产行缺路径", "level 测试\nmesh 甲\n" },
		{ "点光超 8 盏", too_many_points },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		//失败契约：输出清空
		EXPECT_TRUE(loaded.name.empty()) << item.first;
		EXPECT_EQ(loaded.camera.position[0], 0.0f) << item.first;
		EXPECT_EQ(loaded.camera.position[2], 3.0f) << item.first;
		EXPECT_EQ(loaded.light.point_count, 0u) << item.first;
	}
}

/*
资产声明行原样保留（第四百二十九轮）
口径：注释与空行被忽略；mesh / mtl / place 行按去首尾空白后的原文保存；写出再读回仍一致。
*/
TEST(Renderer_Level_Test, 关卡资产行原样保留)
{
	const std::string text =
		"# 关卡资产段落\n"
		"level 资产关卡\n"
		"\n"
		"mesh 甲  box.obj\n"
		"mtl 乙  甲.mtl\n"
		"place 甲 乙 0 1 2 1 1 1\n";

	write_text(level_dir() / L"资产.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("资产.level"), loaded));
	ASSERT_EQ(loaded.asset_lines.size(), 3u);
	EXPECT_EQ(loaded.asset_lines[0], "mesh 甲  box.obj");
	EXPECT_EQ(loaded.asset_lines[1], "mtl 乙  甲.mtl");
	EXPECT_EQ(loaded.asset_lines[2], "place 甲 乙 0 1 2 1 1 1");

	//写出再读回：资产行逐字相同
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("资产_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("资产_回写.level"), reread));
	ASSERT_EQ(reread.asset_lines.size(), 3u);
	for (std::size_t index = 0; index < reread.asset_lines.size(); ++index)
		EXPECT_EQ(reread.asset_lines[index], loaded.asset_lines[index]);
}

/*
材质实例行往返（第四百六十六轮；方向七阶段 B）
口径：`material` 行结构化保存、不进入 asset_lines；写出再读回逐字段（含浮点）逐位相等。
*/
TEST(Renderer_Level_Test, 关卡材质实例行往返)
{
	const std::string text =
		"level 实例关卡\n"
		"material 红 甲 tint 0.9 0.2 0.2 1\n"
		"material 亮 乙 tint 0.5 0.5 0.5 1 emissive 0.35\n"
		"mesh 甲 box.obj\n"
		"mtl 乙 甲.mtl\n"
		"place 甲 红 0 1 2 1 1 1\n";

	write_text(level_dir() / L"实例.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例.level"), loaded));
	//结构化：两条实例；asset_lines 仍只有 mesh / mtl / place 三行
	ASSERT_EQ(loaded.materials.size(), 2u);
	ASSERT_EQ(loaded.asset_lines.size(), 3u);
	//第一条：仅覆盖底色
	EXPECT_EQ(loaded.materials[0].实例名, "红");
	EXPECT_EQ(loaded.materials[0].模板, "甲");
	EXPECT_TRUE(loaded.materials[0].use_tint);
	EXPECT_FALSE(loaded.materials[0].use_emissive);
	EXPECT_TRUE(bit_equal(loaded.materials[0].tint[0], 0.9f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].tint[3], 1.0f));
	//第二条：底色 ＋ 自发光
	EXPECT_TRUE(loaded.materials[1].use_emissive);
	EXPECT_TRUE(bit_equal(loaded.materials[1].emissive, 0.35f));

	//写出再读回：实例逐字段相等
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 2u);
	for (std::size_t material_index = 0; material_index < reread.materials.size(); ++material_index)
	{
		const engine::Level_Material& left = loaded.materials[material_index];
		const engine::Level_Material& right = reread.materials[material_index];
		EXPECT_EQ(left.实例名, right.实例名);
		EXPECT_EQ(left.模板, right.模板);
		EXPECT_EQ(left.use_tint, right.use_tint);
		EXPECT_EQ(left.use_emissive, right.use_emissive);
		EXPECT_TRUE(bit_equal(left.emissive, right.emissive));
		for (std::size_t index = 0; index < 4; ++index)
			EXPECT_TRUE(bit_equal(left.tint[index], right.tint[index]));
	}
}

/*
材质实例缺省不覆盖（第四百六十六轮）
口径：只给实例名与模板名时，两项覆盖均为假，底色全 1、自发光 0。
*/
TEST(Renderer_Level_Test, 关卡材质实例缺省)
{
	write_text(level_dir() / L"实例缺省.level", "level 实例缺省\nmaterial 素 甲\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例缺省.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 1u);
	EXPECT_EQ(loaded.materials[0].实例名, "素");
	EXPECT_EQ(loaded.materials[0].模板, "甲");
	EXPECT_FALSE(loaded.materials[0].use_tint);
	EXPECT_FALSE(loaded.materials[0].use_emissive);
	for (std::size_t index = 0; index < 4; ++index)
		EXPECT_TRUE(bit_equal(loaded.materials[0].tint[index], 1.0f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].emissive, 0.0f));
	EXPECT_TRUE(loaded.asset_lines.empty());
}

/*
材质实例数值越界拒绝（第四百六十六轮）
口径：覆盖值必须有限且在 [0,1]；越界 / NaN / Inf 一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例数值越界)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "底色超上界", "level 越界\nmaterial 甲 乙 tint 1.5 0.2 0.2 1\n" },
		{ "底色为负", "level 越界\nmaterial 甲 乙 tint -0.1 0.2 0.2 1\n" },
		{ "底色 NaN", "level 越界\nmaterial 甲 乙 tint nan 0.2 0.2 1\n" },
		{ "自发光超上界", "level 越界\nmaterial 甲 乙 emissive 1.2\n" },
		{ "自发光为负", "level 越界\nmaterial 甲 乙 emissive -0.2\n" },
		{ "自发光 Inf", "level 越界\nmaterial 甲 乙 emissive inf\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		//失败契约：输出清空
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例结构非法拒绝（第四百六十六轮）
口径：未知覆盖项 / 缺值 / 缺模板名 / 顺序颠倒 / 多余字段一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例结构非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "缺模板名", "level 非法\nmaterial 甲\n" },
		{ "未知覆盖项", "level 非法\nmaterial 甲 乙 gloss 0.5\n" },
		{ "底色缺值", "level 非法\nmaterial 甲 乙 tint 0.2 0.3 0.4\n" },
		{ "顺序颠倒", "level 非法\nmaterial 甲 乙 emissive 0.5 tint 0.2 0.3 0.4 1\n" },
		{ "多余字段", "level 非法\nmaterial 甲 乙 tint 0.2 0.3 0.4 0.5 0.6\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例重名拒绝（第四百六十六轮）
口径：同一关卡内实例名唯一；重复即整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例重名)
{
	const std::string text =
		"level 重名\n"
		"material 甲 乙 tint 0.2 0.2 0.2 1\n"
		"material 甲 丙\n";

	write_text(level_dir() / L"实例重名.level", text);

	engine::Level_Data loaded;
	EXPECT_FALSE(engine::level_text_read(level_utf8("实例重名.level"), loaded));
	EXPECT_TRUE(loaded.materials.empty());
}

/*
材质实例装配拒绝（第四百六十七轮；方向七阶段 B-B）
口径：place 引用未声明实例 / 实例模板名未命中材质库 → 关卡装载整份失败，输出清空。
*/
TEST(Renderer_Level_Test, 关卡材质实例装配拒绝)
{
	write_box_assets();
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "引用未声明实例", "level 拒绝\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\nplace 方盒 蓝 0 0 0 1 1 1\n" },
		{ "模板名未命中", "level 拒绝\nmaterial 红 不存在\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\nplace 方盒 红 0 0 0 1 1 1\n" },
	};

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / L"实例装配.level", item.second);
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		EXPECT_FALSE(renderer.level_load(关卡, 场景, level_utf8("实例装配.level"))) << item.first;
		//失败契约：关卡与场景输出清空
		EXPECT_TRUE(场景.objects.empty()) << item.first;
		EXPECT_TRUE(场景.meshes.empty()) << item.first;
		EXPECT_TRUE(场景.materials.empty()) << item.first;
	}
}

/*
材质实例等价装配（第四百六十七轮；方向七阶段 B-B）
口径：默认覆盖（tint 1/1/1/1、emissive 0）经实例装配，画面与「直接引用材质库」逐字节一致。
*/
TEST(Renderer_Level_Test, 关卡材质实例等价装配)
{
	write_box_assets();
	//直接引用材质库
	write_text(level_dir() / L"实例等价甲.level",
		"level 等价甲\n"
		"camera position 1 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");
	//同模板实例（默认覆盖）
	write_text(level_dir() / L"实例等价乙.level",
		"level 等价乙\n"
		"camera position 1 0 3\n"
		"material 红 甲 tint 1 1 1 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 红 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//空场景基线
	std::vector<uint8_t> reference(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	//甲：直接引用材质库
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(renderer.level_load(甲, 场景甲, level_utf8("实例等价甲.level")));
	ASSERT_EQ(场景甲.objects.size(), 1u);
	EXPECT_TRUE(场景甲.objects[0].material_instances.empty());
	EXPECT_EQ(renderer.scene_draw(场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	renderer.level_unload(场景甲);

	//乙：同模板实例（默认覆盖）
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(renderer.level_load(乙, 场景乙, level_utf8("实例等价乙.level")));
	ASSERT_EQ(场景乙.objects.size(), 1u);
	ASSERT_EQ(场景乙.objects[0].material_instances.size(), 1u);
	//实例模板句柄与绑定句柄一致
	EXPECT_EQ(场景乙.objects[0].material_instances[0].模板, 场景乙.objects[0].materials[0]);
	EXPECT_EQ(renderer.scene_draw(场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	//默认覆盖等价：画面逐字节 0 差异
	EXPECT_EQ(changed_pixels(帧甲, 帧乙), 0u);
	renderer.level_unload(场景乙);
}

/*
材质实例覆盖生效（第四百六十九轮；方向七阶段 C）
口径：同模板不同 tint → 画面差异明显；绘制后 tint / 自发光吸附状态复位（不污染后续绘制项）。
*/
TEST(Renderer_Level_Test, 关卡材质实例覆盖生效)
{
	write_box_assets();
	//实例：白色（默认覆盖）
	write_text(level_dir() / L"覆盖甲.level",
		"level 覆盖甲\n"
		"camera position 1 0 3\n"
		"material 素 甲 tint 1 1 1 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	//实例：深灰覆盖
	write_text(level_dir() / L"覆盖乙.level",
		"level 覆盖乙\n"
		"camera position 1 0 3\n"
		"material 暗 甲 tint 0.2 0.2 0.2 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 暗 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：默认覆盖
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(renderer.level_load(甲, 场景甲, level_utf8("覆盖甲.level")));
	EXPECT_EQ(renderer.scene_draw(场景甲), 1u);
	//绘制后吸附状态复位
	EXPECT_FALSE(renderer.material_tint_active());
	EXPECT_EQ(renderer.material_emissive(), 0.0f);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	renderer.level_unload(场景甲);

	//乙：深灰覆盖
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(renderer.level_load(乙, 场景乙, level_utf8("覆盖乙.level")));
	EXPECT_EQ(renderer.scene_draw(场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	//不同 tint → 明显差异
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例色调差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 200u) << "实例 tint 覆盖未生效";
	renderer.level_unload(场景乙);
}

/*
材质实例自发光生效（第四百六十九轮）
口径：同模板 emissive 0 与 0.6 两个实例 → 画面差异明显。
*/
TEST(Renderer_Level_Test, 关卡材质实例自发光生效)
{
	write_box_assets();
	write_text(level_dir() / L"自发甲.level",
		"level 自发甲\n"
		"camera position 1 0 3\n"
		"material 素 甲 tint 0.2 0.2 0.2 1 emissive 0\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"自发乙.level",
		"level 自发乙\n"
		"camera position 1 0 3\n"
		"material 亮 甲 tint 0.2 0.2 0.2 1 emissive 0.6\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 亮 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：emissive 0
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(renderer.level_load(甲, 场景甲, level_utf8("自发甲.level")));
	EXPECT_EQ(renderer.scene_draw(场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	renderer.level_unload(场景甲);

	//乙：emissive 0.6
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(renderer.level_load(乙, 场景乙, level_utf8("自发乙.level")));
	EXPECT_EQ(renderer.scene_draw(场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例自发光差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "实例 emissive 覆盖未生效";
	renderer.level_unload(场景乙);
}

/*
材质实例卡通覆盖往返（第四百七十一轮；方向七延伸）
口径：`toon` 三值结构化保存并随写出一并回读，字段逐位相等。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通往返)
{
	const std::string text =
		"level 卡通关卡\n"
		"material 卡通 甲 tint 0.8 0.8 0.8 1 emissive 0.1 toon 4 0.5 0.3\n"
		"mesh 甲 box.obj\n"
		"mtl 乙 甲.mtl\n"
		"place 甲 卡通 0 1 2 1 1 1\n";

	write_text(level_dir() / L"实例卡通.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例卡通.level"), loaded));
	ASSERT_EQ(loaded.materials.size(), 1u);
	ASSERT_EQ(loaded.asset_lines.size(), 3u);
	EXPECT_TRUE(loaded.materials[0].use_toon);
	EXPECT_TRUE(bit_equal(loaded.materials[0].toon_steps, 4.0f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].toon_rim, 0.5f));
	EXPECT_TRUE(bit_equal(loaded.materials[0].toon_specular, 0.3f));

	//写出再读回
	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("实例卡通_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("实例卡通_回写.level"), reread));
	ASSERT_EQ(reread.materials.size(), 1u);
	EXPECT_TRUE(reread.materials[0].use_toon);
	EXPECT_TRUE(bit_equal(reread.materials[0].toon_steps, loaded.materials[0].toon_steps));
	EXPECT_TRUE(bit_equal(reread.materials[0].toon_rim, loaded.materials[0].toon_rim));
	EXPECT_TRUE(bit_equal(reread.materials[0].toon_specular, loaded.materials[0].toon_specular));
}

/*
材质实例卡通数值越界（第四百七十一轮）
口径：色阶 0..8、边缘光 / 高光 0..1；越界 / NaN 一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通越界)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "色阶超上限", "level 越界\nmaterial 甲 乙 toon 9 0.5 0.3\n" },
		{ "边缘光越界", "level 越界\nmaterial 甲 乙 toon 4 1.5 0.3\n" },
		{ "高光为负", "level 越界\nmaterial 甲 乙 toon 4 0.5 -0.1\n" },
		{ "色阶 NaN", "level 越界\nmaterial 甲 乙 toon nan 0.5 0.3\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例卡通结构非法（第四百七十一轮）
口径：toon 缺值 / 多余字段 / toon 后再出现 tint 或 emissive → 整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通结构非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "toon 缺值", "level 非法\nmaterial 甲 乙 toon 4 0.5\n" },
		{ "toon 多余字段", "level 非法\nmaterial 甲 乙 toon 4 0.5 0.3 0.9\n" },
		{ "toon 后再 tint", "level 非法\nmaterial 甲 乙 toon 4 0.5 0.3 tint 1 1 1 1\n" },
		{ "toon 后再 emissive", "level 非法\nmaterial 甲 乙 toon 4 0.5 0.3 emissive 0.2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "实例非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("实例非法.level"), loaded)) << item.first;
		EXPECT_TRUE(loaded.materials.empty()) << item.first;
	}
}

/*
材质实例卡通生效（第四百七十二轮；方向七延伸）
口径：同模板「无卡通」与「toon 1 / 边缘光 1.0 / 高光 0.5」两个实例 → 画面差异明显。
*/
TEST(Renderer_Level_Test, 关卡材质实例卡通生效)
{
	write_cube_assets();
	write_text(level_dir() / L"卡通甲.level",
		"level 卡通甲\n"
		"camera position 1 0 3\n"
		"material 素 甲 tint 1 1 1 1\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 素 0 0 0 1 1 1\n");
	write_text(level_dir() / L"卡通乙.level",
		"level 卡通乙\n"
		"camera position 1 0 3\n"
		"material 纹 甲 tint 1 1 1 1 toon 1 1.0 0.5\n"
		"mesh 方盒 cube.obj\n"
		"mtl 方盒材质 cube.mtl\n"
		"place 方盒 纹 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//甲：无卡通
	engine::Level_Data 甲;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(renderer.level_load(甲, 场景甲, level_utf8("卡通甲.level")));
	EXPECT_EQ(renderer.scene_draw(场景甲), 1u);
	std::vector<uint8_t> 帧甲(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧甲.data(), static_cast<uint32_t>(帧甲.size())));
	renderer.level_unload(场景甲);

	//乙：实例卡通
	engine::Level_Data 乙;
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(renderer.level_load(乙, 场景乙, level_utf8("卡通乙.level")));
	EXPECT_EQ(renderer.scene_draw(场景乙), 1u);
	std::vector<uint8_t> 帧乙(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(帧乙.data(), static_cast<uint32_t>(帧乙.size())));
	const uint32_t 差异 = changed_pixels(帧甲, 帧乙);
	std::cout << "[关卡] 实例卡通差异 " << 差异 << std::endl;
	//立方体各面照明接近饱和，色阶量化差异有限；可观测差异主要来自边缘光（阈值取 10）
	EXPECT_GT(差异, 10u) << "实例 toon 覆盖未生效";
	renderer.level_unload(场景乙);
}

/*
聚光灯阴影（第四百七十四轮；方案 A）
口径：第 1 盏聚光灯从上方照地面，遮挡立方体开启阴影后在地面投下暗区；开启前后差异 > 200 像素。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影)
{
	write_cube_assets();
	//地面（压扁立方体）＋ 遮挡体
	write_text(level_dir() / L"聚光阴影.level",
		"level 聚光阴影\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光阴影.level"), false));

	//关闭阴影：基线
	engine::Light_Data 灯光 = 关卡.light;
	ASSERT_EQ(灯光.spot_count, 1u);
	灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//开启聚光灯阴影
	灯光.spot_shadow_enabled = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(有影.data(), static_cast<uint32_t>(有影.size())));

	const uint32_t 差异 = changed_pixels(无影, 有影);
	std::cout << "[关卡] 聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 200u) << "聚光灯阴影未生效";
	renderer.level_unload(场景);
}

/*
聚光灯阴影强度独立（第四百七十六轮）
口径：strength = 0 时与未开阴影逐像素等价（≤ 2）；strength = 1 时恢复遮蔽（> 200 像素差异）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影强度)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光强度.level",
		"level 聚光强度\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光强度.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	//基线：未开聚光阴影
	灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//强度 0：应等价于未开阴影
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_strength = 0.0f;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 零强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(零强度.data(), static_cast<uint32_t>(零强度.size())));
	const uint32_t 零差异 = changed_pixels(无影, 零强度);
	std::cout << "[关卡] 聚光阴影零强度差异 " << 零差异 << std::endl;
	EXPECT_LE(零差异, 2u) << "强度 0 未完全关闭阴影";

	//强度 1：恢复遮蔽
	灯光.spot_shadow_strength = 1.0f;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 全强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(全强度.data(), static_cast<uint32_t>(全强度.size())));
	const uint32_t 全差异 = changed_pixels(无影, 全强度);
	std::cout << "[关卡] 聚光阴影全强度差异 " << 全差异 << std::endl;
	EXPECT_GT(全差异, 200u) << "强度 1 未产生遮蔽";
	renderer.level_unload(场景);
}

/*
聚光灯阴影软度独立（第四百七十六轮）
口径：硬边（软度 0）与软边（软度 0.75 → 5×5 PCF）在阴影边缘产生可观测差异（> 10 像素）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影软度)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光软度.level",
		"level 聚光软度\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光软度.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	//硬边
	灯光.spot_shadow_softness = 0.0f;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 硬边(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(硬边.data(), static_cast<uint32_t>(硬边.size())));

	//软边（0.75 → 5×5 邻域）
	灯光.spot_shadow_softness = 0.75f;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 软边(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(软边.data(), static_cast<uint32_t>(软边.size())));

	const uint32_t 差异 = changed_pixels(硬边, 软边);
	std::cout << "[关卡] 聚光阴影软度差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 10u) << "聚光灯软度未生效";
	renderer.level_unload(场景);
}

/*
聚光灯阴影序号（第四百七十九轮）
口径：两盏聚光各照一块地面与遮挡体；序号 0 与序号 1 只有一处被遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影序号)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光序号.level",
		"level 聚光序号\n"
		"camera position 0 6 7\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光序号.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 2u);

	//序号 0：只遮蔽左灯
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 4u);
	std::vector<uint8_t> 左影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(左影.data(), static_cast<uint32_t>(左影.size())));

	//序号 1：只遮蔽右灯
	灯光.spot_shadow_index = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 4u);
	std::vector<uint8_t> 右影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(右影.data(), static_cast<uint32_t>(右影.size())));

	const uint32_t 差异 = changed_pixels(左影, 右影);
	std::cout << "[关卡] 聚光阴影序号差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "聚光灯阴影序号未生效";
	renderer.level_unload(场景);
}

/*
聚光灯阴影序号越界（第四百七十九轮）
口径：只登记 2 盏聚光时序号 5 越界 → 按无聚光阴影处理，与关闭阴影的画面等价（≤ 2 像素）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影序号越界)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光越界.level",
		"level 聚光越界\n"
		"camera position 0 6 7\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光越界.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	//基线：关闭聚光阴影
	灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 4u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//开启但序号越界
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 5;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 4u);
	std::vector<uint8_t> 越界(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(越界.data(), static_cast<uint32_t>(越界.size())));

	const uint32_t 差异 = changed_pixels(无影, 越界);
	std::cout << "[关卡] 聚光阴影越界差异 " << 差异 << std::endl;
	EXPECT_LE(差异, 2u) << "越界序号仍产生阴影";
	renderer.level_unload(场景);
}

/*
方向光与聚光阴影共存（第四百八十四轮；方案 B）
口径：方向光阴影开启时再开聚光阴影 → 画面继续变化（> 50 像素），证明两者互不覆盖、同时生效。
*/
TEST(Renderer_Level_Test, 关卡聚光与方向光阴影共存)
{
	write_cube_assets();
	write_text(level_dir() / L"共存阴影.level",
		"level 共存阴影\n"
		"camera position 0 6 8\n"
		"camera target 0 0 0\n"
		"light direction 0.3 -1 0.2\n"
		"light color 1 1 1\n"
		"light intensity 0.5\n"
		"light shadow 1 0.004 0 0.8 1024\n"
		"light spot 2 4 0 0 -1 0 1 1 1 10 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("共存阴影.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	//基线：两套阴影都关
	灯光.shadow_enabled = 0;
	灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//仅方向光阴影
	灯光.shadow_enabled = 1;
	灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 方向影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(方向影.data(), static_cast<uint32_t>(方向影.size())));
	const uint32_t 方向差异 = changed_pixels(无影, 方向影);
	std::cout << "[关卡] 方向光阴影差异 " << 方向差异 << std::endl;
	EXPECT_GT(方向差异, 100u) << "方向光阴影未生效";

	//方向光 ＋ 聚光阴影
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 2u);
	std::vector<uint8_t> 双影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(双影.data(), static_cast<uint32_t>(双影.size())));
	const uint32_t 共存差异 = changed_pixels(方向影, 双影);
	std::cout << "[关卡] 聚光叠加方向光差异 " << 共存差异 << std::endl;
	EXPECT_GT(共存差异, 50u) << "聚光阴影未与方向光阴影共存";
	renderer.level_unload(场景);
}

/*
聚光灯阴影按需重绘（第四百八十六轮）
口径：静态场景连续渲染时阴影 pass 只重绘首帧；移动聚光灯后再重绘一次（签名含聚光矩阵）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影按需重绘)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光重绘.level",
		"level 聚光重绘\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光重绘.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	ASSERT_TRUE(renderer.frame_render());

	//静态连续渲染：阴影 pass 不应再增长
	const uint64_t 静态后 = renderer.shadow_pass_count();
	for (int frame = 0; frame < 4; ++frame)
	{
		ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
		ASSERT_TRUE(renderer.frame_render());
	}
	EXPECT_EQ(renderer.shadow_pass_count(), 静态后) << "静态场景仍重绘阴影";

	//移动聚光灯位置：签名变化 → 重绘一次
	灯光.spot_position[0][0] = 1.0f;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.shadow_pass_count(), 静态后) << "聚光灯移动后未重绘阴影";
	renderer.level_unload(场景);
}

/*
聚光灯阴影分辨率切换（第四百八十七轮）
口径：1024 → 512 → 1024 三次渲染均成功；回到 1024 后画面与首次 1024 逐字节一致（≤ 2）。
*/
TEST(Renderer_Level_Test, 关卡聚光阴影分辨率切换)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光分辨率.level",
		"level 聚光分辨率\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光分辨率.level"), false));
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	std::vector<uint8_t> 原图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(原图.data(), static_cast<uint32_t>(原图.size())));

	//切到 512 再切回 1024
	renderer.shadow_resolution_set(512);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	std::vector<uint8_t> 低分辨率(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(低分辨率.data(), static_cast<uint32_t>(低分辨率.size())));
	renderer.shadow_resolution_set(1024);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	std::vector<uint8_t> 复原(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(复原.data(), static_cast<uint32_t>(复原.size())));

	const uint32_t 差异 = changed_pixels(原图, 复原);
	std::cout << "[关卡] 分辨率往返差异 " << 差异 << std::endl;
	EXPECT_LE(差异, 2u) << "分辨率往返后画面未复原";
	EXPECT_GE(changed_pixels(原图, 低分辨率), 1u) << "512 分辨率未改变阴影细节";
	renderer.level_unload(场景);
}

/*
Cluster 路径聚光阴影（第四百八十八轮）
口径：开启 Cluster 后聚光阴影仍生效（同一场景，开启/关闭阴影的画面差异 > 100 像素）。
*/
TEST(Renderer_Level_Test, 关卡Cluster聚光阴影)
{
	write_cube_assets();
	write_text(level_dir() / L"聚光簇.level",
		"level 聚光簇\n"
		"camera position 0 6 6\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot 0 4 0 0 -1 0 1 1 1 12 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 8 0.2 8\n"
		"place 方块 方块材质 0 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	if (!renderer.cluster_supported())
		GTEST_SKIP() << "本机不支持 Cluster 计算路径";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("聚光簇.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	//开启 Cluster（默认网格）
	renderer.cluster_enable_set(true);

	//关闭阴影（Cluster 路径）
	灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//开启阴影（Cluster 路径）
	灯光.spot_shadow_enabled = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	ASSERT_TRUE(renderer.scene_draw(场景) == 2u);
	std::vector<uint8_t> 有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(有影.data(), static_cast<uint32_t>(有影.size())));

	const uint32_t 差异 = changed_pixels(无影, 有影);
	std::cout << "[关卡] Cluster 聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "Cluster 路径聚光阴影未生效";
	renderer.cluster_enable_set(false);
	renderer.level_unload(场景);
}

/*
聚光灯阴影开关文本往返（第四百七十五轮）
口径：`light spot_shadow 0/1` 结构化保存；写出再读回一致。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影开关往返)
{
	write_text(level_dir() / L"聚光开关.level", "level 聚光开关\nlight spot_shadow 1\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光开关.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_enabled, 1);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("聚光开关_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光开关_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_enabled, 1);
}

/*
聚光灯阴影开关非法（第四百七十五轮）
口径：只接受 0 / 1，且不得有多余字段；越界 / 缺值 / 非数字一律拒绝。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影开关非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "开关为 2", "level 非法\nlight spot_shadow 2\n" },
		{ "缺值", "level 非法\nlight spot_shadow\n" },
		{ "非数字", "level 非法\nlight spot_shadow on\n" },
		{ "多余字段", "level 非法\nlight spot_shadow 1 2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_enabled, 0) << item.first;
	}
}

/*
聚光灯阴影质量参数往返（第四百七十八轮）
口径：`light spot_shadow 1 0.02 0.5 0.3` 读入三项并写出后再读回一致；仅写开关时三项保持默认。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影质量参数往返)
{
	write_text(level_dir() / L"聚光质量.level", "level 聚光质量\nlight spot_shadow 1 0.02 0.5 0.3\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光质量.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_enabled, 1);
	EXPECT_FLOAT_EQ(loaded.light.spot_shadow_bias, 0.02f);
	EXPECT_FLOAT_EQ(loaded.light.spot_shadow_softness, 0.5f);
	EXPECT_FLOAT_EQ(loaded.light.spot_shadow_strength, 0.3f);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("聚光质量_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光质量_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_enabled, 1);
	EXPECT_FLOAT_EQ(reread.light.spot_shadow_bias, 0.02f);
	EXPECT_FLOAT_EQ(reread.light.spot_shadow_softness, 0.5f);
	EXPECT_FLOAT_EQ(reread.light.spot_shadow_strength, 0.3f);

	//仅写开关：三项沿用默认（0.010 / 0 / 1）
	write_text(level_dir() / L"聚光默认.level", "level 聚光默认\nlight spot_shadow 1\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光默认.level"), 缺省));
	EXPECT_FLOAT_EQ(缺省.light.spot_shadow_bias, 0.010f);
	EXPECT_FLOAT_EQ(缺省.light.spot_shadow_softness, 0.0f);
	EXPECT_FLOAT_EQ(缺省.light.spot_shadow_strength, 1.0f);
}

/*
聚光灯阴影质量参数非法（第四百七十八轮）
口径：偏置 0..0.05、软度 0..1、强度 0..1；只写一两项 / 多余字段 / 越界一律整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影质量参数非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "偏置越界", "level 非法\nlight spot_shadow 1 0.06 0.5 0.3\n" },
		{ "软度越界", "level 非法\nlight spot_shadow 1 0.01 1.2 0.3\n" },
		{ "强度为负", "level 非法\nlight spot_shadow 1 0.01 0.5 -0.1\n" },
		{ "只写两项", "level 非法\nlight spot_shadow 1 0.01 0.5\n" },
		{ "多余字段", "level 非法\nlight spot_shadow 1 0.01 0.5 0.3 9\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_enabled, 0) << item.first;
		EXPECT_FLOAT_EQ(loaded.light.spot_shadow_bias, 0.010f) << item.first;
	}
}

/*
聚光灯阴影序号文本往返（第四百八十一轮）
口径：`light spot_shadow_index 1` 结构化保存；缺省为 0；写出再读回一致。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影序号往返)
{
	write_text(level_dir() / L"聚光序号文本.level", "level 聚光序号文本\nlight spot_shadow_index 1\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光序号文本.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_index, 1);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("聚光序号文本_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光序号文本_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_index, 1);

	//缺省：不写该行时序号为 0
	write_text(level_dir() / L"聚光序号缺省.level", "level 聚光序号缺省\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("聚光序号缺省.level"), 缺省));
	EXPECT_EQ(缺省.light.spot_shadow_index, 0);
}

/*
聚光灯阴影序号非法（第四百八十一轮）
口径：只接受 0..7 的十进制整数，且不得有多余字段。
*/
TEST(Renderer_Level_Test, 关卡聚光灯阴影序号非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "序号为 8", "level 非法\nlight spot_shadow_index 8\n" },
		{ "负数", "level 非法\nlight spot_shadow_index -1\n" },
		{ "非数字", "level 非法\nlight spot_shadow_index on\n" },
		{ "缺值", "level 非法\nlight spot_shadow_index\n" },
		{ "多余字段", "level 非法\nlight spot_shadow_index 1 2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_index, 0) << item.first;
	}
}

/*
多盏聚光灯阴影（第五百零七轮）
口径：两盏聚光各照一块地面与遮挡体；单盏（序号 0）只遮蔽左侧，多盏开启后两盏各自遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光阴影)
{
	write_cube_assets();
	write_text(level_dir() / L"多盏聚光.level",
		"level 多盏聚光\n"
		"camera position 0 6 7\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.1 0.1 0.1\n"
		"light intensity 0.1\n"
		"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 -3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 3 -1.5 0 4 0.2 4\n"
		"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("多盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 2u);

	//单盏（序号 0）：只遮蔽左侧
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 0;
		灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 4u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));

	//多盏：两盏各自投影
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(renderer.scene_draw(场景), 4u);
	std::vector<uint8_t> 多盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(多盏.data(), static_cast<uint32_t>(多盏.size())));

	const uint32_t 差异 = changed_pixels(单盏, 多盏);
	std::cout << "[关卡] 多盏聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "多盏聚光灯阴影未生效";
	renderer.level_unload(场景);
}

/*
多盏聚光灯开关文本往返（第五百零七轮）
口径：`light spot_shadow_all 1` 结构化保存；缺省为 0；写出再读回一致。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光开关往返)
{
	write_text(level_dir() / L"多盏开关.level", "level 多盏开关\nlight spot_shadow_all 1\n");

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("多盏开关.level"), loaded));
	EXPECT_EQ(loaded.light.spot_shadow_all, 1);

	ASSERT_TRUE(engine::level_text_write(loaded, level_utf8("多盏开关_回写.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("多盏开关_回写.level"), reread));
	EXPECT_EQ(reread.light.spot_shadow_all, 1);

	//缺省：不写该行时为 0
	write_text(level_dir() / L"多盏开关缺省.level", "level 多盏开关缺省\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("多盏开关缺省.level"), 缺省));
	EXPECT_EQ(缺省.light.spot_shadow_all, 0);
}

/*
多盏聚光灯开关非法（第五百零七轮）
口径：只接受 0 / 1，且不得有多余字段。
*/
TEST(Renderer_Level_Test, 关卡多盏聚光开关非法)
{
	const std::vector<std::pair<std::string, std::string>> cases = {
		{ "开关为 2", "level 非法\nlight spot_shadow_all 2\n" },
		{ "负数", "level 非法\nlight spot_shadow_all -1\n" },
		{ "非数字", "level 非法\nlight spot_shadow_all on\n" },
		{ "缺值", "level 非法\nlight spot_shadow_all\n" },
		{ "多余字段", "level 非法\nlight spot_shadow_all 1 2\n" },
	};

	for (const std::pair<std::string, std::string>& item : cases)
	{
		write_text(level_dir() / "非法.level", item.second);
		engine::Level_Data loaded;
		EXPECT_FALSE(engine::level_text_read(level_utf8("非法.level"), loaded)) << item.first;
		EXPECT_EQ(loaded.light.spot_shadow_all, 0) << item.first;
	}
}

/*
写入与读取失败路径（第四百二十九轮）
口径：空路径 / 不存在的文件 → 返回假；读取失败时输出清空。
*/
TEST(Renderer_Level_Test, 关卡读写失败路径)
{
	engine::Level_Data loaded = rich_level();
	EXPECT_FALSE(engine::level_text_read("", loaded));
	EXPECT_TRUE(loaded.name.empty());

	EXPECT_FALSE(engine::level_text_read(level_utf8("不存在.level"), loaded));
	EXPECT_TRUE(loaded.name.empty());

	EXPECT_FALSE(engine::level_text_write(loaded, ""));
}

/*
关卡装载端到端（第四百三十轮 · 阶段 B）
口径：关卡文件内嵌资产段落 → `level_load` 建立 1 网格 / 1 材质 / 1 对象；绘制后覆盖达标；
`level_save` 可把装载结果原样写回并可再读；`level_unload` 幂等释放。
*/
TEST(Renderer_Level_Test, 关卡装载端到端)
{
	write_box_assets();
	write_text(level_dir() / L"端到端.level",
		"level 端到端关卡\n"
		"camera position 0 0 3\n"
		"camera target 0 0 0\n"
		"light direction 0 0 -1\n"
		"light color 1 1 1\n"
		"light intensity 1\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//基线（空场景）
	std::vector<uint8_t> reference(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("端到端.level")));
	EXPECT_EQ(关卡.name, "端到端关卡");
	EXPECT_EQ(关卡.asset_lines.size(), 3u);
	ASSERT_EQ(场景.meshes.size(), 1u);
	ASSERT_EQ(场景.materials.size(), 1u);
	ASSERT_EQ(场景.objects.size(), 1u);
	ASSERT_EQ(场景.objects[0].materials.size(), 1u);

	//逐段绑材质绘制（门面 scene_draw）
	EXPECT_EQ(renderer.scene_draw(场景), 1u);
	std::vector<uint8_t> shown(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
	const uint32_t covered = changed_pixels(reference, shown);
	std::cout << "[关卡] 端到端覆盖 " << covered << std::endl;
	EXPECT_GE(covered, LEVEL_MIN_PIXELS) << "关卡装载的网格没有画出来";

	//门面保存：写回后再读，名称与资产行数量一致
	ASSERT_TRUE(renderer.level_save(关卡, level_utf8("端到端_保存.level")));
	engine::Level_Data reread;
	ASSERT_TRUE(engine::level_text_read(level_utf8("端到端_保存.level"), reread));
	EXPECT_EQ(reread.name, 关卡.name);
	EXPECT_EQ(reread.asset_lines.size(), 关卡.asset_lines.size());

	//幂等释放
	renderer.level_unload(场景);
	EXPECT_TRUE(场景.objects.empty());
	EXPECT_TRUE(场景.meshes.empty());
}

/*
关卡与手工构造画面一致（第四百三十轮 · 阶段 B）
口径：同一关卡经「level_load」与「手工 scene_apply ＋ scene_load」两条路径出帧，逐字节一致。
*/
TEST(Renderer_Level_Test, 关卡与手工构造画面一致)
{
	write_box_assets();
	//关卡：带相机 / 光照 / 雾与一段资产
	write_text(level_dir() / L"等价.level",
		"level 等价关卡\n"
		"camera position 1 0 3\n"
		"camera target 0 0 0\n"
		"light direction 0.3 -1 0.2\n"
		"light color 1 0.9 0.8\n"
		"light intensity 0.8\n"
		"fog color 0.3 0.35 0.4\n"
		"fog density 0.2\n"
		"fog range 2 20\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");
	//等价清单（仅资产段落）
	write_text(level_dir() / L"等价清单.txt",
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//路径一：关卡装载
	engine::Level_Data 关卡;
	engine::Asset_Scene 场景甲;
	ASSERT_TRUE(renderer.level_load(关卡, 场景甲, level_utf8("等价.level")));
	ASSERT_EQ(场景甲.objects.size(), 1u);
	EXPECT_EQ(renderer.scene_draw(场景甲), 1u);
	std::vector<uint8_t> 关卡帧(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(关卡帧.data(), static_cast<uint32_t>(关卡帧.size())));
	renderer.level_unload(场景甲);

	//路径二：手工三组 ＋ 场景清单
	renderer.scene_apply(关卡.camera, 关卡.light, 关卡.fog);
	engine::Asset_Scene 场景乙;
	ASSERT_TRUE(renderer.scene_load(场景乙, level_utf8("等价清单.txt")));
	ASSERT_EQ(场景乙.objects.size(), 1u);
	ASSERT_TRUE(renderer.mesh_draw_sections(场景乙.objects[0].mesh, nullptr,
		场景乙.objects[0].materials.data(), static_cast<uint32_t>(场景乙.objects[0].materials.size())));
	std::vector<uint8_t> 手工帧(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(手工帧.data(), static_cast<uint32_t>(手工帧.size())));
	renderer.scene_unload(场景乙);

	//逐字节比较
	ASSERT_EQ(关卡帧.size(), 手工帧.size());
	uint32_t 差异字节 = 0;
	for (std::size_t index = 0; index < 关卡帧.size(); ++index)
	{
		if (关卡帧[index] != 手工帧[index])
			++差异字节;
	}
	EXPECT_EQ(差异字节, 0u) << "关卡链路与手工构造画面不一致";
}

/*
关卡装载失败回退（第四百三十轮 · 阶段 B）
口径：资产不存在 / 相机参数非法 → `level_load` 返回假，输出清空且资产不残留。
*/
TEST(Renderer_Level_Test, 关卡装载失败回退)
{
	//资产不存在
	write_text(level_dir() / L"缺失资产.level",
		"level 缺失资产\n"
		"mesh 方盒 不存在.obj\n"
		"place 方盒 0 0 0 1 1 1\n");
	//相机非法（视场角 0）
	write_text(level_dir() / L"非法相机.level", "level 非法相机\ncamera fov 0\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	EXPECT_FALSE(renderer.level_load(关卡, 场景, level_utf8("缺失资产.level")));
	EXPECT_TRUE(关卡.name.empty());
	EXPECT_TRUE(场景.meshes.empty());
	EXPECT_TRUE(场景.materials.empty());
	EXPECT_TRUE(场景.objects.empty());

	EXPECT_FALSE(renderer.level_load(关卡, 场景, level_utf8("非法相机.level")));
	EXPECT_TRUE(关卡.name.empty());
	EXPECT_TRUE(场景.objects.empty());
}

/*
变换矩阵构造（第四百三十一轮 · 阶段 C）
口径：旋转全零 → 对角缩放 ＋ 末列平移；绕 Z 90° → 局部 (1,0,0) 变为 (0,1,0)（缩放 2 倍、平移 (1,2,3)）。
*/
TEST(Renderer_Level_Test, 变换矩阵构造)
{
	//单位旋转：对角缩放 ＋ 平移
	{
		const float position[3] = { 1.0f, 2.0f, 3.0f };
		const float rotation[3] = { 0.0f, 0.0f, 0.0f };
		const float scale[3] = { 2.0f, 3.0f, 4.0f };
		float matrix[16] = {};
		engine::level_transform_build(position, rotation, scale, matrix);
		EXPECT_FLOAT_EQ(matrix[0], 2.0f);
		EXPECT_FLOAT_EQ(matrix[5], 3.0f);
		EXPECT_FLOAT_EQ(matrix[10], 4.0f);
		EXPECT_FLOAT_EQ(matrix[12], 1.0f);
		EXPECT_FLOAT_EQ(matrix[13], 2.0f);
		EXPECT_FLOAT_EQ(matrix[14], 3.0f);
		EXPECT_FLOAT_EQ(matrix[15], 1.0f);
		EXPECT_FLOAT_EQ(matrix[1], 0.0f);
	}
	//绕 Z 轴 90°（列主序；局部 +X 转到 +Y）
	{
		const float position[3] = { 1.0f, 2.0f, 3.0f };
		const float rotation[3] = { 0.0f, 0.0f, 90.0f };
		const float scale[3] = { 2.0f, 2.0f, 2.0f };
		float matrix[16] = {};
		engine::level_transform_build(position, rotation, scale, matrix);
		//列主序矩阵作用于局部点 (1,0,0)：结果应为缩放 2 倍后旋转到 +Y，再平移
		const float local[3] = { 1.0f, 0.0f, 0.0f };
		float world[3] = {};
		for (std::size_t row = 0; row < 3; ++row)
			world[row] = matrix[row] * local[0] + matrix[4 + row] * local[1]
				+ matrix[8 + row] * local[2] + matrix[12 + row];
		EXPECT_NEAR(world[0], 1.0f, 1e-6f);
		EXPECT_NEAR(world[1], 4.0f, 1e-6f);
		EXPECT_NEAR(world[2], 3.0f, 1e-6f);
	}
}

/*
摆放旋转（第四百三十一轮 · 阶段 C）
口径：`place` 行带旋转三轴 → 对象 `rotation_deg` 生效；零旋转用构造矩阵绘制与旧（nullptr）画面逐字节一致；
绕 Y 45° 后画面变化。
*/
TEST(Renderer_Level_Test, 摆放旋转)
{
	write_box_assets();
	//零旋转关卡
	write_text(level_dir() / L"旋转零.level",
		"level 零旋转\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1 0 0 0\n");
	//绕 Y 45°
	write_text(level_dir() / L"旋转四十五.level",
		"level 旋转四十五\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1 0 45 0\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//零旋转：对象字段为 0；用构造矩阵绘制与 nullptr 画面一致
	engine::Level_Data 零关卡;
	engine::Asset_Scene 零场景;
	ASSERT_TRUE(renderer.level_load(零关卡, 零场景, level_utf8("旋转零.level")));
	ASSERT_EQ(零场景.objects.size(), 1u);
	EXPECT_EQ(零场景.objects[0].rotation_deg[1], 0.0f);
	std::vector<uint8_t> 旧画面(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.mesh_draw_sections(零场景.objects[0].mesh, nullptr,
		零场景.objects[0].materials.data(), static_cast<uint32_t>(零场景.objects[0].materials.size())));
	ASSERT_TRUE(renderer.frame_render(旧画面.data(), static_cast<uint32_t>(旧画面.size())));
	renderer.level_unload(零场景);

	//重新装载（对象已释放）并改用构造矩阵
	ASSERT_TRUE(renderer.level_load(零关卡, 零场景, level_utf8("旋转零.level")));
	ASSERT_EQ(零场景.objects.size(), 1u);
	float 矩阵[16] = {};
	engine::level_transform_build(零场景.objects[0].position, 零场景.objects[0].rotation_deg,
		零场景.objects[0].scale, 矩阵);
	ASSERT_TRUE(renderer.mesh_draw_sections(零场景.objects[0].mesh, 矩阵,
		零场景.objects[0].materials.data(), static_cast<uint32_t>(零场景.objects[0].materials.size())));
	std::vector<uint8_t> 矩阵画面(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(矩阵画面.data(), static_cast<uint32_t>(矩阵画面.size())));
	EXPECT_EQ(changed_pixels(旧画面, 矩阵画面), 0u) << "零旋转构造矩阵应与 nullptr 画面一致";
	renderer.level_unload(零场景);

	//绕 Y 45°：对象字段 45；画面应变化
	engine::Level_Data 旋转关卡;
	engine::Asset_Scene 旋转场景;
	ASSERT_TRUE(renderer.level_load(旋转关卡, 旋转场景, level_utf8("旋转四十五.level")));
	ASSERT_EQ(旋转场景.objects.size(), 1u);
	EXPECT_FLOAT_EQ(旋转场景.objects[0].rotation_deg[1], 45.0f);
	float 旋转矩阵[16] = {};
	engine::level_transform_build(旋转场景.objects[0].position, 旋转场景.objects[0].rotation_deg,
		旋转场景.objects[0].scale, 旋转矩阵);
	ASSERT_TRUE(renderer.mesh_draw_sections(旋转场景.objects[0].mesh, 旋转矩阵,
		旋转场景.objects[0].materials.data(), static_cast<uint32_t>(旋转场景.objects[0].materials.size())));
	std::vector<uint8_t> 旋转画面(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(旋转画面.data(), static_cast<uint32_t>(旋转画面.size())));
	const uint32_t 差异 = changed_pixels(旧画面, 旋转画面);
	std::cout << "[关卡] 旋转 45° 画面差异 " << 差异 << " 像素" << std::endl;
	EXPECT_GT(差异, 50u) << "旋转未生效";
	renderer.level_unload(旋转场景);
}

/*
材质实例数据校验（第四百四十八轮 · 方向七阶段 A）
口径：模板须为已登记材质；覆盖 tint / 自发光须有限且在 [0,1]；越界与非有限值一律拒绝；不建资源、不改状态。
*/
TEST(Renderer_Level_Test, 材质实例数据校验)
{
	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	//模板材质（默认描述）
	engine::Material_Data 描述;
	engine::Material_Handle 模板 = 0;
	ASSERT_TRUE(renderer.material_load(模板, 描述));
	ASSERT_NE(模板, 0u);

	//合法实例（不覆盖 / 覆盖 tint / 覆盖自发光）
	engine::Material_Instance_Data 实例;
	实例.模板 = 模板;
	EXPECT_TRUE(renderer.material_instance_validate(实例));
	实例.use_tint = true;
	实例.tint[0] = 0.2f;
	实例.tint[1] = 0.4f;
	实例.tint[2] = 0.6f;
	实例.tint[3] = 1.0f;
	EXPECT_TRUE(renderer.material_instance_validate(实例));
	实例.use_emissive = true;
	实例.emissive = 0.8f;
	EXPECT_TRUE(renderer.material_instance_validate(实例));

	//模板非法
	engine::Material_Instance_Data 坏模板 = 实例;
	坏模板.模板 = 0;
	EXPECT_FALSE(renderer.material_instance_validate(坏模板));
	坏模板.模板 = 9999;
	EXPECT_FALSE(renderer.material_instance_validate(坏模板));

	//覆盖越界 / 非有限
	engine::Material_Instance_Data 坏色 = 实例;
	坏色.tint[0] = 1.5f;
	EXPECT_FALSE(renderer.material_instance_validate(坏色));
	坏色.tint[0] = -0.1f;
	EXPECT_FALSE(renderer.material_instance_validate(坏色));
	坏色.tint[0] = std::nanf("");
	EXPECT_FALSE(renderer.material_instance_validate(坏色));
	engine::Material_Instance_Data 坏光 = 实例;
	坏光.emissive = -0.2f;
	EXPECT_FALSE(renderer.material_instance_validate(坏光));
	坏光.emissive = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(renderer.material_instance_validate(坏光));

	renderer.material_unload(模板);
}

/*
关卡装载开销（第四百三十三轮 · 阶段 E）
口径：单关卡（1 网格 ＋ 1 材质 ＋ 4 对象）连续装载三次，取最短耗时；给出实测数值并断言不高于 300 ms（防抖动上界）。
*/
TEST(Renderer_Level_Test, 关卡装载开销)
{
	write_box_assets();
	write_text(level_dir() / L"开销.level",
		"level 开销关卡\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 -2 0 0 1 1 1\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 方盒材质 2 0 0 1 1 1\n"
		"place 方盒 方盒材质 0 2 0 1 1 1\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	//预热一次（首轮包含管线 / 采样器等惰性创建）
	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("开销.level")));
	EXPECT_EQ(场景.objects.size(), 4u);
	renderer.level_unload(场景);

	double 最短毫秒 = 1e9;
	for (uint32_t 次数 = 0; 次数 < 3; ++次数)
	{
		engine::Asset_Scene 测场景;
		const std::chrono::steady_clock::time_point 开始 = std::chrono::steady_clock::now();
		ASSERT_TRUE(renderer.level_load(关卡, 测场景, level_utf8("开销.level")));
		const std::chrono::steady_clock::time_point 结束 = std::chrono::steady_clock::now();
		const double 毫秒 = std::chrono::duration<double, std::milli>(结束 - 开始).count();
		if (毫秒 < 最短毫秒)
			最短毫秒 = 毫秒;
		renderer.level_unload(测场景);
	}
	std::cout << "[关卡] 装载开销（1 网格 + 1 材质 + 4 对象）最短 " << 最短毫秒 << " ms" << std::endl;
	EXPECT_LT(最短毫秒, 300.0) << "关卡装载耗时异常";
}

/*
关卡反复装载与释放（第四百三十四轮 · 稳定性）
口径：同一关卡连续装载 / 释放 200 次全部成功；结束后资产缓存条目为 0、无句柄残留。
*/
TEST(Renderer_Level_Test, 关卡反复装载释放)
{
	write_box_assets();
	write_text(level_dir() / L"反复.level",
		"level 反复关卡\n"
		"camera position 0 0 3\n"
		"mesh 方盒 box.obj\n"
		"mtl 方盒材质 box.mtl\n"
		"place 方盒 方盒材质 0 0 0 1 1 1\n"
		"place 方盒 方盒材质 2 0 0 1 1 1 0 30 0\n");

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	for (uint32_t 轮次 = 0; 轮次 < 200; ++轮次)
	{
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("反复.level"))) << "第 " << 轮次 << " 次装载失败";
		ASSERT_EQ(场景.objects.size(), 2u);
		renderer.level_unload(场景);
		EXPECT_EQ(renderer.asset_cache_count(), 0u) << "第 " << 轮次 << " 次释放后仍有缓存条目";
	}
	std::cout << "[关卡] 反复装载释放 200 次完成" << std::endl;
}

/*
格式版本与写出前校验（第四百三十六轮）
口径：版本行写出并可读回；未知版本 / 版本 < 1 / 非有限字段 → 拒绝；缺省版本按 1。
*/
TEST(Renderer_Level_Test, 格式版本与写出前校验)
{
	//版本往返
	engine::Level_Data source = rich_level();
	source.version = 1;
	ASSERT_TRUE(engine::level_text_write(source, level_utf8("版本.level")));
	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("版本.level"), loaded));
	EXPECT_EQ(loaded.version, 1);

	//缺省版本按 1
	write_text(level_dir() / L"无版本.level", "level 无版本\ncamera position 0 0 3\n");
	engine::Level_Data legacy;
	ASSERT_TRUE(engine::level_text_read(level_utf8("无版本.level"), legacy));
	EXPECT_EQ(legacy.version, 1);

	//未知版本拒绝
	write_text(level_dir() / L"未来版本.level", "level 未来\nversion 2\n");
	engine::Level_Data future;
	EXPECT_FALSE(engine::level_text_read(level_utf8("未来版本.level"), future));
	EXPECT_TRUE(future.name.empty());

	//版本 0 不写出
	engine::Level_Data bad_version = rich_level();
	bad_version.version = 0;
	EXPECT_FALSE(engine::level_text_write(bad_version, level_utf8("坏版本.level")));

	//非有限字段不写出
	engine::Level_Data bad_value = rich_level();
	bad_value.fog.density = std::nanf("");
	EXPECT_FALSE(engine::level_text_write(bad_value, level_utf8("坏数值.level")));
	bad_value = rich_level();
	bad_value.light.point_shininess[0] = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(engine::level_text_write(bad_value, level_utf8("坏点光.level")));
}

/*
八盏点光边界（第四百三十六轮）
口径：恰好 8 盏通过；第 9 盏拒绝（上限 8）。
*/
TEST(Renderer_Level_Test, 八盏点光边界)
{
	std::string text = "level 八盏\n";
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		char 行[128] = {};
		std::snprintf(行, sizeof(行), "light point %d 0 0 1 1 1 5 1 0 1 0\n", static_cast<int>(序号));
		text += 行;
	}
	write_text(level_dir() / L"八盏.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("八盏.level"), loaded));
	EXPECT_EQ(loaded.light.point_count, 8u);

	text += "light point 8 0 0 1 1 1 5 1 0 1 0\n";
	write_text(level_dir() / L"九盏.level", text);
	engine::Level_Data too_many;
	EXPECT_FALSE(engine::level_text_read(level_utf8("九盏.level"), too_many));
	EXPECT_EQ(too_many.light.point_count, 0u);
}

/*
随机关卡往返（第四百四十轮 · 属性用例）
口径：固定种子生成 100 份关卡（随机相机 / 光照 / 雾 / 0–2 点光 / 0–1 聚光），写出再读回后全字段逐位相等。
*/
TEST(Renderer_Level_Test, 随机关卡往返逐位一致)
{
	//线性同余发生器（固定种子，结果可复现）
	uint32_t seed = 20261102u;
	auto next_unit = [&seed]()
	{
		seed = seed * 1664525u + 1013904223u;
		return static_cast<float>((seed >> 8) & 0xFFFFFFu) / 16777215.0f;
	};
	//区间随机
	auto next_range = [&next_unit](float low, float high)
	{
		return low + (high - low) * next_unit();
	};

	for (uint32_t 轮次 = 0; 轮次 < 100; ++轮次)
	{
		engine::Level_Data 随机;
		随机.name = "随机关卡" + std::to_string(轮次);
		//相机（合法域内随机）
		随机.camera.position[0] = next_range(-50.0f, 50.0f);
		随机.camera.position[1] = next_range(-10.0f, 30.0f);
		随机.camera.position[2] = next_range(-200.0f, 50.0f);
		随机.camera.target[0] = next_range(-20.0f, 20.0f);
		随机.camera.target[1] = next_range(-10.0f, 10.0f);
		随机.camera.target[2] = next_range(-200.0f, 0.0f);
		随机.camera.fov = next_range(0.2f, 2.8f);
		随机.camera.near_plane = next_range(0.01f, 1.0f);
		随机.camera.far_plane = next_range(20.0f, 2000.0f);
		随机.camera.ortho = (next_unit() < 0.3f);
		随机.camera.ortho_height = next_range(0.5f, 50.0f);
		随机.camera.aspect = next_range(0.0f, 3.0f);
		随机.camera.clear_color[0] = next_unit();
		随机.camera.clear_color[1] = next_unit();
		随机.camera.clear_color[2] = next_unit();
		随机.camera.clear_color[3] = next_unit();
		//光照
		随机.light.direction[0] = next_range(-1.0f, 1.0f);
		随机.light.direction[1] = next_range(-1.0f, 1.0f);
		随机.light.direction[2] = next_range(-1.0f, 1.0f);
		随机.light.color[0] = next_unit();
		随机.light.color[1] = next_unit();
		随机.light.color[2] = next_unit();
		随机.light.intensity = next_range(0.0f, 8.0f);
		随机.light.ambient = next_range(0.0f, 1.0f);
		随机.light.specular_intensity = next_range(0.0f, 2.0f);
		随机.light.specular_shininess = next_range(1.0f, 1000.0f);
		随机.light.attenuation_mode = static_cast<uint32_t>(next_range(0.0f, 2.99f));
		随机.light.shadow_enabled = (next_unit() < 0.5f) ? 1 : 0;
		随机.light.pbr_enabled = (next_unit() < 0.5f) ? 1 : 0;
		随机.light.toon_steps = static_cast<uint32_t>(next_range(0.0f, 8.99f));
		//点光 0..2 盏
		随机.light.point_count = static_cast<uint32_t>(next_range(0.0f, 2.99f));
		for (uint32_t index = 0; index < 随机.light.point_count; ++index)
		{
			随机.light.point_position[index][0] = next_range(-20.0f, 20.0f);
			随机.light.point_position[index][1] = next_range(0.0f, 20.0f);
			随机.light.point_position[index][2] = next_range(-50.0f, 0.0f);
			随机.light.point_color[index][0] = next_unit();
			随机.light.point_color[index][1] = next_unit();
			随机.light.point_color[index][2] = next_unit();
			随机.light.point_radius[index] = next_range(0.5f, 30.0f);
			随机.light.point_intensity[index] = next_range(0.0f, 5.0f);
			随机.light.point_attenuation[index] = static_cast<uint32_t>(next_range(0.0f, 2.99f));
			随机.light.point_specular[index] = next_range(0.0f, 1.0f);
			随机.light.point_shininess[index] = next_range(0.0f, 500.0f);
		}
		//聚光 0..1 盏
		随机.light.spot_count = (next_unit() < 0.5f) ? 1u : 0u;
		if (随机.light.spot_count == 1u)
		{
			for (uint32_t axis = 0; axis < 3; ++axis)
			{
				随机.light.spot_position[0][axis] = next_range(-20.0f, 20.0f);
				随机.light.spot_direction[0][axis] = next_range(-1.0f, 1.0f);
				随机.light.spot_color[0][axis] = next_unit();
			}
			随机.light.spot_radius[0] = next_range(1.0f, 40.0f);
			随机.light.spot_intensity[0] = next_range(0.0f, 6.0f);
			随机.light.spot_inner_deg[0] = next_range(1.0f, 30.0f);
			随机.light.spot_outer_deg[0] = next_range(31.0f, 90.0f);
			随机.light.spot_attenuation[0] = static_cast<uint32_t>(next_range(0.0f, 2.99f));
			随机.light.spot_specular[0] = next_range(0.0f, 1.0f);
			随机.light.spot_shininess[0] = next_range(0.0f, 500.0f);
		}
		//雾
		随机.fog.color[0] = next_unit();
		随机.fog.color[1] = next_unit();
		随机.fog.color[2] = next_unit();
		随机.fog.density = next_range(0.0f, 1.0f);
		随机.fog.start = next_range(0.0f, 5.0f);
		随机.fog.end = next_range(6.0f, 500.0f);
		随机.fog.mode = static_cast<uint32_t>(next_range(0.0f, 1.99f));
		随机.fog.height_falloff = next_range(0.0f, 1.0f);
		//固定资产段落
		随机.asset_lines.push_back("mesh 甲 box.obj");
		随机.asset_lines.push_back("place 甲 0 0 0 1 1 1 0 30 0");

		ASSERT_TRUE(engine::level_text_write(随机, level_utf8("随机.level")));
		engine::Level_Data 读回;
		ASSERT_TRUE(engine::level_text_read(level_utf8("随机.level"), 读回)) << "第 " << 轮次 << " 份随机关卡读取失败";
		expect_level_equal(随机, 读回);
	}
	std::cout << "[关卡] 随机往返 100 份完成" << std::endl;
}

/*
关卡规模装载开销（第四百四十二轮 · 性能）
口径：同一网格摆放 100 / 400 / 1000 个对象，各装载两次取最短；断言不高于 1000 ms（防抖动上界）。
*/
TEST(Renderer_Level_Test, 关卡规模装载开销)
{
	write_box_assets();

	engine::Renderer_Config config;
	config.width = 64;
	config.height = 36;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	for (uint32_t 数量 : { 100u, 400u, 1000u })
	{
		//生成关卡文本（同一网格 / 材质）
		std::string 文本 = "level 规模\ncamera position 0 0 8\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\n";
		文本.reserve(文本.size() + static_cast<std::size_t>(数量) * 64);
		for (uint32_t 序号 = 0; 序号 < 数量; ++序号)
		{
			char 行[96] = {};
			std::snprintf(行, sizeof(行), "place 方盒 方盒材质 %d %d 0 1 1 1 0 %d 0\n",
				static_cast<int>(序号 % 20) - 10, static_cast<int>(序号 / 20) - 10,
				static_cast<int>(序号 % 7) * 5);
			文本 += 行;
		}
		write_text(level_dir() / L"规模.level", 文本);

		//预热一次
		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("规模.level")));
		ASSERT_EQ(场景.objects.size(), 数量);
		renderer.level_unload(场景);

		double 最短毫秒 = 1e9;
		for (uint32_t 次数 = 0; 次数 < 2; ++次数)
		{
			engine::Asset_Scene 测场景;
			const std::chrono::steady_clock::time_point 开始 = std::chrono::steady_clock::now();
			ASSERT_TRUE(renderer.level_load(关卡, 测场景, level_utf8("规模.level")));
			const std::chrono::steady_clock::time_point 结束 = std::chrono::steady_clock::now();
			const double 毫秒 = std::chrono::duration<double, std::milli>(结束 - 开始).count();
			if (毫秒 < 最短毫秒)
				最短毫秒 = 毫秒;
			renderer.level_unload(测场景);
		}
		std::cout << "[关卡] " << 数量 << " 摆放装载最短 " << 最短毫秒 << " ms" << std::endl;
		EXPECT_LT(最短毫秒, 1000.0);
	}
}

/*
关卡大量摆放（第四百三十四轮 · 稳定性）
口径：同一网格 / 材质摆放 20 个对象（含旋转）；对象数、句柄复用与绘制登记量正确。
*/
TEST(Renderer_Level_Test, 关卡大量摆放)
{
	write_box_assets();
	std::string 文本 = "level 大量摆放\ncamera position 0 0 6\nmesh 方盒 box.obj\nmtl 方盒材质 box.mtl\n";
	for (uint32_t 序号 = 0; 序号 < 20; ++序号)
	{
		char 行[128] = {};
		std::snprintf(行, sizeof(行), "place 方盒 方盒材质 %d %d 0 1 1 1 0 %d 0\n",
			static_cast<int>(序号 % 5) - 2, static_cast<int>(序号 / 5) - 2, static_cast<int>(序号) * 9);
		文本 += 行;
	}
	write_text(level_dir() / L"大量摆放.level", 文本);

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(renderer.level_load(关卡, 场景, level_utf8("大量摆放.level")));
	EXPECT_EQ(场景.meshes.size(), 1u);
	EXPECT_EQ(场景.materials.size(), 1u);
	ASSERT_EQ(场景.objects.size(), 20u);
	//全部对象共享同一网格句柄
	for (const engine::Asset_Object& 对象 : 场景.objects)
		EXPECT_EQ(对象.mesh, 场景.meshes[0]);

	//逐对象登记绘制（同一网格 20 份；门面 scene_draw）
	EXPECT_EQ(renderer.scene_draw(场景), 20u);
	//出帧一次结算帧统计
	std::vector<uint8_t> 像素(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(像素.data(), static_cast<uint32_t>(像素.size())));
	const engine::Frame_Stats 统计 = renderer.frame_stats_get();
	std::cout << "[关卡] 大量摆放绘制项 " << 统计.drawn_items << std::endl;
	EXPECT_GE(统计.drawn_items, 1u);

	renderer.level_unload(场景);
	EXPECT_TRUE(场景.objects.empty());
}
