/*
逐绘制项纹理测试（第一百七十四轮，L2 隐藏窗口 + 像素回读）
覆盖：同一帧内不同绘制项各自采样自己的反照率纹理（同帧多纹理）、
      静态合批不跨纹理合并、文字绘制不覆盖场景纹理（文字图集与场景纹理共存）。

口径：
- 左半屏绘制纯红四边形、右半屏绘制纯蓝四边形，两帧均只改纹理绑定，不改几何；
- 若仍沿用「整帧一条采样器集」的旧行为，两侧都会采到最后一次绑定的纹理，颜色相同；
- 合批用例开启静态合批：两项几何 / 管线 / 推送常量全同、实例区首尾相接，
  只差纹理；旧合批条件会把它们并成一项，故同时断言「合并数为 0」与「两侧颜色不同」；
- 文字用例先画满屏棋盘格四边形、再画一小段文字，读回棋盘中心区域应仍有明暗格，
  证明字体图集没有顶掉场景纹理。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（左右各留半屏，便于取样）
	constexpr uint32_t WINDOW_WIDTH = 320;
	constexpr uint32_t WINDOW_HEIGHT = 180;
	//候选中文字体路径（按顺序取第一个存在者）
	const char* const FONT_CANDIDATES[] = {
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
	};

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
		const std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//平移 ＋ 三轴非等比缩放（列主序）
	std::array<float, 16> make_transform(float x, float y, float z, float sx, float sy)
	{
		return {
			sx, 0.0f, 0.0f, 0.0f,
			0.0f, sy, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//XY 平面四边形（法线 +Z、UV 0..1）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//纯色纹理（RGBA8 紧排 2×2）
	uint64_t solid_texture(engine::Renderer& renderer, uint8_t red, uint8_t green, uint8_t blue)
	{
		std::vector<uint8_t> texels(2 * 2 * 4, 255);
		for (std::size_t offset = 0; offset < texels.size(); offset += 4)
		{
			texels[offset] = red;
			texels[offset + 1] = green;
			texels[offset + 2] = blue;
			texels[offset + 3] = 255;
		}
		uint64_t handle = 0;
		renderer.texture_load(handle, texels.data(), 2, 2);
		return handle;
	}

	//固定相机与光照（保证两侧取样只受反照率影响）
	void apply_scene(engine::Renderer& renderer)
	{
		renderer.camera_position_set(0.0f, 0.0f, 6.0f);
		renderer.camera_target_set(0.0f, 0.0f, 0.0f);
		renderer.camera_fov_set(1.0f);
		renderer.camera_near_far_set(0.1f, 100.0f);
		const float direction[3] = { 0.0f, 0.0f, -1.0f };
		const float white[3] = { 1.0f, 1.0f, 1.0f };
		renderer.light_set(direction, white, 0.8f, 0.2f);
	}

	//取当前系统可用的中文字体路径（都不存在返回空串）
	std::string cjk_font_path(void)
	{
		std::error_code code;
		for (const char* candidate : FONT_CANDIDATES)
		{
			if (std::filesystem::exists(std::filesystem::path(candidate), code))
				return candidate;
		}
		return std::string();
	}

	//像素诊断文本
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//像素三通道亮度和（通道序无关，避免依赖表面格式的 RGBA / BGRA 差异）
	uint32_t brightness(const std::array<uint8_t, 4>& value)
	{
		return static_cast<uint32_t>(value[0]) + value[1] + value[2];
	}

	//判定颜色是否为亮色（白 / 浅灰）
	bool is_bright(const std::array<uint8_t, 4>& value)
	{
		return brightness(value) > 450;
	}

	//判定颜色是否为暗色（黑 / 深灰）
	bool is_dark(const std::array<uint8_t, 4>& value)
	{
		return brightness(value) < 180;
	}

	//区域内亮度极值（只统计非清屏色像素；用于判断棋盘格是否仍在）
	void region_brightness_range(const std::vector<uint8_t>& pixels, uint32_t center_x, uint32_t center_y,
		uint32_t radius, uint32_t& minimum_out, uint32_t& maximum_out)
	{
		minimum_out = 0xFFFFFFFFu;
		maximum_out = 0;
		for (uint32_t y = center_y - radius; y <= center_y + radius; ++y)
		{
			for (uint32_t x = center_x - radius; x <= center_x + radius; ++x)
			{
				const std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				//三通道最小 / 最大（本层包含链存在 min / max 宏，故手工计算）
				uint8_t low = value[0];
				uint8_t high = value[0];
				for (uint32_t channel = 1; channel < 3; ++channel)
				{
					if (value[channel] < low)
						low = value[channel];
					if (value[channel] > high)
						high = value[channel];
				}
				//背景候选（清屏色 13/13/26 或 63/63/89）不参与统计
				const bool dark_clear = low >= 7 && low <= 19 && high >= 20 && high <= 32;
				const bool light_clear = low >= 57 && low <= 69 && high >= 83 && high <= 95;
				if (dark_clear || light_clear)
					continue;
				const uint32_t brightness = static_cast<uint32_t>(value[0]) + value[1] + value[2];
				if (brightness < minimum_out)
					minimum_out = brightness;
				if (brightness > maximum_out)
					maximum_out = brightness;
			}
		}
	}
}

//逐绘制项纹理测试夹具
class Renderer_Per_Draw_Texture_Test : public ::testing::Test
{
};

/*
同帧多纹理（第一百七十四轮）
左半屏红四边形、右半屏蓝四边形同帧登记；读回左 / 右取样点应分别偏红 / 偏蓝。
旧行为（整帧共用一条采样器集）下两侧都会采到最后一次绑定的纹理，颜色一致。
*/
TEST_F(Renderer_Per_Draw_Texture_Test, 同帧两个四边形各自贴图)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	apply_scene(renderer);
	//纯白 / 纯黑纹理（只用亮度判定，不依赖通道序）
	const uint64_t white_texture = solid_texture(renderer, 235, 235, 235);
	const uint64_t black_texture = solid_texture(renderer, 20, 20, 20);
	ASSERT_NE(white_texture, 0u);
	ASSERT_NE(black_texture, 0u);
	//同一网格，左右两块（覆盖左右半屏）
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	const std::array<float, 16> left = make_transform(-2.9f, 0.0f, 0.0f, 2.6f, 3.0f);
	const std::array<float, 16> right = make_transform(2.9f, 0.0f, 0.0f, 2.6f, 3.0f);
	//同一帧：两块各用自己的纹理（最后一次绑定为黑）
	renderer.texture_bind_set(white_texture);
	ASSERT_TRUE(renderer.mesh_draw(quad, left.data()));
	renderer.texture_bind_set(black_texture);
	ASSERT_TRUE(renderer.mesh_draw(quad, right.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	const std::array<uint8_t, 4> left_pixel = pixel_get(pixels, WINDOW_WIDTH / 4, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> right_pixel = pixel_get(pixels, WINDOW_WIDTH * 3 / 4, WINDOW_HEIGHT / 2);
	//两侧必须一亮一暗（旧行为下两侧都会采到最后绑定的黑）
	const bool left_bright = is_bright(left_pixel);
	const bool right_bright = is_bright(right_pixel);
	EXPECT_NE(left_bright, right_bright)
		<< "两侧纹理相同（未按绘制项分别采样）：左 " << pixel_text(left_pixel)
		<< "，右 " << pixel_text(right_pixel);
	EXPECT_TRUE(left_bright ? is_dark(right_pixel) : is_dark(left_pixel))
		<< "暗色侧未采到黑纹理：左 " << pixel_text(left_pixel)
		<< "，右 " << pixel_text(right_pixel);

	renderer.mesh_unload(quad);
	renderer.texture_unload(white_texture);
	renderer.texture_unload(black_texture);
}

/*
静态合批不跨纹理合并（第一百七十四轮）
两项几何 / 管线 / 推送常量全同、实例区首尾相接，仅纹理不同；开启静态合批后
既不应发生合并（合并数为 0），两侧也应各自保留自己的纹理。
*/
TEST_F(Renderer_Per_Draw_Texture_Test, 静态合批不跨纹理合并)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	apply_scene(renderer);
	renderer.static_batching_set(true);
	const uint64_t white_texture = solid_texture(renderer, 235, 235, 235);
	const uint64_t black_texture = solid_texture(renderer, 20, 20, 20);
	ASSERT_NE(white_texture, 0u);
	ASSERT_NE(black_texture, 0u);
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	//左右两块紧邻（实例区首尾相接，满足旧合批条件）
	const std::array<float, 16> left = make_transform(-2.9f, 0.0f, 0.0f, 2.6f, 3.0f);
	const std::array<float, 16> right = make_transform(2.9f, 0.0f, 0.0f, 2.6f, 3.0f);
	renderer.texture_bind_set(white_texture);
	ASSERT_TRUE(renderer.mesh_draw(quad, left.data()));
	renderer.texture_bind_set(black_texture);
	ASSERT_TRUE(renderer.mesh_draw(quad, right.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	const std::array<uint8_t, 4> left_pixel = pixel_get(pixels, WINDOW_WIDTH / 4, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> right_pixel = pixel_get(pixels, WINDOW_WIDTH * 3 / 4, WINDOW_HEIGHT / 2);
	const bool left_bright = is_bright(left_pixel);
	const bool right_bright = is_bright(right_pixel);
	EXPECT_NE(left_bright, right_bright) << "合批后两侧纹理被覆盖成同一张：左 "
		<< pixel_text(left_pixel) << "，右 " << pixel_text(right_pixel);
	EXPECT_TRUE(left_bright ? is_dark(right_pixel) : is_dark(left_pixel)) << "暗色侧纹理丢失：左 "
		<< pixel_text(left_pixel) << "，右 " << pixel_text(right_pixel);
	//纹理不同则不可合并
	EXPECT_EQ(renderer.frame_stats_get().last_merged_items, 0u) << "不同纹理的相邻项被错误合批";

	renderer.mesh_unload(quad);
	renderer.texture_unload(white_texture);
	renderer.texture_unload(black_texture);
}

/*
对照：棋盘格四边形本身可见（不加载字体、只画一块纹理四边形）
用于确认「文字用例」的取样口径成立：中心邻域内应同时存在明暗格。
*/
TEST_F(Renderer_Per_Draw_Texture_Test, 棋盘格四边形可见)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	apply_scene(renderer);
	uint64_t checker = 0;
	//格边长 2 像素 → 满屏约 32 格（每格约 9 像素），中心邻域内必含明暗两档
	ASSERT_TRUE(renderer.texture_load_checkerboard(checker, 64, 64, 2));
	//先确认棋盘纹理本身确有两档灰度（排除纹理生成本身只有一档的可能）
	std::vector<uint8_t> texels(64 * 64 * 4, 0);
	uint32_t texel_width = 0;
	uint32_t texel_height = 0;
	ASSERT_TRUE(renderer.texture_readback(checker, texels.data(), static_cast<uint32_t>(texels.size()),
		&texel_width, &texel_height));
	uint32_t dark_texels = 0;
	uint32_t light_texels = 0;
	for (std::size_t offset = 0; offset < texels.size(); offset += 4)
	{
		if (texels[offset] < 100)
			++dark_texels;
		if (texels[offset] > 150)
			++light_texels;
	}
	ASSERT_GT(dark_texels, 0u) << "棋盘纹理缺少暗格";
	ASSERT_GT(light_texels, 0u) << "棋盘纹理缺少亮格";
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	const std::array<float, 16> full = make_transform(0.0f, 0.0f, 0.0f, 5.2f, 3.0f);
	renderer.texture_bind_set(checker);
	ASSERT_TRUE(renderer.mesh_draw(quad, full.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	uint32_t minimum = 0;
	uint32_t maximum = 0;
	region_brightness_range(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 20, minimum, maximum);
	//棋盘两档在 sRGB 输出下约为「暗 64 / 亮 192」每通道（三通道和为 192 / 576）
	EXPECT_LT(minimum, 250u) << "棋盘中心缺少暗格";
	EXPECT_GT(maximum, 500u) << "棋盘中心缺少亮格";

	renderer.mesh_unload(quad);
	renderer.texture_unload(checker);
}

/*
文字不覆盖场景纹理（第一百七十四轮）
先绘制满屏棋盘格四边形，再在同一帧登记一段文字；读回棋盘中心区域应仍存在明暗两档，
说明文字图集只作用于文字绘制项，没有把整帧的反照率换成图集。
*/
TEST_F(Renderer_Per_Draw_Texture_Test, 文字绘制不覆盖场景纹理)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//字体缺失时跳过（与中文字形用例同一口径）
	const std::string font_path = cjk_font_path();
	if (font_path.empty())
		GTEST_SKIP() << "本机缺少候选中文字体";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, font_path, 24));
	apply_scene(renderer);
	//满屏棋盘格四边形
	uint64_t checker = 0;
	//格边长 2 像素 → 满屏约 32 格（每格约 9 像素），中心邻域内必含明暗两档
	ASSERT_TRUE(renderer.texture_load_checkerboard(checker, 64, 64, 2));
	engine::Mesh_Handle quad = 0;
	ASSERT_TRUE(renderer.mesh_load(quad, quad_data()));
	const std::array<float, 16> full = make_transform(0.0f, 0.0f, 0.0f, 5.2f, 3.0f);
	//对照帧：只画棋盘四边形
	renderer.texture_bind_set(checker);
	ASSERT_TRUE(renderer.mesh_draw(quad, full.data()));
	std::vector<uint8_t> control_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(control_pixels.data(), static_cast<std::uint32_t>(control_pixels.size())));
	uint32_t control_minimum = 0;
	uint32_t control_maximum = 0;
	region_brightness_range(control_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 20,
		control_minimum, control_maximum);
	ASSERT_LT(control_minimum, 250u) << "对照帧棋盘中心缺少暗格（用例前提不成立）";
	ASSERT_GT(control_maximum, 500u) << "对照帧棋盘中心缺少亮格（用例前提不成立）";

	//第二帧：棋盘四边形 ＋ 同帧文字（文字会把字体图集绑定为当前纹理）
	renderer.texture_bind_set(checker);
	ASSERT_TRUE(renderer.mesh_draw(quad, full.data()));
	ASSERT_TRUE(renderer.text_draw(font, "ABC", 8.0f, 8.0f));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));

	//棋盘中心区域应仍有明暗两档（若被图集顶掉则趋于均匀）
	uint32_t minimum = 0;
	uint32_t maximum = 0;
	region_brightness_range(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 20, minimum, maximum);
	EXPECT_LT(minimum, 250u) << "加入文字后棋盘中心失去暗格";
	EXPECT_GT(maximum, 500u) << "加入文字后棋盘中心失去亮格";

	renderer.mesh_unload(quad);
	renderer.texture_unload(checker);
	renderer.font_unload(font);
}
