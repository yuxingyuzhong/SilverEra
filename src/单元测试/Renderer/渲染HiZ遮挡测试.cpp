/*
渲染 Hi-Z 遮挡测试（第二百五十五轮）
覆盖：CPU 降采样与着色器同口径（2×2 取最大、边界钳制）、矩形级选择、
遮挡判定（完全遮挡 / 在前方 / 容差内不剔）、跨近平面与视图外保守判可见、投影矩形几何。
断言口径：遮挡测试只做保守剔除（可见侧多、剔除侧少），故「在前方 / 容差内 / 跨近平面」必须判可见。
*/
#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

//遮挡测试公开面（Vulkan-free）
#include "Application/Renderer/src/render/HiZ/遮挡测试.h"

namespace
{
	//金字塔第 0 级尺寸（64×64，4 级：64 / 32 / 16 / 8）
	constexpr uint32_t PYRAMID_WIDTH = 64;
	constexpr uint32_t PYRAMID_HEIGHT = 64;
	constexpr uint32_t PYRAMID_LEVELS = 4;

	//正交投影（w 恒为 1）：ndc = 世界坐标，深度直接取 z（0..1）
	std::array<float, 16> orthographic_matrix(void)
	{
		return std::array<float, 16>{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};
	}

	//逐级紧排尺寸之和
	uint32_t pyramid_total_count(uint32_t width, uint32_t height, uint32_t levels)
	{
		uint32_t total = 0;
		for (uint32_t level = 0; level < levels; ++level)
			total += (std::max)(1u, width >> level) * (std::max)(1u, height >> level);
		return total;
	}

	/*
	构造测试金字塔
	第 0 级填充 fill；其余级用 CPU 降采样逐级生成（与 GPU 同口径）。
	*/
	std::vector<float> pyramid_build(float fill)
	{
		std::vector<float> data(pyramid_total_count(PYRAMID_WIDTH, PYRAMID_HEIGHT, PYRAMID_LEVELS), fill);
		//逐级降采样
		uint32_t offset = 0;
		uint32_t source_width = PYRAMID_WIDTH;
		uint32_t source_height = PYRAMID_HEIGHT;
		for (uint32_t level = 1; level < PYRAMID_LEVELS; ++level)
		{
			const uint32_t dest_width = (std::max)(1u, PYRAMID_WIDTH >> level);
			const uint32_t dest_height = (std::max)(1u, PYRAMID_HEIGHT >> level);
			const uint32_t source_offset = offset;
			offset += source_width * source_height;
			//上一级 → 本级
			const bool ok = engine::hiz_pyramid_downsample(data.data() + source_offset, source_width, source_height,
				data.data() + offset, dest_width, dest_height);
			EXPECT_TRUE(ok);
			source_width = dest_width;
			source_height = dest_height;
		}
		return data;
	}
}

//Hi-Z 遮挡测试夹具
class Renderer_HiZ_Occlusion_Test : public ::testing::Test
{
};

/*
CPU 降采样与着色器同口径
口径：2×2 邻域取最大值；邻域越界钳到最后一行 / 列；同尺寸调用退化为逐纹素拷贝。
*/
TEST_F(Renderer_HiZ_Occlusion_Test, 降采样与着色器同口径)
{
	//8×8 源：值 = 行号（逐行递增）
	std::vector<float> source(64, 0.0f);
	for (uint32_t y = 0; y < 8; ++y)
		for (uint32_t x = 0; x < 8; ++x)
			source[y * 8 + x] = static_cast<float>(y);
	//4×4 目标：每个纹素 = 2×2 的最大值（下一行）
	std::vector<float> dest(16, -1.0f);
	ASSERT_TRUE(engine::hiz_pyramid_downsample(source.data(), 8, 8, dest.data(), 4, 4));
	for (uint32_t y = 0; y < 4; ++y)
		for (uint32_t x = 0; x < 4; ++x)
			EXPECT_FLOAT_EQ(dest[y * 4 + x], static_cast<float>(y * 2 + 1)) << "纹素 " << x << "," << y;
	//同尺寸调用：每纹素取 (2x,2x+1)×(2y,2y+1) 邻域最大值（越界钳到末行 / 列；略偏保守，非逐纹素恒等）
	std::vector<float> copy(64, -1.0f);
	ASSERT_TRUE(engine::hiz_pyramid_downsample(source.data(), 8, 8, copy.data(), 8, 8));
	for (uint32_t y = 0; y < 8; ++y)
		for (uint32_t x = 0; x < 8; ++x)
		{
			//2×2 邻域（每轴 2x、2x+1，越界钳到最后一行 / 列）
			const uint32_t x0 = (std::min)(x * 2, 7u);
			const uint32_t x1 = (std::min)(x * 2 + 1, 7u);
			const uint32_t y0 = (std::min)(y * 2, 7u);
			const uint32_t y1 = (std::min)(y * 2 + 1, 7u);
			const float expected = (std::max)((std::max)(source[y0 * 8 + x0], source[y0 * 8 + x1]),
				(std::max)(source[y1 * 8 + x0], source[y1 * 8 + x1]));
			EXPECT_FLOAT_EQ(copy[y * 8 + x], expected) << "纹素 " << x << "," << y;
		}
	//非法入参
	EXPECT_FALSE(engine::hiz_pyramid_downsample(nullptr, 8, 8, dest.data(), 4, 4));
	EXPECT_FALSE(engine::hiz_pyramid_downsample(source.data(), 0, 8, dest.data(), 4, 4));
}

//矩形级选择：1 像素 → 0 级；3 像素 → 2 级；超上限钳到末级
TEST_F(Renderer_HiZ_Occlusion_Test, 矩形级选择)
{
	EXPECT_EQ(engine::hiz_level_for_rect(1.0f, 1.0f, PYRAMID_LEVELS), 0u);
	EXPECT_EQ(engine::hiz_level_for_rect(2.5f, 2.5f, PYRAMID_LEVELS), 2u);
	EXPECT_EQ(engine::hiz_level_for_rect(64.0f, 64.0f, PYRAMID_LEVELS), PYRAMID_LEVELS - 1);
	//0 级金字塔恒取 0
	EXPECT_EQ(engine::hiz_level_for_rect(64.0f, 64.0f, 1u), 0u);
}

/*
遮挡判定：完全遮挡剔除、在前方可见、容差内可见
口径：金字塔面在 0.2；对象最近深度 0.4 → 剔除；0.1 → 可见；0.201（差值在 bias 内）→ 可见。
*/
TEST_F(Renderer_HiZ_Occlusion_Test, 遮挡判定三态)
{
	std::vector<float> data = pyramid_build(0.2f);
	engine::HiZ_Pyramid_View pyramid{};
	pyramid.data = data.data();
	pyramid.width = PYRAMID_WIDTH;
	pyramid.height = PYRAMID_HEIGHT;
	pyramid.levels = PYRAMID_LEVELS;
	//视图中央的小盒
	const float aabb_min[3] = { -0.02f, -0.02f, 0.4f };
	const float aabb_max[3] = { 0.02f, 0.02f, 0.6f };
	std::array<float, 16> matrix = orthographic_matrix();
	engine::HiZ_Screen_Rect rect;
	ASSERT_TRUE(engine::hiz_screen_rect_build(rect, matrix.data(), aabb_min, aabb_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
	EXPECT_FALSE(rect.cross_near);
	//完全遮挡：剔除
	EXPECT_EQ(engine::hiz_occlusion_test(pyramid, rect), engine::HiZ_Visibility::Occluded);
	//对象位于遮挡面前方：可见
	const float front_min[3] = { -0.02f, -0.02f, 0.1f };
	const float front_max[3] = { 0.02f, 0.02f, 0.2f };
	ASSERT_TRUE(engine::hiz_screen_rect_build(rect, matrix.data(), front_min, front_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
	EXPECT_EQ(engine::hiz_occlusion_test(pyramid, rect), engine::HiZ_Visibility::Visible);
	//与遮挡面几乎同深度（差值在 bias 内）：可见
	const float near_min[3] = { -0.02f, -0.02f, 0.201f };
	const float near_max[3] = { 0.02f, 0.02f, 0.201f };
	ASSERT_TRUE(engine::hiz_screen_rect_build(rect, matrix.data(), near_min, near_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
	EXPECT_EQ(engine::hiz_occlusion_test(pyramid, rect), engine::HiZ_Visibility::Visible);
}

/*
跨近平面与视图外：保守判可见
口径：任一角点 w ≤ 0 → cross_near 置真且判可见；矩形完全在视图外 → 判可见（交给视锥剔除）。
*/
TEST_F(Renderer_HiZ_Occlusion_Test, 跨近平面与视图外判可见)
{
	std::vector<float> data = pyramid_build(0.2f);
	engine::HiZ_Pyramid_View pyramid{};
	pyramid.data = data.data();
	pyramid.width = PYRAMID_WIDTH;
	pyramid.height = PYRAMID_HEIGHT;
	pyramid.levels = PYRAMID_LEVELS;
	//w = −1 的矩阵（全部角点跨近平面）
	std::array<float, 16> behind = orthographic_matrix();
	behind[15] = -1.0f;
	const float near_min[3] = { -0.02f, -0.02f, 0.4f };
	const float near_max[3] = { 0.02f, 0.02f, 0.6f };
	engine::HiZ_Screen_Rect rect;
	ASSERT_TRUE(engine::hiz_screen_rect_build(rect, behind.data(), near_min, near_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
	EXPECT_TRUE(rect.cross_near);
	EXPECT_EQ(engine::hiz_occlusion_test(pyramid, rect), engine::HiZ_Visibility::Visible);
	//视图外（x 远超 NDC 范围）
	std::array<float, 16> matrix = orthographic_matrix();
	const float outside_min[3] = { 5.0f, 5.0f, 0.4f };
	const float outside_max[3] = { 6.0f, 6.0f, 0.6f };
	ASSERT_TRUE(engine::hiz_screen_rect_build(rect, matrix.data(), outside_min, outside_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
	EXPECT_FALSE(rect.cross_near);
	EXPECT_EQ(engine::hiz_occlusion_test(pyramid, rect), engine::HiZ_Visibility::Visible);
}

/*
投影矩形几何
口径：正交矩阵下 NDC = 世界坐标；中央小盒映射到屏幕中央、最近深度取角点最小 z。
*/
TEST_F(Renderer_HiZ_Occlusion_Test, 投影矩形几何)
{
	std::array<float, 16> matrix = orthographic_matrix();
	const float aabb_min[3] = { -0.1f, -0.1f, 0.2f };
	const float aabb_max[3] = { 0.1f, 0.1f, 0.6f };
	engine::HiZ_Screen_Rect rect;
	ASSERT_TRUE(engine::hiz_screen_rect_build(rect, matrix.data(), aabb_min, aabb_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
	//中央对称
	EXPECT_NEAR(rect.min_x, 28.8f, 1e-3f);
	EXPECT_NEAR(rect.max_x, 35.2f, 1e-3f);
	EXPECT_NEAR(rect.min_y, 28.8f, 1e-3f);
	EXPECT_NEAR(rect.max_y, 35.2f, 1e-3f);
	//最近深度 = 最小 z
	EXPECT_NEAR(rect.nearest_depth, 0.2f, 1e-5f);
	//非法入参
	const float bad_min[3] = { 1.0f, 0.0f, 0.0f };
	const float bad_max[3] = { -1.0f, 0.0f, 0.0f };
	EXPECT_FALSE(engine::hiz_screen_rect_build(rect, matrix.data(), bad_min, bad_max, PYRAMID_WIDTH, PYRAMID_HEIGHT));
}
