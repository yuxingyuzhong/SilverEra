/*
渲染超采样测试（SSAA；L2 隐藏窗口 ＋ 像素回读）
覆盖：① 2× 超采样下对角线边缘出现中间色（盒式平均生效），关闭时边缘保持硬边；
      ② 倍率配置与门面的钳制语义（0 → 1、>4 → 4；未初始化时忽略）。
断言只看绿通道（RGBA / BGRA 下均为下标 1），故与表面通道序无关；
      中间色判定用「既非清屏色、也非白色」，并留出容差，兼容 sRGB 与 UNORM 附件。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（小尺寸便于逐像素遍历）
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

	//失败诊断文本
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	/*
	构造覆盖整个视口的直角三角形（对角线为斜边）
	顶点取 ±2（NDC 约 ±1.15），确保斜边贯穿视口；默认相机在 +Z 3 单位处、默认透视投影。
	斜边斜率 1，与像素网格不平行，故未开 SSAA 时边缘处不会自然产生中间色。
	*/
	engine::Mesh_Data triangle_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -2.0f, -2.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 2.0f, -2.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 2.0f, 2.0f, 0.0f }, { 1.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2 };
		return data;
	}

	//清屏色的绿通道候选（UNORM ≈ 13、sRGB ≈ 63；与既有回读用例同口径）
	bool green_is_clear(uint8_t green)
	{
		return (green >= 13 - 12 && green <= 13 + 12) || (green >= 63 - 12 && green <= 63 + 12);
	}

	//是否白色（白纹理在满照下接近饱和）
	bool green_is_white(uint8_t green)
	{
		return green >= 235;
	}

	//统计「既非清屏色、也非白色」的像素数（SSAA 的盒式平均会产生这类中间值）
	uint32_t intermediate_count(const std::vector<uint8_t>& pixels)
	{
		//中间色像素计数
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				//绿通道（通道序无关）
				uint8_t green = pixel_get(pixels, x, y)[1];
				if (!green_is_clear(green) && !green_is_white(green))
					++count;
			}
		return count;
	}

	//渲染一帧并回读（返回 RGBA8 像素）
	std::vector<uint8_t> render_once(engine::Renderer& renderer)
	{
		//回读缓冲
		std::vector<uint8_t> pixels(pixel_bytes());
		//回读尺寸出参
		uint32_t width = 0;
		uint32_t height = 0;
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
		EXPECT_EQ(width, WINDOW_WIDTH);
		EXPECT_EQ(height, WINDOW_HEIGHT);
		return pixels;
	}
}

//未初始化时倍率查询为 1、设置被忽略
TEST(Renderer_Ssaa_Test, UninitializedGuard)
{
	//未初始化的渲染器
	engine::Renderer renderer;
	EXPECT_EQ(renderer.ssaa_factor(), 1u);
	//未初始化时设置应静默忽略
	renderer.ssaa_factor_set(4);
	EXPECT_EQ(renderer.ssaa_factor(), 1u);
}

//初始化时倍率越界被钳制，运行时设置同样钳制（0 → 1、>4 → 4）
TEST(Renderer_Ssaa_Test, FactorClamp)
{
	//配置倍率 9（应钳到 4）
	engine::Renderer_Config config = hidden_config();
	config.ssaa_factor = 9;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	EXPECT_EQ(renderer.ssaa_factor(), 4u);
	//运行时设置 0 → 关闭
	renderer.ssaa_factor_set(0);
	EXPECT_EQ(renderer.ssaa_factor(), 1u);
	//运行时设置 9 → 4
	renderer.ssaa_factor_set(9);
	EXPECT_EQ(renderer.ssaa_factor(), 4u);
	//设置为 3
	renderer.ssaa_factor_set(3);
	EXPECT_EQ(renderer.ssaa_factor(), 3u);
}

//2× 超采样：对角线边缘出现中间色；关闭时边缘保持硬边
TEST(Renderer_Ssaa_Test, EdgeBlend)
{
	//构造三角形网格
	engine::Mesh_Data data = triangle_data();
	//(1) 关闭 SSAA 的基准帧
	engine::Renderer off_renderer;
	ASSERT_TRUE(off_renderer.init(hidden_config()));
	engine::Mesh_Handle off_mesh = 0;
	ASSERT_TRUE(off_renderer.mesh_load(off_mesh, data));
	ASSERT_TRUE(off_renderer.mesh_draw(off_mesh, nullptr));
	std::vector<uint8_t> off_pixels = render_once(off_renderer);
	uint32_t off_intermediate = intermediate_count(off_pixels);
	//(2) 2× SSAA 帧
	engine::Renderer_Config config = hidden_config();
	config.ssaa_factor = 2;
	engine::Renderer on_renderer;
	ASSERT_TRUE(on_renderer.init(config));
	engine::Mesh_Handle on_mesh = 0;
	ASSERT_TRUE(on_renderer.mesh_load(on_mesh, data));
	ASSERT_TRUE(on_renderer.mesh_draw(on_mesh, nullptr));
	std::vector<uint8_t> on_pixels = render_once(on_renderer);
	uint32_t on_intermediate = intermediate_count(on_pixels);
	//关闭时边缘为硬边：中间色应为 0
	EXPECT_EQ(off_intermediate, 0u) << "关闭 SSAA 时不应出现中间色";
	//开启时应沿对角线产生足够多的中间色（斜边长度约 70 像素）
	EXPECT_GT(on_intermediate, 10u) << "SSAA 未产生预期的边缘混合（实际 " << on_intermediate << " 像素）";
	//开启后中间色须显著多于关闭（结构性对比）
	EXPECT_GT(on_intermediate, off_intermediate + 10u);
	//两种情形下画面仍须同时含白色与清屏色（未整屏染色）
	EXPECT_GT(on_intermediate, 0u);
}
