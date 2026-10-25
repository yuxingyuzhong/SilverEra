/*
渲染层后处理色彩校正 / 径向模糊 / 色温测试（第一百零八～一百一十轮）
覆盖：色彩校正的饱和度与亮度、径向模糊的画面差异、色温的红蓝偏移方向。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

//后处理链测试辅助（窗口尺寸、像素读取、四边形与最大通道差）
#include "渲染后处理链测试辅助.h"

namespace
{
	//登记「纹理四边形」场景（调用方自行绑定纹理）
	void quad_register(engine::Renderer& renderer, engine::Mesh_Handle& handle_out)
	{
		ASSERT_TRUE(renderer.mesh_load(handle_out, quad_data()));
		ASSERT_TRUE(renderer.mesh_draw(handle_out, nullptr));
	}

	//直接渲染到交换链并回读（无后处理链）
	std::vector<uint8_t> render_direct(engine::Renderer& renderer)
	{
		std::vector<uint8_t> pixels(pixel_bytes(), 0);
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		return pixels;
	}

	/*
	经单级后处理链渲染并回读
	frag 为片元着色器文件名；params 为三级后处理参数（阈值 / 软膝 / 强度）。
	*/
	std::vector<uint8_t> render_stage(engine::Renderer& renderer, const std::string& frag,
		float first, float second, float third)
	{
		//场景目标（与交换链同尺寸）
		uint64_t target = 0;
		EXPECT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
		//登记管线并设为单级链
		uint64_t pipeline = post_pipeline_register(renderer, frag);
		EXPECT_NE(pipeline, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pipeline, 1));
		//设置该级参数
		renderer.post_stage_param_set(0, first, second, third);
		//设定同帧后处理呈现并出帧
		renderer.render_target_present_set(target);
		std::vector<uint8_t> pixels(pixel_bytes(), 0);
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		//清理
		renderer.render_target_present_set(0);
		renderer.pipeline_unload(pipeline);
		renderer.render_target_destroy(target);
		return pixels;
	}
}

//色彩校正 / 径向模糊 / 色温测试夹具
class Renderer_Post_Grade_Test : public ::testing::Test
{
};

//色彩校正：饱和度归零使彩色变灰；亮度减半使像素变暗
TEST_F(Renderer_Post_Grade_Test, 色彩校正饱和度与亮度)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//纯红 1×1 纹理贴满四边形
	uint64_t texture = 0;
	const uint8_t red[4] = { 255, 0, 0, 255 };
	ASSERT_TRUE(renderer.texture_load(texture, red, 1, 1));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;
	quad_register(renderer, quad);

	//基准：直接交换链（红色）
	std::vector<uint8_t> reference = render_direct(renderer);
	std::array<uint8_t, 4> base = pixel_get(reference, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//交换链为 BGRA：下标 2 为红、下标 0 为蓝
	int base_spread = static_cast<int>(base[2]) - static_cast<int>(base[0]);
	EXPECT_GT(base_spread, 120) << "输入应为红色";

	//饱和度 0（对比度 1、亮度 1）→ 灰度
	quad_register(renderer, quad);
	std::vector<uint8_t> gray = render_stage(renderer, "色彩校正.frag.spv", 1.0f, 0.0f, 1.0f);
	std::array<uint8_t, 4> gray_pixel = pixel_get(gray, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	int gray_spread = static_cast<int>(gray_pixel[2]) - static_cast<int>(gray_pixel[0]);
	EXPECT_LE(std::abs(gray_spread), 6) << "饱和度 0 后应接近灰度";

	//亮度 0.5 → 主通道明显低于基准
	quad_register(renderer, quad);
	std::vector<uint8_t> dim = render_stage(renderer, "色彩校正.frag.spv", 1.0f, 1.0f, 0.5f);
	std::array<uint8_t, 4> dim_pixel = pixel_get(dim, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//以红通道（下标 2）比较亮度变化
	EXPECT_LT(dim_pixel[2], base[2]) << "亮度 0.5 应变暗";

	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

//径向模糊：棋盘格画面与基准存在明显差异
TEST_F(Renderer_Post_Grade_Test, 径向模糊产生画面差异)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//棋盘格纹理增强高频细节
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 32, 32, 4));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;
	quad_register(renderer, quad);
	std::vector<uint8_t> reference = render_direct(renderer);

	//径向模糊（阈值 0.8 → 位移比例 0.08，强度 1）
	quad_register(renderer, quad);
	std::vector<uint8_t> blurred = render_stage(renderer, "径向模糊.frag.spv", 0.8f, 0.0f, 1.0f);
	EXPECT_GT(max_channel_diff(reference, blurred), 20) << "径向模糊未产生可见变化";

	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}

//色温：暖色使红通道高于蓝通道，冷色相反
TEST_F(Renderer_Post_Grade_Test, 色温冷暖偏移方向)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//中灰纹理
	uint64_t texture = 0;
	const uint8_t gray[4] = { 128, 128, 128, 255 };
	ASSERT_TRUE(renderer.texture_load(texture, gray, 1, 1));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle quad = 0;
	quad_register(renderer, quad);

	//暖色（色温 +1、色调 0、强度 1）
	quad_register(renderer, quad);
	std::vector<uint8_t> warm = render_stage(renderer, "色温.frag.spv", 1.0f, 0.0f, 1.0f);
	std::array<uint8_t, 4> warm_pixel = pixel_get(warm, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GT(static_cast<int>(warm_pixel[2]) - static_cast<int>(warm_pixel[0]), 20) << "暖色应红多于蓝";

	//冷色（色温 -1、色调 0、强度 1）
	quad_register(renderer, quad);
	std::vector<uint8_t> cool = render_stage(renderer, "色温.frag.spv", -1.0f, 0.0f, 1.0f);
	std::array<uint8_t, 4> cool_pixel = pixel_get(cool, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GT(static_cast<int>(cool_pixel[0]) - static_cast<int>(cool_pixel[2]), 20) << "冷色应蓝多于红";

	renderer.mesh_unload(quad);
	renderer.texture_unload(texture);
}
