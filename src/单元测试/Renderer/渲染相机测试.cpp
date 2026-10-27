/*
渲染相机测试（L2 隐藏窗口）
覆盖：相机位置 / 注视点 / 视场角 / 近远平面设置后推进帧不报错；极端视场角不崩溃；
      未初始化时设置相机为安全无操作。
*/
#include <gtest/gtest.h>

//获取渲染器门面与配置
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
}

//相机测试夹具
class Renderer_Camera_Test : public ::testing::Test
{
};

//设置位置与注视点后推进一帧
TEST_F(Renderer_Camera_Test, 设置位置与注视点)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//把相机拉到前方
	renderer.camera_position_set(0.0f, 0.0f, 3.0f);
	//注视原点
	renderer.camera_target_set(0.0f, 0.0f, 0.0f);
	EXPECT_TRUE(renderer.frame_render());
}

//设置视场角与近远平面后推进一帧
TEST_F(Renderer_Camera_Test, 设置视场角与近远平面)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//约 60 度视场角
	renderer.camera_fov_set(1.0472f);
	//近远平面
	renderer.camera_near_far_set(0.1f, 100.0f);
	EXPECT_TRUE(renderer.frame_render());
}

//拉远拉近组合设置后连续多帧推进
TEST_F(Renderer_Camera_Test, 组合设置连续跑帧)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//连续五帧逐步拉近
	for (uint32_t index = 0; index < 5; ++index)
	{
		//逐帧把相机推近原点
		float distance = 5.0f - static_cast<float>(index);
		renderer.camera_position_set(0.0f, 0.0f, distance);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		EXPECT_TRUE(renderer.frame_render());
	}
}

//极端视场角设置后推进一帧不崩溃
TEST_F(Renderer_Camera_Test, 极端视场角不崩溃)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//很小的视场角
	renderer.camera_fov_set(0.1f);
	EXPECT_TRUE(renderer.frame_render());
	//很大的视场角
	renderer.camera_fov_set(3.0f);
	EXPECT_TRUE(renderer.frame_render());
}

//未初始化时各相机设置接口为安全无操作
TEST_F(Renderer_Camera_Test, 未初始化设置相机无副作用)
{
        engine::Renderer renderer;
        //未初始化时逐个调用各设置接口
        renderer.camera_position_set(1.0f, 2.0f, 3.0f);
        renderer.camera_target_set(0.0f, 0.0f, 0.0f);
        renderer.camera_fov_set(1.0f);
        renderer.camera_near_far_set(0.1f, 100.0f);
        //仍处于未初始化
        EXPECT_FALSE(renderer.valid());
}

/*
相机屏幕投影与反投影（第一百四十九轮）
口径：默认相机（位于 +Z 3、注视原点）：原点落在屏幕中心（0.5 / 0.5）且深度在 (0,1)；
      x = 1 的点落在右半（u > 0.5）；相机背后（z = 10）返回假且不改出参；
      unproject / project 往返误差 ≤ 1e-3；未初始化返回假。
*/
TEST_F(Renderer_Camera_Test, 相机屏幕投影与反投影)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//原点 → 屏幕中心
	const float origin[3] = { 0.0f, 0.0f, 0.0f };
	float u = -1.0f;
	float v = -1.0f;
	float depth = -1.0f;
	ASSERT_TRUE(renderer.camera_project(origin, &u, &v, &depth));
	EXPECT_NEAR(u, 0.5f, 1e-3f) << "原点未落在屏幕中心";
	EXPECT_NEAR(v, 0.5f, 1e-3f) << "原点未落在屏幕中心";
	EXPECT_GT(depth, 0.0f);
	EXPECT_LT(depth, 1.0f);
	//右侧点 → u > 0.5 且 v 仍居中
	const float right[3] = { 1.0f, 0.0f, 0.0f };
	ASSERT_TRUE(renderer.camera_project(right, &u, &v, nullptr));
	EXPECT_GT(u, 0.5f) << "右侧点未落在右半屏";
	EXPECT_NEAR(v, 0.5f, 1e-3f);
	//相机背后 → 假且出参不变
	const float behind[3] = { 0.0f, 0.0f, 10.0f };
	float untouched = 0.25f;
	EXPECT_FALSE(renderer.camera_project(behind, &untouched, nullptr, nullptr));
	EXPECT_FLOAT_EQ(untouched, 0.25f) << "失败时不应改写出参";
	//往返（屏幕 → 世界 → 屏幕）
	const float screen_u = 0.25f;
	const float screen_v = 0.75f;
	const float screen_depth = 0.6f;
	float world[3] = {};
	ASSERT_TRUE(renderer.camera_unproject(screen_u, screen_v, screen_depth, world));
	float round_u = 0.0f;
	float round_v = 0.0f;
	float round_depth = 0.0f;
	ASSERT_TRUE(renderer.camera_project(world, &round_u, &round_v, &round_depth));
	EXPECT_NEAR(round_u, screen_u, 1e-3f);
	EXPECT_NEAR(round_v, screen_v, 1e-3f);
	EXPECT_NEAR(round_depth, screen_depth, 1e-3f);
	//未初始化：两入口均返回假
	engine::Renderer uninitialized;
	EXPECT_FALSE(uninitialized.camera_project(origin, &u, &v, &depth));
	EXPECT_FALSE(uninitialized.camera_unproject(0.5f, 0.5f, 0.5f, world));
}

/*
世界空间 AABB 视锥可见性（第一百六十轮）
口径：默认相机（0,0,3）朝 −Z、近 0.1 / 远 100、垂直视场 60°：
      原点小盒可见；相机背后 / 远平面之外 / 横向视场之外不可见；未初始化与空入参返回假。
*/
TEST_F(Renderer_Camera_Test, 包围盒视锥可见性)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//原点附近的小盒（可见）
	const float center_min[3] = { -0.5f, -0.5f, -0.5f };
	const float center_max[3] = { 0.5f, 0.5f, 0.5f };
	EXPECT_TRUE(renderer.camera_aabb_visible(center_min, center_max));
	//相机背后（z = 10 > 相机 z = 3）
	const float behind_min[3] = { -0.5f, -0.5f, 9.5f };
	const float behind_max[3] = { 0.5f, 0.5f, 10.5f };
	EXPECT_FALSE(renderer.camera_aabb_visible(behind_min, behind_max));
	//远平面之外（距相机约 150）
	const float far_min[3] = { -0.5f, -0.5f, -150.5f };
	const float far_max[3] = { 0.5f, 0.5f, -149.5f };
	EXPECT_FALSE(renderer.camera_aabb_visible(far_min, far_max));
	//横向视场之外（x = 50）
	const float side_min[3] = { 49.5f, -0.5f, -1.0f };
	const float side_max[3] = { 50.5f, 0.5f, 0.0f };
	EXPECT_FALSE(renderer.camera_aabb_visible(side_min, side_max));
	//远但仍在视锥内（z = −50）
	const float inside_min[3] = { -0.5f, -0.5f, -50.5f };
	const float inside_max[3] = { 0.5f, 0.5f, -49.5f };
	EXPECT_TRUE(renderer.camera_aabb_visible(inside_min, inside_max));
	//空入参 / 未初始化
	EXPECT_FALSE(renderer.camera_aabb_visible(nullptr, center_max));
	EXPECT_FALSE(renderer.camera_aabb_visible(center_min, nullptr));
	engine::Renderer uninitialized;
	EXPECT_FALSE(uninitialized.camera_aabb_visible(center_min, center_max));
}

/*
相机矩阵导出（第一百六十一轮）
口径：导出的「视图 × 投影」矩阵把世界原点投到屏幕中心（NDC 近零），且深度落在 0..1；
      未初始化 / 空出参返回假。
*/
TEST_F(Renderer_Camera_Test, 相机矩阵导出)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	float matrix[16] = {};
	ASSERT_TRUE(renderer.camera_view_proj(matrix));
	//矩阵非零（存在有效投影）
	float magnitude = 0.0f;
	for (int index = 0; index < 16; ++index)
		magnitude += matrix[index] < 0.0f ? -matrix[index] : matrix[index];
	EXPECT_GT(magnitude, 0.1f) << "导出矩阵全零";

	//手工变换世界原点（列主序乘法）
	const float origin[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float clip[4] = {};
	for (int row = 0; row < 4; ++row)
	{
		float sum = 0.0f;
		for (int column = 0; column < 4; ++column)
			sum += matrix[column * 4 + row] * origin[column];
		clip[row] = sum;
	}
	EXPECT_GT(clip[3], 0.0f) << "原点在相机背后";
	EXPECT_NEAR(clip[0] / clip[3], 0.0f, 1e-3f) << "原点未投向屏幕中心";
	EXPECT_NEAR(clip[1] / clip[3], 0.0f, 1e-3f) << "原点未投向屏幕中心";
	float depth = clip[2] / clip[3];
	EXPECT_GT(depth, 0.0f);
	EXPECT_LT(depth, 1.0f);

	//未初始化 / 空出参
	engine::Renderer uninitialized;
	EXPECT_FALSE(uninitialized.camera_view_proj(matrix));
	EXPECT_FALSE(renderer.camera_view_proj(nullptr));
}

/*
正交投影与相机状态查询（第一百六十四轮）
口径：默认 fov ≈ 1.0472 弧度、透视模式、正交高度 2.0；
      设置 fov / 正交后查询一致；正交高度非正时忽略本次设置；未初始化查询返回假。
*/
TEST_F(Renderer_Camera_Test, 正交投影与状态查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认状态
	float fov = 0.0f;
	ASSERT_TRUE(renderer.camera_fov_get(&fov));
	EXPECT_NEAR(fov, 1.0472f, 1e-3f);
	bool ortho_enabled = true;
	float ortho_height = 0.0f;
	ASSERT_TRUE(renderer.camera_ortho_get(&ortho_enabled, &ortho_height));
	EXPECT_FALSE(ortho_enabled);
	EXPECT_NEAR(ortho_height, 2.0f, 1e-3f);

	//设置 fov 后查询一致
	renderer.camera_fov_set(0.8f);
	ASSERT_TRUE(renderer.camera_fov_get(&fov));
	EXPECT_NEAR(fov, 0.8f, 1e-3f);

	//开启正交并设高度
	renderer.camera_ortho_set(true, 3.0f);
	ASSERT_TRUE(renderer.camera_ortho_get(&ortho_enabled, &ortho_height));
	EXPECT_TRUE(ortho_enabled);
	EXPECT_NEAR(ortho_height, 3.0f, 1e-3f);
	//正交下世界原点仍投在屏幕中心
	float u = -1.0f;
	float v = -1.0f;
	const float origin[3] = { 0.0f, 0.0f, 0.0f };
	ASSERT_TRUE(renderer.camera_project(origin, &u, &v, nullptr));
	EXPECT_NEAR(u, 0.5f, 1e-3f);
	EXPECT_NEAR(v, 0.5f, 1e-3f);

	//高度非正：忽略本次设置（保持 3.0 与开启）
	renderer.camera_ortho_set(true, 0.0f);
	ASSERT_TRUE(renderer.camera_ortho_get(&ortho_enabled, &ortho_height));
	EXPECT_TRUE(ortho_enabled);
	EXPECT_NEAR(ortho_height, 3.0f, 1e-3f);

	//关闭正交回到透视
	renderer.camera_ortho_set(false, 3.0f);
	ASSERT_TRUE(renderer.camera_ortho_get(&ortho_enabled, nullptr));
	EXPECT_FALSE(ortho_enabled);

	//未初始化查询返回假
	engine::Renderer cold;
	EXPECT_FALSE(cold.camera_fov_get(&fov));
	EXPECT_FALSE(cold.camera_ortho_get(&ortho_enabled, &ortho_height));
	//未初始化设置无副作用（不崩溃即可）
	cold.camera_ortho_set(true, 2.0f);
}

/*
视深可见高度查询（第一百六十五轮）
口径：透视（fov ≈ 1.0472）下视深 3 的可见高度 ≈ 2·tan(30°)·3 ≈ 3.464，且随视深线性；
      正交（高度 2.0）恒为 2.0；视深非正按 0；未初始化返回假。
*/
TEST_F(Renderer_Camera_Test, 视深可见高度查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//透视：视深 3 → 约 3.464
	float height = 0.0f;
	ASSERT_TRUE(renderer.camera_world_height_at(3.0f, &height));
	EXPECT_NEAR(height, 3.4641f, 1e-2f);
	//透视：视深翻倍 → 高度翻倍
	float doubled = 0.0f;
	ASSERT_TRUE(renderer.camera_world_height_at(6.0f, &doubled));
	EXPECT_NEAR(doubled, height * 2.0f, 1e-2f);
	//视深非正按 0
	float zero = -1.0f;
	ASSERT_TRUE(renderer.camera_world_height_at(-2.0f, &zero));
	EXPECT_NEAR(zero, 0.0f, 1e-4f);
	//出参可为空（仅查询状态）
	EXPECT_TRUE(renderer.camera_world_height_at(1.0f, nullptr));

	//正交：恒为正交高度（与视深无关）
	renderer.camera_ortho_set(true, 2.0f);
	float near_height = 0.0f;
	float far_height = 0.0f;
	ASSERT_TRUE(renderer.camera_world_height_at(1.0f, &near_height));
	ASSERT_TRUE(renderer.camera_world_height_at(50.0f, &far_height));
	EXPECT_NEAR(near_height, 2.0f, 1e-3f);
	EXPECT_NEAR(far_height, 2.0f, 1e-3f);

	//未初始化
	engine::Renderer cold;
	EXPECT_FALSE(cold.camera_world_height_at(3.0f, &height));
}
