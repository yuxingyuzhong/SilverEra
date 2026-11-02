/*
渲染关卡测试（第四百二十九轮 · 方向六：场景序列化与关卡装载 · 阶段 A）
覆盖：关卡文本往返（全字段逐位一致）、缺省补齐、非法文本拒绝（未知段 / 未知键 / 字段数不符 /
非有限值 / 开关非 0-1 / 点光超 8 盏 / 资产行缺字段）、资产声明行原样保留、写入与读取失败路径。
口径：只测文本层（`engine::level_text_read` / `engine::level_text_write`），不建任何 GPU 资源。
临时文件写在 `out/关卡测试/`：写盘用宽字符路径，加载 / 写出入参用 UTF-8 窄字符串。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
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

	//三通道浮点逐位相等
	bool triple_bit_equal(const float* left, const float* right)
	{
		return bit_equal(left[0], right[0]) && bit_equal(left[1], right[1]) && bit_equal(left[2], right[2]);
	}

	//逐位比较两个关卡的全字段（失败时输出首个差异字段）
	void expect_level_equal(const engine::Level_Data& left, const engine::Level_Data& right)
	{
		EXPECT_EQ(left.name, right.name);

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

	//逐段绑材质绘制
	const engine::Material_Handle* 材质 = 场景.objects[0].materials.data();
	ASSERT_TRUE(renderer.mesh_draw_sections(场景.objects[0].mesh, nullptr,
		材质, static_cast<uint32_t>(场景.objects[0].materials.size())));
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
	ASSERT_TRUE(renderer.mesh_draw_sections(场景甲.objects[0].mesh, nullptr,
		场景甲.objects[0].materials.data(), static_cast<uint32_t>(场景甲.objects[0].materials.size())));
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

	//逐对象登记绘制（同一网格 20 份）
	for (const engine::Asset_Object& 对象 : 场景.objects)
	{
		float 矩阵[16] = {};
		engine::level_transform_build(对象.position, 对象.rotation_deg, 对象.scale, 矩阵);
		ASSERT_TRUE(renderer.mesh_draw_sections(对象.mesh, 矩阵, 对象.materials.data(),
			static_cast<uint32_t>(对象.materials.size())));
	}
	//出帧一次结算帧统计
	std::vector<uint8_t> 像素(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(像素.data(), static_cast<uint32_t>(像素.size())));
	const engine::Frame_Stats 统计 = renderer.frame_stats_get();
	std::cout << "[关卡] 大量摆放绘制项 " << 统计.drawn_items << std::endl;
	EXPECT_GE(统计.drawn_items, 1u);

	renderer.level_unload(场景);
	EXPECT_TRUE(场景.objects.empty());
}
