/*
渲染层资源计数与链查询测试（第一百一十三～一百一十六轮）
覆盖：资源计数（网格 / 纹理 / 管线 / 字体 / 目标 / 材质）、聚光灯数量、后处理链各级采样数查询。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		return config;
	}

	//构造三角形网格
	engine::Mesh_Data triangle_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2 };
		return data;
	}
}

//资源与链查询测试夹具
class Renderer_Resource_Test : public ::testing::Test
{
};

//资源计数：随加载 / 卸载正确增减
TEST_F(Renderer_Resource_Test, 资源计数随加载卸载变化)
{
	engine::Renderer renderer;
	//未初始化全零
	engine::Renderer_Resource_Counts empty = renderer.resource_counts();
	EXPECT_EQ(empty.meshes, 0u);
	EXPECT_EQ(empty.textures, 0u);
	EXPECT_EQ(empty.pipelines, 0u);

	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Renderer_Resource_Counts initial = renderer.resource_counts();
	//初始化后：默认白纹理 ＋ 默认天空盒立方体、默认场景管线 ＋ 外扩描边管线（第二百二十八轮）、阴影目标各一（第一百二十五轮）
	EXPECT_EQ(initial.meshes, 0u);
	EXPECT_EQ(initial.textures, 2u);
	EXPECT_EQ(initial.pipelines, 2u);
	EXPECT_EQ(initial.fonts, 0u);
	EXPECT_EQ(initial.render_targets, 1u);
	EXPECT_EQ(initial.materials, 0u);

	//加载各类资源
	engine::Mesh_Handle mesh = 0;
	uint64_t texture = 0;
	uint64_t font = 0;
	uint64_t pipeline = 0;
	uint64_t target = 0;
	engine::Material_Handle material = 0;
	const uint8_t pixel[4] = { 255, 255, 255, 255 };
	ASSERT_TRUE(renderer.mesh_load(mesh, triangle_data()));
	ASSERT_TRUE(renderer.texture_load(texture, pixel, 1, 1));
	ASSERT_TRUE(renderer.font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16));
	ASSERT_TRUE(renderer.pipeline_load(pipeline, "后处理.vert.spv", "反色.frag.spv"));
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	ASSERT_TRUE(renderer.material_load(material, engine::Material_Data{}));

	engine::Renderer_Resource_Counts loaded = renderer.resource_counts();
	EXPECT_EQ(loaded.meshes, 1u);
	//默认白纹理 ＋ 默认天空盒立方体 ＋ 测试纹理 ＋ 字体图集
	EXPECT_EQ(loaded.textures, 4u);
	EXPECT_EQ(loaded.pipelines, 3u);
	EXPECT_EQ(loaded.fonts, 1u);
	EXPECT_EQ(loaded.render_targets, 2u);
	EXPECT_EQ(loaded.materials, 1u);

	//卸载后回落
	renderer.material_unload(material);
	renderer.render_target_destroy(target);
	renderer.pipeline_unload(pipeline);
	renderer.font_unload(font);
	renderer.texture_unload(texture);
	renderer.mesh_unload(mesh);
	engine::Renderer_Resource_Counts unloaded = renderer.resource_counts();
	EXPECT_EQ(unloaded.meshes, 0u);
	EXPECT_EQ(unloaded.textures, 2u);
	EXPECT_EQ(unloaded.pipelines, 2u);
	EXPECT_EQ(unloaded.fonts, 0u);
	EXPECT_EQ(unloaded.render_targets, 1u);
	EXPECT_EQ(unloaded.materials, 0u);
}

//聚光灯数量：仅统计 is_spot 为真的光源
TEST_F(Renderer_Resource_Test, 聚光灯数量统计)
{
	engine::Renderer renderer;
	EXPECT_EQ(renderer.spot_light_count(), 0u);
	ASSERT_TRUE(renderer.init(hidden_config()));
	EXPECT_EQ(renderer.spot_light_count(), 0u);

	const float position[3] = { 0.0f, 0.0f, 1.0f };
	const float direction[3] = { 0.0f, 0.0f, -1.0f };
	const float color[3] = { 1.0f, 1.0f, 1.0f };
	//一个点光源
	ASSERT_TRUE(renderer.light_add_point(position, color, 5.0f, 1.0f, 0.0f));
	EXPECT_EQ(renderer.spot_light_count(), 0u);
	//两个聚光灯
	ASSERT_TRUE(renderer.light_add_spot(position, direction, color, 5.0f, 1.0f, 20.0f, 30.0f, 0.0f));
	ASSERT_TRUE(renderer.light_add_spot(position, direction, color, 5.0f, 1.0f, 10.0f, 20.0f, 0.0f));
	EXPECT_EQ(renderer.spot_light_count(), 2u);
	EXPECT_EQ(renderer.point_light_count(), 3u);
	//清空后归零
	renderer.lights_clear();
	EXPECT_EQ(renderer.spot_light_count(), 0u);
}

//后处理链采样数查询：按级序写回并支持容量截断
TEST_F(Renderer_Resource_Test, 后处理链采样数查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//未设链返回 0
	uint32_t samples[4] = {};
	EXPECT_EQ(renderer.post_chain_sample_counts(samples, 4), 0u);

	//两条管线（单采样模式）
	uint64_t first = 0;
	uint64_t second = 0;
	ASSERT_TRUE(renderer.pipeline_load(first, "后处理.vert.spv", "反色.frag.spv"));
	ASSERT_TRUE(renderer.pipeline_load(second, "后处理.vert.spv", "灰度.frag.spv"));
	uint64_t chain[2] = { first, second };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));

	//单采样下两级均应为 1
	EXPECT_EQ(renderer.post_chain_sample_counts(samples, 4), 2u);
	EXPECT_EQ(samples[0], 1u);
	EXPECT_EQ(samples[1], 1u);
	//容量 1 只写第一级
	uint32_t one[1] = {};
	EXPECT_EQ(renderer.post_chain_sample_counts(one, 1), 1u);
	EXPECT_EQ(one[0], 1u);
	//空出参返回 0
	EXPECT_EQ(renderer.post_chain_sample_counts(nullptr, 4), 0u);

	renderer.post_chain_set(nullptr, 0);
	renderer.pipeline_unload(second);
	renderer.pipeline_unload(first);
}
