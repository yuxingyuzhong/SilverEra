/*
渲染层管线缓存与阴影视锥测试（第一百零六～一百零七轮）
覆盖：管线缓存文件写入 / 载入后仍可渲染；阴影视锥范围默认值、设置与非法值忽略。
说明：本用例只按渲染层公共面编写，不涉及 Vulkan 细节。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
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

	//测试用管线缓存文件（相对 Test 层根；ASCII 路径避免编码问题）
	const char* const PIPELINE_CACHE_PATH = "out/测试管线缓存.bin";

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

//管线缓存与阴影范围测试夹具
class Renderer_Cache_Shadow_Test : public ::testing::Test
{
};

//管线缓存：首次运行写盘，二次运行载入后仍可正常出帧
TEST_F(Renderer_Cache_Shadow_Test, 管线缓存写入与载入)
{
	//先清理旧文件
	std::error_code error;
	std::filesystem::remove(std::filesystem::u8path(PIPELINE_CACHE_PATH), error);

	//第一次运行：创建并写回缓存
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.pipeline_cache_path = PIPELINE_CACHE_PATH;
		ASSERT_TRUE(renderer.init(config));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, triangle_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render());
		renderer.mesh_unload(handle);
		renderer.shutdown();
	}
	//缓存文件应已生成且非空
	ASSERT_TRUE(std::filesystem::exists(std::filesystem::u8path(PIPELINE_CACHE_PATH)));
	EXPECT_GT(std::filesystem::file_size(std::filesystem::u8path(PIPELINE_CACHE_PATH)), 0u);

	//第二次运行：载入同一缓存并渲染
	{
		engine::Renderer renderer;
		engine::Renderer_Config config = hidden_config();
		config.pipeline_cache_path = PIPELINE_CACHE_PATH;
		ASSERT_TRUE(renderer.init(config));
		engine::Mesh_Handle handle = 0;
		ASSERT_TRUE(renderer.mesh_load(handle, triangle_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		ASSERT_TRUE(renderer.frame_render());
		renderer.mesh_unload(handle);
	}
}

//阴影视锥：默认值、合法设置生效、非法设置被整次忽略
TEST_F(Renderer_Cache_Shadow_Test, 阴影视锥范围可配)
{
	engine::Renderer renderer;
	//未初始化时返回默认值
	float half_extent = 0.0f;
	float near_plane = 0.0f;
	float far_plane = 0.0f;
	renderer.shadow_range_get(&half_extent, &near_plane, &far_plane);
	EXPECT_FLOAT_EQ(half_extent, 12.0f);
	EXPECT_FLOAT_EQ(near_plane, 1.0f);
	EXPECT_FLOAT_EQ(far_plane, 60.0f);

	ASSERT_TRUE(renderer.init(hidden_config()));
	//初始化后仍为默认
	renderer.shadow_range_get(&half_extent, &near_plane, &far_plane);
	EXPECT_FLOAT_EQ(half_extent, 12.0f);

	//合法设置生效
	renderer.shadow_range_set(8.0f, 0.5f, 40.0f);
	renderer.shadow_range_get(&half_extent, &near_plane, &far_plane);
	EXPECT_FLOAT_EQ(half_extent, 8.0f);
	EXPECT_FLOAT_EQ(near_plane, 0.5f);
	EXPECT_FLOAT_EQ(far_plane, 40.0f);

	//非法设置（半宽 0 / 近 0 / 远小于近）整次忽略
	renderer.shadow_range_set(0.0f, 0.5f, 40.0f);
	renderer.shadow_range_set(8.0f, 0.0f, 40.0f);
	renderer.shadow_range_set(8.0f, 10.0f, 5.0f);
	renderer.shadow_range_get(&half_extent, &near_plane, &far_plane);
	EXPECT_FLOAT_EQ(half_extent, 8.0f);
	EXPECT_FLOAT_EQ(near_plane, 0.5f);
	EXPECT_FLOAT_EQ(far_plane, 40.0f);

	//开启阴影后仍可正常出帧（视锥改用新范围）
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, triangle_data()));
	renderer.shadow_enable_set(true);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GE(renderer.shadow_pass_count(), 1u);
	renderer.shadow_enable_set(false);
	renderer.mesh_unload(handle);
}
