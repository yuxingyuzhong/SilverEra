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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Application/Renderer/src/render/Level/关卡文件.h"

namespace
{
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
