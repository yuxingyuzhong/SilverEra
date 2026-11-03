/*
关卡文本测试（系统层；渲染层职责下沉第一批 · 轮 1）
覆盖：关卡文本往返（全字段逐位一致）、缺省补齐、非法文本拒绝（未知段 / 未知键 / 字段数不符 /
非有限值 / 开关越界 / 点光超 8 盏 / 资产行缺字段）、写出前校验、资产行原样保留、
材质实例声明（往返 / 结构非法 / 重名 / CRUD）、变换矩阵（零旋转 / 绕 Z 旋转）、随机往返。
口径：只测文本层（`engine::level_text_read` / `engine::level_text_write` 等），不建任何 GPU 资源。
临时文件写在 `out/关卡文本测试/`：写盘用宽字符路径，加载 / 写出入参用 UTF-8 窄字符串。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "Engine/EngineSystem/src/scene/Level/关卡文本.h"

namespace
{
	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path level_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"关卡文本测试");
	}

	//关卡文件的 UTF-8 窄路径（供读写入参）
	std::string level_utf8(const char* name)
	{
		return std::string("out/关卡文本测试/") + name;
	}

	//关卡文件的宽字符路径（供写盘 / 读盘）
	std::filesystem::path level_wide(const char* name)
	{
		const std::string text(name);
		std::u8string u8(reinterpret_cast<const char8_t*>(text.data()), text.size());
		return level_dir() / std::filesystem::path(u8);
	}

	//写一个 UTF-8 文本文件（目录自动创建；宽路径写盘）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//读一个文件的全部字节（宽路径读盘；用于判断两次写出是否逐字节相同）
	std::string read_bytes(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}

	//浮点逐位相等（往返口径：%.9g 必须能唯一还原二进制 32 位浮点）
	bool bit_equal(float left, float right)
	{
		return std::memcmp(&left, &right, sizeof(float)) == 0;
	}

	//浮点数组逐位相等
	void expect_floats_equal(const float* left, const float* right, std::size_t count, const char* label)
	{
		for (std::size_t index = 0; index < count; ++index)
			//逐元素比对
			EXPECT_TRUE(bit_equal(left[index], right[index])) << label << " 第 " << index << " 项不一致";
	}

	//相机组逐位比对
	void expect_camera_equal(const engine::Camera_Data& left, const engine::Camera_Data& right)
	{
		expect_floats_equal(left.position, right.position, 3, "相机位置");
		expect_floats_equal(left.target, right.target, 3, "相机注视点");
		expect_floats_equal(left.up, right.up, 3, "相机上方向");
		expect_floats_equal(&left.fov, &right.fov, 1, "相机视场角");
		expect_floats_equal(&left.near_plane, &right.near_plane, 1, "相机近平面");
		expect_floats_equal(&left.far_plane, &right.far_plane, 1, "相机远平面");
		expect_floats_equal(&left.ortho_height, &right.ortho_height, 1, "正交可见高度");
		expect_floats_equal(&left.aspect, &right.aspect, 1, "宽高比");
		expect_floats_equal(left.clear_color, right.clear_color, 4, "清屏色");
		//正交开关
		EXPECT_EQ(left.ortho, right.ortho);
	}

	//雾组逐位比对
	void expect_fog_equal(const engine::Fog_Data& left, const engine::Fog_Data& right)
	{
		expect_floats_equal(left.color, right.color, 3, "雾色");
		expect_floats_equal(&left.density, &right.density, 1, "雾密度");
		expect_floats_equal(&left.start, &right.start, 2, "雾起止距离");
		expect_floats_equal(&left.height_falloff, &right.height_falloff, 1, "雾高度衰减");
		EXPECT_EQ(left.mode, right.mode);
	}

	//光照组逐位比对（含全部有效光源槽位）
	void expect_light_equal(const engine::Light_Data& left, const engine::Light_Data& right)
	{
		expect_floats_equal(left.direction, right.direction, 3, "光方向");
		expect_floats_equal(left.color, right.color, 3, "光颜色");
		expect_floats_equal(&left.intensity, &right.intensity, 1, "光强度");
		expect_floats_equal(&left.ambient, &right.ambient, 1, "环境光");
		expect_floats_equal(&left.specular_intensity, &right.specular_intensity, 2, "镜面高光两项");
		expect_floats_equal(left.ambient_color, right.ambient_color, 3, "环境光颜色");
		expect_floats_equal(left.ambient_ground, right.ambient_ground, 3, "环境光地面色");
		expect_floats_equal(&left.ambient_hemisphere, &right.ambient_hemisphere, 1, "半球混合强度");
		expect_floats_equal(left.fill_direction, right.fill_direction, 3, "补光方向");
		expect_floats_equal(left.fill_color, right.fill_color, 3, "补光颜色");
		expect_floats_equal(&left.fill_intensity, &right.fill_intensity, 1, "补光强度");
		expect_floats_equal(&left.directional_specular, &right.directional_specular, 2, "方向光高光缩放");
		expect_floats_equal(&left.shadow_bias, &right.shadow_bias, 3, "方向光阴影三项");
		expect_floats_equal(&left.spot_shadow_bias, &right.spot_shadow_bias, 1, "聚光阴影偏置");
		expect_floats_equal(&left.spot_shadow_softness, &right.spot_shadow_softness, 2, "聚光阴影软度与强度");
		expect_floats_equal(&left.pbr_roughness, &right.pbr_roughness, 2, "PBR 粗糙度与金属度");
		expect_floats_equal(&left.toon_rim, &right.toon_rim, 1, "边缘光强度");
		expect_floats_equal(left.toon_shadow_tint, right.toon_shadow_tint, 3, "暗部色调");
		expect_floats_equal(&left.toon_specular, &right.toon_specular, 1, "卡通高光阈值");
		expect_floats_equal(&left.outline_width, &right.outline_width, 1, "描边宽度");
		expect_floats_equal(left.outline_color, right.outline_color, 3, "描边色");
		EXPECT_EQ(left.attenuation_mode, right.attenuation_mode);
		EXPECT_EQ(left.shadow_enabled, right.shadow_enabled);
		EXPECT_EQ(left.shadow_resolution, right.shadow_resolution);
		EXPECT_EQ(left.spot_shadow_enabled, right.spot_shadow_enabled);
		EXPECT_EQ(left.spot_shadow_index, right.spot_shadow_index);
		EXPECT_EQ(left.spot_shadow_all, right.spot_shadow_all);
		EXPECT_EQ(left.spot_shadow_bias_auto, right.spot_shadow_bias_auto);
		EXPECT_EQ(left.pbr_enabled, right.pbr_enabled);
		EXPECT_EQ(left.toon_steps, right.toon_steps);
		//点光源槽位
		EXPECT_EQ(left.point_count, right.point_count);
		EXPECT_EQ(left.spot_count, right.spot_count);
		for (uint32_t index = 0; index < left.point_count && index < 8u; ++index)
		{
			expect_floats_equal(left.point_position[index], right.point_position[index], 3, "点光位置");
			expect_floats_equal(left.point_color[index], right.point_color[index], 3, "点光颜色");
			expect_floats_equal(&left.point_radius[index], &right.point_radius[index], 1, "点光半径");
			expect_floats_equal(&left.point_intensity[index], &right.point_intensity[index], 1, "点光强度");
			expect_floats_equal(&left.point_specular[index], &right.point_specular[index], 1, "点光高光缩放");
			expect_floats_equal(&left.point_shininess[index], &right.point_shininess[index], 1, "点光光泽度");
			EXPECT_EQ(left.point_attenuation[index], right.point_attenuation[index]);
		}
		//聚光灯槽位
		for (uint32_t index = 0; index < left.spot_count && index < 8u; ++index)
		{
			expect_floats_equal(left.spot_position[index], right.spot_position[index], 3, "聚光位置");
			expect_floats_equal(left.spot_direction[index], right.spot_direction[index], 3, "聚光方向");
			expect_floats_equal(left.spot_color[index], right.spot_color[index], 3, "聚光颜色");
			expect_floats_equal(&left.spot_radius[index], &right.spot_radius[index], 1, "聚光半径");
			expect_floats_equal(&left.spot_intensity[index], &right.spot_intensity[index], 1, "聚光强度");
			expect_floats_equal(&left.spot_inner_deg[index], &right.spot_inner_deg[index], 1, "聚光内锥角");
			expect_floats_equal(&left.spot_outer_deg[index], &right.spot_outer_deg[index], 1, "聚光外锥角");
			expect_floats_equal(&left.spot_specular[index], &right.spot_specular[index], 1, "聚光高光缩放");
			expect_floats_equal(&left.spot_shininess[index], &right.spot_shininess[index], 1, "聚光光泽度");
			EXPECT_EQ(left.spot_attenuation[index], right.spot_attenuation[index]);
		}
	}

	//材质实例逐位比对
	void expect_material_equal(const engine::Level_Material& left, const engine::Level_Material& right)
	{
		EXPECT_EQ(left.实例名, right.实例名);
		EXPECT_EQ(left.模板, right.模板);
		EXPECT_EQ(left.use_tint, right.use_tint);
		expect_floats_equal(left.tint, right.tint, 4, "实例底色");
		EXPECT_EQ(left.use_emissive, right.use_emissive);
		expect_floats_equal(&left.emissive, &right.emissive, 1, "实例自发光");
		EXPECT_EQ(left.use_toon, right.use_toon);
		expect_floats_equal(&left.toon_steps, &right.toon_steps, 3, "实例卡通三值");
		EXPECT_EQ(left.use_pbr, right.use_pbr);
		expect_floats_equal(&left.pbr_roughness, &right.pbr_roughness, 2, "实例 PBR 两值");
		EXPECT_EQ(left.use_clearcoat, right.use_clearcoat);
		expect_floats_equal(&left.clear_coat, &right.clear_coat, 2, "实例清漆两值");
	}

	//关卡整体逐位比对（不含未写出的空闲光源槽位）
	void expect_level_equal(const engine::Level_Data& left, const engine::Level_Data& right)
	{
		EXPECT_EQ(left.name, right.name);
		EXPECT_EQ(left.version, right.version);
		expect_camera_equal(left.camera, right.camera);
		expect_light_equal(left.light, right.light);
		expect_fog_equal(left.fog, right.fog);
		EXPECT_EQ(left.asset_lines, right.asset_lines);
		ASSERT_EQ(left.materials.size(), right.materials.size());
		for (std::size_t index = 0; index < left.materials.size(); ++index)
			expect_material_equal(left.materials[index], right.materials[index]);
	}

	//判断是否为「失败清空后的空关卡」
	void expect_empty_level(const engine::Level_Data& level)
	{
		EXPECT_TRUE(level.name.empty());
		EXPECT_EQ(level.version, 1);
		EXPECT_EQ(level.light.point_count, 0u);
		EXPECT_EQ(level.light.spot_count, 0u);
		EXPECT_TRUE(level.asset_lines.empty());
		EXPECT_TRUE(level.materials.empty());
	}

	/*
	往返一致性
	写 → 读 → 再写，比对两份文本逐字节相同；再比对内存结构逐位相同。
	*/
	void expect_roundtrip(const engine::Level_Data& level, const char* name)
	{
		ASSERT_TRUE(engine::level_text_write(level, level_utf8(name)))
			<< "写出失败：" << name;
		engine::Level_Data back;
		ASSERT_TRUE(engine::level_text_read(level_utf8(name), back))
			<< "读回失败：" << name;
		expect_level_equal(level, back);
		//二次写出必须与原文本逐字节一致（覆盖未参与比对的字段）
		const std::string second = std::string("再写_") + name;
		ASSERT_TRUE(engine::level_text_write(back, level_utf8(second.c_str())))
			<< "二次写出失败：" << second;
		EXPECT_EQ(read_bytes(level_wide(name)), read_bytes(level_wide(second.c_str())));
	}

	//填充一份「全字段非默认」的关卡
	engine::Level_Data full_level_build(void)
	{
		engine::Level_Data level;
		level.name = "全字段关卡";
		level.version = 1;
		//相机
		const float position[3] = { 1.5f, -2.25f, 6.75f };
		const float target[3] = { 0.5f, 0.25f, -1.0f };
		const float up[3] = { 0.0f, 0.7071f, 0.7071f };
		const float clear[4] = { 0.123456f, 0.314159f, 0.271828f, 0.5f };
		std::memcpy(level.camera.position, position, sizeof(position));
		std::memcpy(level.camera.target, target, sizeof(target));
		std::memcpy(level.camera.up, up, sizeof(up));
		std::memcpy(level.camera.clear_color, clear, sizeof(clear));
		level.camera.fov = 1.2345f;
		level.camera.near_plane = 0.05f;
		level.camera.far_plane = 250.0f;
		level.camera.ortho = true;
		level.camera.ortho_height = 3.5f;
		level.camera.aspect = 1.777f;
		//光照基础
		const float direction[3] = { -0.5f, -1.0f, -0.25f };
		std::memcpy(level.light.direction, direction, sizeof(direction));
		level.light.color[0] = 0.9f;
		level.light.color[1] = 0.85f;
		level.light.color[2] = 0.8f;
		level.light.intensity = 2.5f;
		level.light.ambient = 0.125f;
		level.light.specular_intensity = 0.75f;
		level.light.specular_shininess = 48.0f;
		level.light.attenuation_mode = 2;
		level.light.ambient_color[0] = 0.7f;
		level.light.ambient_color[1] = 0.8f;
		level.light.ambient_color[2] = 0.9f;
		level.light.ambient_ground[0] = 0.2f;
		level.light.ambient_ground[1] = 0.3f;
		level.light.ambient_ground[2] = 0.4f;
		level.light.ambient_hemisphere = 0.65f;
		level.light.fill_direction[0] = 0.25f;
		level.light.fill_direction[1] = 0.5f;
		level.light.fill_direction[2] = 1.0f;
		level.light.fill_color[0] = 0.1f;
		level.light.fill_color[1] = 0.2f;
		level.light.fill_color[2] = 0.3f;
		level.light.fill_intensity = 0.45f;
		level.light.directional_specular = 0.5f;
		level.light.fill_specular = 0.25f;
		level.light.shadow_enabled = 1;
		level.light.shadow_bias = 0.012f;
		level.light.shadow_softness = 0.75f;
		level.light.shadow_strength = 0.9f;
		level.light.shadow_resolution = 2048;
		level.light.spot_shadow_enabled = 1;
		level.light.spot_shadow_bias = 0.03f;
		level.light.spot_shadow_softness = 0.5f;
		level.light.spot_shadow_strength = 0.8f;
		level.light.spot_shadow_index = 1;
		level.light.spot_shadow_all = 1;
		level.light.spot_shadow_bias_auto = 1;
		level.light.pbr_enabled = 1;
		level.light.pbr_roughness = 0.35f;
		level.light.pbr_metallic = 0.9f;
		level.light.toon_steps = 4;
		level.light.toon_rim = 0.55f;
		level.light.toon_shadow_tint[0] = 0.66f;
		level.light.toon_shadow_tint[1] = 0.7f;
		level.light.toon_shadow_tint[2] = 0.88f;
		level.light.toon_specular = 0.3f;
		level.light.outline_width = 0.02f;
		level.light.outline_color[0] = 0.05f;
		level.light.outline_color[1] = 0.02f;
		level.light.outline_color[2] = 0.01f;
		//两盏点光
		for (uint32_t index = 0; index < 2; ++index)
		{
			level.light.point_position[index][0] = 1.0f + index;
			level.light.point_position[index][1] = 2.0f;
			level.light.point_position[index][2] = -1.0f;
			level.light.point_color[index][0] = 0.4f;
			level.light.point_color[index][1] = 0.6f;
			level.light.point_color[index][2] = 0.8f;
			level.light.point_radius[index] = 3.5f;
			level.light.point_intensity[index] = 2.0f;
			level.light.point_attenuation[index] = index + 1;
			level.light.point_specular[index] = 0.5f;
			level.light.point_shininess[index] = 32.0f;
		}
		level.light.point_count = 2;
		//两盏聚光
		for (uint32_t index = 0; index < 2; ++index)
		{
			level.light.spot_position[index][0] = 2.0f;
			level.light.spot_position[index][1] = 3.0f + index;
			level.light.spot_position[index][2] = 4.0f;
			level.light.spot_direction[index][0] = 0.0f;
			level.light.spot_direction[index][1] = -1.0f;
			level.light.spot_direction[index][2] = 0.0f;
			level.light.spot_color[index][0] = 0.9f;
			level.light.spot_color[index][1] = 0.95f;
			level.light.spot_color[index][2] = 1.0f;
			level.light.spot_radius[index] = 7.0f;
			level.light.spot_intensity[index] = 1.5f;
			level.light.spot_inner_deg[index] = 12.0f;
			level.light.spot_outer_deg[index] = 25.0f;
			level.light.spot_attenuation[index] = 2;
			level.light.spot_specular[index] = 0.75f;
			level.light.spot_shininess[index] = 64.0f;
		}
		level.light.spot_count = 2;
		//雾
		level.fog.color[0] = 0.1f;
		level.fog.color[1] = 0.2f;
		level.fog.color[2] = 0.35f;
		level.fog.density = 0.6f;
		level.fog.start = 2.5f;
		level.fog.end = 40.0f;
		level.fog.mode = 1;
		level.fog.height_falloff = 0.15f;
		//材质实例（覆盖项全开）
		engine::Level_Material material;
		material.实例名 = "甲";
		material.模板 = "基础";
		material.use_tint = true;
		material.tint[0] = 0.8f;
		material.tint[1] = 0.2f;
		material.tint[2] = 0.2f;
		material.tint[3] = 0.75f;
		material.use_emissive = true;
		material.emissive = 0.4f;
		material.use_toon = true;
		material.toon_steps = 3.0f;
		material.toon_rim = 0.45f;
		material.toon_specular = 0.6f;
		material.use_pbr = true;
		material.pbr_roughness = 0.25f;
		material.pbr_metallic = 0.8f;
		material.use_clearcoat = true;
		material.clear_coat = 0.9f;
		material.clear_coat_roughness = 0.2f;
		level.materials.push_back(material);
		//第二个实例：仅覆盖底色（验证「未使用的覆盖项不写出」）
		engine::Level_Material plain;
		plain.实例名 = "乙";
		plain.模板 = "基础";
		plain.use_tint = true;
		plain.tint[0] = 0.3f;
		plain.tint[1] = 0.5f;
		plain.tint[2] = 0.7f;
		level.materials.push_back(plain);
		//资产行
		level.asset_lines.push_back("mesh 箱子 box.obj");
		level.asset_lines.push_back("mtl 基础 box.mtl");
		level.asset_lines.push_back("place 箱子 0 0 0 0.5 0.5 0.5 0 0 0 甲");
		return level;
	}
}

//默认关卡写出后读回与原值逐位一致
TEST(EngineSystem_LevelText_Test, 关卡文本缺省往返)
{
	engine::Level_Data level;
	level.name = "默认关卡";
	ASSERT_TRUE(engine::level_text_write(level, level_utf8("缺省.level")));
	engine::Level_Data back;
	ASSERT_TRUE(engine::level_text_read(level_utf8("缺省.level"), back));
	expect_level_equal(level, back);
	expect_roundtrip(level, "缺省再写.level");
}

//全字段关卡写出后读回逐位一致
TEST(EngineSystem_LevelText_Test, 关卡文本全字段往返)
{
	const engine::Level_Data level = full_level_build();
	expect_roundtrip(level, "全字段.level");
}

//只写少量行的文本读回后其余字段取默认值
TEST(EngineSystem_LevelText_Test, 关卡缺省补齐)
{
	write_text(level_dir() / L"缺省补齐.level",
		"# 注释行\n"
		"level 稀疏关卡\n"
		"camera position 1 2 3\n"
		"\n");
	engine::Level_Data level;
	ASSERT_TRUE(engine::level_text_read(level_utf8("缺省补齐.level"), level));
	//名称与显式字段
	EXPECT_EQ(level.name, "稀疏关卡");
	EXPECT_EQ(level.version, 1);
	EXPECT_TRUE(bit_equal(level.camera.position[0], 1.0f));
	EXPECT_TRUE(bit_equal(level.camera.position[1], 2.0f));
	EXPECT_TRUE(bit_equal(level.camera.position[2], 3.0f));
	//其余取默认
	EXPECT_TRUE(bit_equal(level.camera.fov, 1.0472f));
	EXPECT_TRUE(bit_equal(level.light.intensity, 1.0f));
	EXPECT_TRUE(bit_equal(level.fog.density, 0.0f));
	EXPECT_EQ(level.light.point_count, 0u);
	EXPECT_TRUE(level.asset_lines.empty());
}

//未知段 / 未知键 / 字段数不符 / 非有限值均拒绝读取
TEST(EngineSystem_LevelText_Test, 关卡非法被拒绝)
{
	const std::vector<std::string> texts = {
		"level 甲\nunknown 1\n",
		"level 甲\ncamera position 1 2\n",
		"level 甲\ncamera focus 1\n",
		"level 甲\nlight nameless 1\n",
		"level 甲\nlight intensity nan\n",
		"level 甲\nlight intensity inf\n",
		"level 甲\ncamera ortho 2\n",
		"level 甲\nversion 2\n",
		"level 甲\nfog mode -1\n",
		"level 甲\nlight spot_shadow 1 0.02 0.5\n",
		"level 甲\nlight spot_shadow_index 8\n",
		"level 甲\nmaterial 甲 基础 tint 0.5 0.5 0.5\n",
		"level 甲\nmesh 箱子\n",
		"level 甲\nplace 箱子\n",
	};
	const char* const names[] = {
		"非法未知段", "非法相机缺字段", "非法相机未知键", "非法光照未知键", "非法非数值", "非法无穷值",
		"非法开关越界", "非法版本", "非法负数", "非法聚光阴影缺项", "非法聚光阴影序号", "非法材质缺底色",
		"非法资产缺路径", "非法摆放缺字段",
	};
	for (std::size_t index = 0; index < texts.size(); ++index)
	{
		//逐个写入并读取
		write_text(level_wide(names[index]), texts[index]);
		engine::Level_Data level;
		EXPECT_FALSE(engine::level_text_read(level_utf8(names[index]), level))
			<< "非法文本被接受：" << names[index];
		//失败契约：输出清空
		expect_empty_level(level);
	}
}

//点光 / 聚光超过 8 盏被拒绝
TEST(EngineSystem_LevelText_Test, 关卡光源超限被拒绝)
{
	std::string text = "level 甲\nlight point 0 0 0 1 1 1 5 1 0 1 0\n";
	//第 9 盏
	text += "light point 0 0 0 1 1 1 5 1 0 1 0\nlight point 0 0 0 1 1 1 5 1 0 1 0\n"
		"light point 0 0 0 1 1 1 5 1 0 1 0\nlight point 0 0 0 1 1 1 5 1 0 1 0\n"
		"light point 0 0 0 1 1 1 5 1 0 1 0\nlight point 0 0 0 1 1 1 5 1 0 1 0\n"
		"light point 0 0 0 1 1 1 5 1 0 1 0\nlight point 0 0 0 1 1 1 5 1 0 1 0\n";
	write_text(level_dir() / L"光源超限.level", text);
	engine::Level_Data level;
	EXPECT_FALSE(engine::level_text_read(level_utf8("光源超限.level"), level));
	expect_empty_level(level);
}

//资产声明行原样保留（首尾空白被裁剪、内部原样）
TEST(EngineSystem_LevelText_Test, 关卡资产行原样保留)
{
	write_text(level_dir() / L"资产行.level",
		"level 甲\n"
		"   mesh    箱子   box.obj   \n"
		"mtl 基础 box.mtl\n"
		"place 箱子 0 0 0 1 1 1 0 0 0 甲\n");
	engine::Level_Data level;
	ASSERT_TRUE(engine::level_text_read(level_utf8("资产行.level"), level));
	ASSERT_EQ(level.asset_lines.size(), 3u);
	EXPECT_EQ(level.asset_lines[0], "mesh    箱子   box.obj");
	EXPECT_EQ(level.asset_lines[1], "mtl 基础 box.mtl");
	EXPECT_EQ(level.asset_lines[2], "place 箱子 0 0 0 1 1 1 0 0 0 甲");
	//原样写回后再读，行内容不变
	expect_roundtrip(level, "资产行往返.level");
}

//材质实例声明往返与缺省
TEST(EngineSystem_LevelText_Test, 关卡材质实例往返)
{
	const engine::Level_Data level = full_level_build();
	expect_roundtrip(level, "材质实例.level");
	//仅覆盖底色的实例：其余覆盖开关保持关闭
	ASSERT_EQ(level.materials.size(), 2u);
	EXPECT_FALSE(level.materials[1].use_emissive);
	EXPECT_FALSE(level.materials[1].use_toon);
	EXPECT_FALSE(level.materials[1].use_clearcoat);
}

//材质实例结构非法与数值越界被拒绝
TEST(EngineSystem_LevelText_Test, 关卡材质实例非法)
{
	const std::vector<std::string> texts = {
		"level 甲\nmaterial 甲\n",
		"level 甲\nmaterial 甲 基础 tint 1 1 1 1 tint 1 1 1 1\n",
		"level 甲\nmaterial 甲 基础 toon 3 0.5 0.5 tint 1 1 1 1\n",
		"level 甲\nmaterial 甲 基础 pbr 0.05 0.5\n",
		"level 甲\nmaterial 甲 基础 pbr 0.1 1.2\n",
		"level 甲\nmaterial 甲 基础 clearcoat 0 0.2\n",
		"level 甲\nmaterial 甲 基础 clearcoat 0.5 0.02\n",
		"level 甲\nmaterial 甲 基础 unknown 1\n",
		"level 甲\nmaterial 甲 基础\nmaterial 甲 基础\n",
	};
	const char* const names[] = {
		"材质缺模板", "材质重复底色", "材质顺序非法", "材质PBR边界", "材质金属度越界",
		"材质清漆强度", "材质清漆粗糙度", "材质未知覆盖项", "材质重名",
	};
	for (std::size_t index = 0; index < texts.size(); ++index)
	{
		write_text(level_wide(names[index]), texts[index]);
		engine::Level_Data level;
		EXPECT_FALSE(engine::level_text_read(level_utf8(names[index]), level))
			<< "非法材质被接受：" << names[index];
		expect_empty_level(level);
	}
}

//写出前校验：空路径 / 关卡名换行 / 版本 0 / 非有限字段 / 材质越界
TEST(EngineSystem_LevelText_Test, 关卡写出前校验)
{
	engine::Level_Data level;
	//空路径
	EXPECT_FALSE(engine::level_text_write(level, ""));
	//关卡名含换行
	level.name = "甲\n乙";
	EXPECT_FALSE(engine::level_text_write(level, level_utf8("名称换行.level")));
	level.name = "甲";
	//版本 0
	level.version = 0;
	EXPECT_FALSE(engine::level_text_write(level, level_utf8("版本零.level")));
	level.version = 1;
	//非有限字段
	level.camera.fov = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(engine::level_text_write(level, level_utf8("非有限.level")));
	level.camera.fov = 1.0f;
	//材质越界（写出前统一校验）
	engine::Level_Material material;
	material.实例名 = "甲";
	material.模板 = "基础";
	material.use_tint = true;
	material.tint[0] = 1.5f;
	level.materials.push_back(material);
	EXPECT_FALSE(engine::level_text_write(level, level_utf8("材质越界.level")));
	//修正后写出成功
	level.materials[0].tint[0] = 0.5f;
	EXPECT_TRUE(engine::level_text_write(level, level_utf8("材质修正.level")));
}

//材质实例 CRUD：查表 / 整体替换 / 追加 / 删除
TEST(EngineSystem_LevelText_Test, 关卡材质实例增删改查)
{
	engine::Level_Data level = full_level_build();
	//查表：命中
	engine::Level_Material found;
	EXPECT_TRUE(engine::level_material_find(level, "甲", found));
	EXPECT_EQ(found.模板, "基础");
	EXPECT_TRUE(found.use_clearcoat);
	//查表：空名 / 未命中
	EXPECT_FALSE(engine::level_material_find(level, "", found));
	EXPECT_FALSE(engine::level_material_find(level, "丙", found));

	//整体替换（名称以关卡内为准）
	engine::Level_Material update;
	update.实例名 = "错误名称";
	update.模板 = "替换模板";
	update.use_emissive = true;
	update.emissive = 0.9f;
	EXPECT_TRUE(engine::level_material_update(level, "甲", update));
	EXPECT_STREQ(level.materials[0].实例名.c_str(), "甲");
	EXPECT_EQ(level.materials[0].模板, "替换模板");
	EXPECT_TRUE(level.materials[0].use_emissive);
	EXPECT_FALSE(level.materials[0].use_tint);
	EXPECT_FALSE(engine::level_material_update(level, "丙", update));

	//追加：空名 / 重名 / 正常
	engine::Level_Material empty;
	EXPECT_FALSE(engine::level_material_add(level, empty));
	engine::Level_Material duplicate;
	duplicate.实例名 = "甲";
	EXPECT_FALSE(engine::level_material_add(level, duplicate));
	engine::Level_Material added;
	added.实例名 = "丙";
	added.模板 = "基础";
	added.use_pbr = true;
	added.pbr_roughness = 0.5f;
	added.pbr_metallic = 0.25f;
	EXPECT_TRUE(engine::level_material_add(level, added));
	ASSERT_EQ(level.materials.size(), 3u);
	EXPECT_EQ(level.materials[2].实例名, "丙");

	//删除：空名 / 未命中 / 正常（其余顺序不变）
	EXPECT_FALSE(engine::level_material_remove(level, ""));
	EXPECT_FALSE(engine::level_material_remove(level, "丁"));
	EXPECT_TRUE(engine::level_material_remove(level, "乙"));
	ASSERT_EQ(level.materials.size(), 2u);
	EXPECT_EQ(level.materials[0].实例名, "甲");
	EXPECT_EQ(level.materials[1].实例名, "丙");
}

//变换矩阵：零旋转等价于对角缩放＋平移
TEST(EngineSystem_LevelText_Test, 关卡变换零旋转)
{
	const float position[3] = { 2.0f, -3.0f, 4.0f };
	const float rotation[3] = { 0.0f, 0.0f, 0.0f };
	const float scale[3] = { 2.0f, 0.5f, 3.0f };
	float matrix[16] = {};
	engine::level_transform_build(position, rotation, scale, matrix);
	//对角缩放
	EXPECT_TRUE(bit_equal(matrix[0], 2.0f));
	EXPECT_TRUE(bit_equal(matrix[5], 0.5f));
	EXPECT_TRUE(bit_equal(matrix[10], 3.0f));
	EXPECT_TRUE(bit_equal(matrix[15], 1.0f));
	//平移落在末列
	EXPECT_TRUE(bit_equal(matrix[12], 2.0f));
	EXPECT_TRUE(bit_equal(matrix[13], -3.0f));
	EXPECT_TRUE(bit_equal(matrix[14], 4.0f));
	//其余为零（旋转矩阵的 r20 = -sin 0 会带出 -0.0，故按数值判定而非逐位判定）
	const std::size_t zeros[] = { 1, 2, 3, 4, 6, 7, 8, 9, 11 };
	for (std::size_t index : zeros)
		EXPECT_TRUE(matrix[index] == 0.0f);
}

//变换矩阵：绕 Z 旋转 90°（列主序：第 0 列写给 +Y，第 1 列写给 -X）
TEST(EngineSystem_LevelText_Test, 关卡变换绕Z旋转)
{
	const float position[3] = { 0.0f, 0.0f, 0.0f };
	const float rotation[3] = { 0.0f, 0.0f, 90.0f };
	const float scale[3] = { 1.0f, 1.0f, 1.0f };
	float matrix[16] = {};
	engine::level_transform_build(position, rotation, scale, matrix);
	//数值按 cos/sin(π/2) 的浮点结果比对
	const float zero = 0.0f;
	const float one = 1.0f;
	EXPECT_TRUE(bit_equal(matrix[0], std::cos(3.14159265f / 2.0f)));
	EXPECT_TRUE(bit_equal(matrix[1], one));
	EXPECT_TRUE(bit_equal(matrix[4], -one));
	EXPECT_TRUE(bit_equal(matrix[5], std::cos(3.14159265f / 2.0f)));
	EXPECT_TRUE(bit_equal(matrix[10], one));
	//位移为零、齐次末项为 1
	EXPECT_TRUE(bit_equal(matrix[12], zero));
	EXPECT_TRUE(bit_equal(matrix[15], one));
}

//随机关卡往返：固定种子生成 16 组，逐位一致
TEST(EngineSystem_LevelText_Test, 关卡随机往返)
{
	std::mt19937 generator(20261103u);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> positive(0.01f, 20.0f);
	std::uniform_int_distribution<int> count(0, 3);
	for (uint32_t round = 0; round < 16; ++round)
	{
		engine::Level_Data level;
		level.name = "随机" + std::to_string(round);
		//相机
		for (std::size_t index = 0; index < 3; ++index)
		{
			level.camera.position[index] = unit(generator);
			level.camera.target[index] = unit(generator);
			level.camera.up[index] = unit(generator);
			level.light.direction[index] = unit(generator);
			level.light.color[index] = std::abs(unit(generator));
			level.light.fill_direction[index] = unit(generator);
			level.light.fill_color[index] = std::abs(unit(generator));
			level.light.ambient_color[index] = std::abs(unit(generator));
			level.light.ambient_ground[index] = std::abs(unit(generator));
			level.light.toon_shadow_tint[index] = std::abs(unit(generator));
			level.light.outline_color[index] = std::abs(unit(generator));
			level.fog.color[index] = std::abs(unit(generator));
		}
		for (std::size_t index = 0; index < 4; ++index)
			level.camera.clear_color[index] = std::abs(unit(generator));
		level.camera.fov = positive(generator);
		level.camera.near_plane = positive(generator) * 0.01f;
		level.camera.far_plane = positive(generator) * 10.0f;
		level.camera.ortho = (round % 2u) != 0u;
		level.camera.ortho_height = positive(generator);
		level.camera.aspect = positive(generator);
		//光照标量
		level.light.intensity = positive(generator);
		level.light.ambient = std::abs(unit(generator));
		level.light.specular_intensity = std::abs(unit(generator));
		level.light.specular_shininess = positive(generator);
		level.light.attenuation_mode = round % 2u;
		level.light.ambient_hemisphere = std::abs(unit(generator));
		level.light.fill_intensity = std::abs(unit(generator));
		level.light.directional_specular = std::abs(unit(generator));
		level.light.fill_specular = std::abs(unit(generator));
		level.light.shadow_enabled = round % 2u;
		level.light.shadow_bias = std::abs(unit(generator)) * 0.05f;
		level.light.shadow_softness = std::abs(unit(generator));
		level.light.shadow_strength = std::abs(unit(generator));
		level.light.shadow_resolution = 512 * (1 + static_cast<int>(round % 3u));
		level.light.spot_shadow_enabled = round % 2u;
		level.light.spot_shadow_bias = std::abs(unit(generator)) * 0.05f;
		level.light.spot_shadow_softness = std::abs(unit(generator));
		level.light.spot_shadow_strength = std::abs(unit(generator));
		level.light.spot_shadow_index = static_cast<int>(round % 8u);
		level.light.spot_shadow_all = round % 2u;
		level.light.spot_shadow_bias_auto = round % 2u;
		level.light.pbr_enabled = round % 2u;
		level.light.pbr_roughness = 0.1f + std::abs(unit(generator));
		level.light.pbr_metallic = std::abs(unit(generator));
		level.light.toon_steps = round % 9u;
		level.light.toon_rim = std::abs(unit(generator));
		level.light.toon_specular = std::abs(unit(generator));
		level.light.outline_width = std::abs(unit(generator));
		//光源槽位
		const int point_count = count(generator);
		for (int index = 0; index < point_count; ++index)
		{
			for (std::size_t axis = 0; axis < 3; ++axis)
			{
				level.light.point_position[index][axis] = unit(generator) * 10.0f;
				level.light.point_color[index][axis] = std::abs(unit(generator));
			}
			level.light.point_radius[index] = positive(generator);
			level.light.point_intensity[index] = positive(generator);
			level.light.point_attenuation[index] = round % 3u;
			level.light.point_specular[index] = std::abs(unit(generator));
			level.light.point_shininess[index] = positive(generator);
		}
		level.light.point_count = static_cast<uint32_t>(point_count);
		const int spot_count = count(generator);
		for (int index = 0; index < spot_count; ++index)
		{
			for (std::size_t axis = 0; axis < 3; ++axis)
			{
				level.light.spot_position[index][axis] = unit(generator) * 10.0f;
				level.light.spot_direction[index][axis] = unit(generator);
				level.light.spot_color[index][axis] = std::abs(unit(generator));
			}
			level.light.spot_radius[index] = positive(generator);
			level.light.spot_intensity[index] = positive(generator);
			level.light.spot_inner_deg[index] = 1.0f + std::abs(unit(generator)) * 10.0f;
			level.light.spot_outer_deg[index] = 20.0f + std::abs(unit(generator)) * 60.0f;
			level.light.spot_attenuation[index] = round % 3u;
			level.light.spot_specular[index] = std::abs(unit(generator));
			level.light.spot_shininess[index] = positive(generator);
		}
		level.light.spot_count = static_cast<uint32_t>(spot_count);
		//雾
		level.fog.density = std::abs(unit(generator));
		level.fog.start = std::abs(unit(generator)) * 5.0f;
		level.fog.end = level.fog.start + positive(generator);
		level.fog.mode = round % 2u;
		level.fog.height_falloff = std::abs(unit(generator));
		//材质实例
		for (int index = 0; index < count(generator); ++index)
		{
			engine::Level_Material material;
			material.实例名 = "实例" + std::to_string(index);
			material.模板 = "模板" + std::to_string(round);
			material.use_tint = true;
			for (std::size_t channel = 0; channel < 4; ++channel)
				material.tint[channel] = std::abs(unit(generator));
			material.use_emissive = true;
			material.emissive = std::abs(unit(generator));
			material.use_toon = true;
			material.toon_steps = static_cast<float>(round % 9u);
			material.toon_rim = std::abs(unit(generator));
			material.toon_specular = std::abs(unit(generator));
			material.use_pbr = true;
			material.pbr_roughness = 0.1f + std::abs(unit(generator));
			material.pbr_metallic = std::abs(unit(generator));
			material.use_clearcoat = true;
			material.clear_coat = 0.1f + std::abs(unit(generator)) * 0.8f;
			material.clear_coat_roughness = 0.05f + std::abs(unit(generator)) * 0.9f;
			level.materials.push_back(material);
		}
		//资产行
		level.asset_lines.push_back("mesh 网格" + std::to_string(round) + " 网格" + std::to_string(round) + ".obj");
		const std::string name = "随机" + std::to_string(round) + ".level";
		expect_roundtrip(level, name.c_str());
	}
}
