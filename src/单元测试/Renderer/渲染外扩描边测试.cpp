/*
反向外扩描边测试（L2 隐藏窗口 + 像素回读；第二百二十八轮）
覆盖：默认关闭（只登记主体项、画面无描边色）；开启后额外登记描边项且轮廓出现描边色；
      outline_clear 复位；宽度与颜色非法值归一（负值 / 超界 / 非有限值）。
断言对 BGRA / RGBA 通道序无关（只用「最大通道 ≥ 200 且次小通道 ≤ 30」判定纯红描边色）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

//获取渲染器门面（含外扩描边接口）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = WINDOW_WIDTH;
		config.height = WINDOW_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数（RGBA8 紧排）
	uint32_t pixel_bytes(void)
	{
		return WINDOW_WIDTH * WINDOW_HEIGHT * 4;
	}

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		//行内偏移（每行宽 × 4 字节）
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	/*
	统计「纯红」像素数（容差口径）
	纯红描边输出为 (255, 0, 0)；排序后「最大通道 ≥ 200 且次小通道 ≤ 30」即命中，
	故与 BGRA / RGBA 通道序无关，且能排除清屏色（sRGB 下最小通道约 63）与灰白几何。
	*/
	uint32_t red_count(const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			std::array<uint8_t, 4> channels = { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
			std::sort(channels.begin(), channels.end());
			//最大通道足够亮、次小通道足够暗即为纯色（红 / 蓝 / 绿任一单通道高亮）
			if (channels[3] >= 200 && channels[1] <= 30)
				++count;
		}
		return count;
	}
}

//反向外扩描边测试夹具
class Renderer_Outline_Test : public ::testing::Test
{
};

//默认关闭：只登记一个主体项，画面无描边色
TEST_F(Renderer_Outline_Test, 默认关闭不登记描边项)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//默认外扩宽度为 0
	EXPECT_EQ(renderer.outline_width(), 0.0f);
	//加载立方体并登记绘制
	engine::Mesh_Handle cube = 0;
	ASSERT_TRUE(renderer.mesh_cube_load(cube, 0.5f));
	ASSERT_TRUE(renderer.mesh_draw(cube, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//关闭时只登记主体项
	EXPECT_EQ(renderer.frame_stats_get().last_draw_items, 1u);
	//画面不含纯色（描边色）像素
	EXPECT_EQ(red_count(pixels), 0u);
	renderer.mesh_unload(cube);
}

//开启后：额外登记一条描边项，轮廓出现描边色；outline_clear 后恢复
TEST_F(Renderer_Outline_Test, 开启后轮廓出现描边色)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载立方体
	engine::Mesh_Handle cube = 0;
	ASSERT_TRUE(renderer.mesh_cube_load(cube, 0.5f));

	//第一帧：开启 0.5 世界单位外扩 ＋ 纯红描边
	const float red[3] = { 1.0f, 0.0f, 0.0f };
	renderer.outline_set(0.5f, red);
	EXPECT_NEAR(renderer.outline_width(), 0.5f, 1e-6f);
	ASSERT_TRUE(renderer.mesh_draw(cube, nullptr));
	std::vector<uint8_t> outline_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(outline_pixels.data(), static_cast<uint32_t>(outline_pixels.size())));
	//描边项 ＋ 主体项各一
	EXPECT_EQ(renderer.frame_stats_get().last_draw_items, 2u);
	//轮廓处应出现成片纯红像素
	EXPECT_GT(red_count(outline_pixels), 20u) << "外扩描边未产生可见轮廓";

	//第二帧：关闭描边，描边项消失且画面无描边色
	renderer.outline_clear();
	ASSERT_TRUE(renderer.mesh_draw(cube, nullptr));
	std::vector<uint8_t> plain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(plain_pixels.data(), static_cast<uint32_t>(plain_pixels.size())));
	EXPECT_EQ(renderer.frame_stats_get().last_draw_items, 1u);
	EXPECT_EQ(red_count(plain_pixels), 0u);
	renderer.mesh_unload(cube);
}

//宽度与颜色归一：负值 / 超界 / 非有限值
TEST_F(Renderer_Outline_Test, 宽度与颜色非法值归一)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//负宽度按 0（关闭）
	renderer.outline_set(-0.5f, nullptr);
	EXPECT_EQ(renderer.outline_width(), 0.0f);
	//非有限宽度按 0
	renderer.outline_set(std::numeric_limits<float>::quiet_NaN(), nullptr);
	EXPECT_EQ(renderer.outline_width(), 0.0f);
	//超界宽度钳到 1
	const float over[3] = { 2.0f, -1.0f, 0.5f };
	renderer.outline_set(2.0f, over);
	EXPECT_NEAR(renderer.outline_width(), 1.0f, 1e-6f);
	//颜色逐分量钳到 [0, 1]（非有限值按 0）
	float color[3] = { -1.0f, -1.0f, -1.0f };
	renderer.outline_color(color);
	EXPECT_NEAR(color[0], 1.0f, 1e-6f);
	EXPECT_NEAR(color[1], 0.0f, 1e-6f);
	EXPECT_NEAR(color[2], 0.5f, 1e-6f);
}
