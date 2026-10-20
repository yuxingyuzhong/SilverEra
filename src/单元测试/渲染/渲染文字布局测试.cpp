/*
渲染文字布局测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化守卫、单行度量单调性、空 / 全空白度量为零、
      显式换行与按 max_width 自动换行（多行墨迹分布）、行内右对齐（墨迹右移）、
      不同布局不命中同一网格缓存。
做法：字体取自系统 TTF（`C:\Windows\Fonts\consola.ttf`，本机不存在时用例跳过）；
      文字像素坐标口径与既有文字用例一致（原点在窗口左上、Y 向下）。
断言一律「格式无关」：以「是否近似清屏色」判定墨迹（双编码候选 ＋ 容差 ＋ 通道排序；alpha 恒取第 4 字节）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

//获取渲染器门面（含文字布局语义类型）
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（高度留出多行空间）
	constexpr uint32_t WINDOW_WIDTH = 128;
	constexpr uint32_t WINDOW_HEIGHT = 96;
	//字号（字形像素高度）
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	//系统字体路径（本机存在才跑绘制类用例）
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

	//清屏色候选（0.05 / 0.05 / 0.10：UNORM ≈ 13/13/26，sRGB ≈ 63/63/89）
	constexpr std::array<std::array<uint8_t, 3>, 2> CLEAR_CANDIDATES = { {
		{ { 13, 13, 26 } },
		{ { 63, 63, 89 } },
	} };
	//清屏色通道容差
	constexpr int CLEAR_TOLERANCE = 6;

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

	//系统字体是否可用
	bool font_available(void)
	{
		std::error_code code;
		return std::filesystem::exists(std::filesystem::path(FONT_PATH), code);
	}

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//判断像素是否近似等于清屏色（排序后与两种编码候选比对）
	bool pixel_is_clear(std::array<uint8_t, 4> value)
	{
		//排序后的四通道
		std::array<uint8_t, 4> sorted = value;
		std::sort(sorted.begin(), sorted.end());
		//末位须为不透明（文字墨迹的 alpha 会被混合压低）
		if (sorted[3] < 250)
			return false;
		//逐候选比对前三个通道
		for (const std::array<uint8_t, 3>& candidate : CLEAR_CANDIDATES)
		{
			bool matched = true;
			for (std::size_t index = 0; index < 3; ++index)
			{
				int delta = static_cast<int>(sorted[index]) - static_cast<int>(candidate[index]);
				if (delta < -CLEAR_TOLERANCE || delta > CLEAR_TOLERANCE)
					matched = false;
			}
			if (matched)
				return true;
		}
		return false;
	}

	//统计指定纵向区间内的墨迹像素数
	uint32_t ink_in_band(const std::vector<uint8_t>& pixels, uint32_t y0, uint32_t y1)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1 && y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				if (!pixel_is_clear(pixel_get(pixels, x, y)))
					++count;
		return count;
	}

	//全文墨迹像素数
	uint32_t ink_total(const std::vector<uint8_t>& pixels)
	{
		return ink_in_band(pixels, 0, WINDOW_HEIGHT);
	}

	//墨迹的水平范围（min_x / max_x；无墨迹时均为 0）
	void ink_horizontal_extent(const std::vector<uint8_t>& pixels, uint32_t& min_x_out, uint32_t& max_x_out)
	{
		min_x_out = 0;
		max_x_out = 0;
		//是否已遇到第一个墨迹像素
		bool found = false;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				if (pixel_is_clear(pixel_get(pixels, x, y)))
					continue;
				if (!found)
				{
					min_x_out = x;
					found = true;
				}
				max_x_out = x;
			}
		}
	}
}

//文字布局测试夹具
class Renderer_Text_Layout_Test : public ::testing::Test
{
};

//未初始化时度量失败且出参归零；布局绘制同样失败
TEST_F(Renderer_Text_Layout_Test, 未初始化守卫)
{
	engine::Renderer renderer;
	float width = 123.0f;
	float height = 123.0f;
	EXPECT_FALSE(renderer.text_measure(1u, "AB", &width, &height));
	EXPECT_EQ(width, 0.0f);
	EXPECT_EQ(height, 0.0f);
	//出参可为空（不崩溃）
	EXPECT_FALSE(renderer.text_measure(1u, "AB", nullptr, nullptr));
	engine::Text_Layout layout{};
	EXPECT_FALSE(renderer.text_draw_layout(1u, "AB", 0.0f, 0.0f, layout));
}

//度量：同一字体下宽度随字符数单调递增，高度恒为字体像素高度
TEST_F(Renderer_Text_Layout_Test, 度量宽度随字符数单调)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//三个长度递增的文本
	float width_one = 0.0f;
	float width_two = 0.0f;
	float width_three = 0.0f;
	float measured_height = 0.0f;
	ASSERT_TRUE(renderer.text_measure(font, "I", &width_one, &measured_height));
	ASSERT_TRUE(renderer.text_measure(font, "II", &width_two, nullptr));
	ASSERT_TRUE(renderer.text_measure(font, "III", &width_three, nullptr));
	EXPECT_GT(width_one, 0.0f);
	EXPECT_GT(width_two, width_one);
	EXPECT_GT(width_three, width_two);
	//高度恒为字号
	EXPECT_EQ(measured_height, static_cast<float>(FONT_PIXEL_HEIGHT));

	//尾随空白不计入宽度
	float with_space = 0.0f;
	ASSERT_TRUE(renderer.text_measure(font, "I   ", &with_space, nullptr));
	EXPECT_EQ(with_space, width_one) << "尾随空白不应计入宽度";

	//无效句柄返回假
	EXPECT_FALSE(renderer.text_measure(99999u, "I", &width_one, nullptr));
	renderer.font_unload(font);
}

//空文本与全空白文本：无可见字形 → 度量返回假且出参归零
TEST_F(Renderer_Text_Layout_Test, 空文本度量为零)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	float width = 99.0f;
	float height = 99.0f;
	EXPECT_FALSE(renderer.text_measure(font, "", &width, &height));
	EXPECT_EQ(width, 0.0f);
	EXPECT_EQ(height, 0.0f);
	//全空白同样无可见字形
	EXPECT_FALSE(renderer.text_measure(font, "   ", &width, &height));
	EXPECT_EQ(width, 0.0f);
	EXPECT_EQ(height, 0.0f);
	renderer.font_unload(font);
}

//显式换行：两行墨迹分别落在第一 / 第二个行高区间
TEST_F(Renderer_Text_Layout_Test, 显式换行产生两行墨迹)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//「I\nI」：显式换行，行高默认取字号
	engine::Text_Layout layout{};
	ASSERT_TRUE(renderer.text_draw_layout(font, "I\nI", 4.0f, 0.0f, layout));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//第一行区间与第二行区间都应有墨迹
	uint32_t first_band = ink_in_band(pixels, 0, FONT_PIXEL_HEIGHT);
	uint32_t second_band = ink_in_band(pixels, FONT_PIXEL_HEIGHT, 2 * FONT_PIXEL_HEIGHT);
	EXPECT_GT(first_band, 0u) << "第一行无墨迹";
	EXPECT_GT(second_band, 0u) << "第二行无墨迹（显式换行未生效）";
	//第三行区间应无墨迹（仅两行）
	uint32_t third_band = ink_in_band(pixels, 2 * FONT_PIXEL_HEIGHT, 3 * FONT_PIXEL_HEIGHT);
	EXPECT_EQ(third_band, 0u) << "出现多余的第三行墨迹";

	renderer.font_unload(font);
}

//按 max_width 自动换行：每行仅容纳一个字形时，三个字形分处三个行高区间
TEST_F(Renderer_Text_Layout_Test, 窄宽度逐字换行)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//以「单字形宽度」为换行上限 → 每行恰好一个字形
	float glyph_width = 0.0f;
	ASSERT_TRUE(renderer.text_measure(font, "I", &glyph_width, nullptr));
	ASSERT_GT(glyph_width, 0.0f);
	engine::Text_Layout layout{};
	layout.max_width = glyph_width;
	ASSERT_TRUE(renderer.text_draw_layout(font, "III", 4.0f, 0.0f, layout));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//三行各自有墨迹
	EXPECT_GT(ink_in_band(pixels, 0, FONT_PIXEL_HEIGHT), 0u) << "第一行无墨迹";
	EXPECT_GT(ink_in_band(pixels, FONT_PIXEL_HEIGHT, 2 * FONT_PIXEL_HEIGHT), 0u) << "第二行无墨迹";
	EXPECT_GT(ink_in_band(pixels, 2 * FONT_PIXEL_HEIGHT, 3 * FONT_PIXEL_HEIGHT), 0u) << "第三行无墨迹";
	//第四行应无墨迹
	EXPECT_EQ(ink_in_band(pixels, 3 * FONT_PIXEL_HEIGHT, 4 * FONT_PIXEL_HEIGHT), 0u) << "多出一行";
	renderer.font_unload(font);
}

//行内右对齐：同一文本在对齐基准内被平移到右侧
TEST_F(Renderer_Text_Layout_Test, 右对齐使墨迹靠右)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//对齐基准宽度（远大于单字宽度）
	constexpr float ALIGN_WIDTH = 100.0f;
	engine::Text_Layout right_layout{};
	right_layout.max_width = ALIGN_WIDTH;
	right_layout.align = engine::Text_Align::Right;
	ASSERT_TRUE(renderer.text_draw_layout(font, "I", 4.0f, 0.0f, right_layout));
	std::vector<uint8_t> right_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(right_pixels.data(), static_cast<uint32_t>(right_pixels.size())));
	uint32_t right_min = 0;
	uint32_t right_max = 0;
	ink_horizontal_extent(right_pixels, right_min, right_max);
	EXPECT_GT(right_max, right_min);
	EXPECT_GE(right_min, 60u) << "右对齐未生效（墨迹起点 " << right_min << "）";

	//同一文本改为左对齐：墨迹应回到左侧，且不命中上一次的网格缓存
	engine::Text_Layout left_layout{};
	left_layout.max_width = ALIGN_WIDTH;
	left_layout.align = engine::Text_Align::Left;
	ASSERT_TRUE(renderer.text_draw_layout(font, "I", 4.0f, 0.0f, left_layout));
	std::vector<uint8_t> left_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(left_pixels.data(), static_cast<uint32_t>(left_pixels.size())));
	uint32_t left_min = 0;
	uint32_t left_max = 0;
	ink_horizontal_extent(left_pixels, left_min, left_max);
	EXPECT_GT(left_max, left_min);
	EXPECT_LE(left_min, 20u) << "左对齐墨迹未回到左侧（可能命中旧网格缓存）";
	EXPECT_GT(ink_total(left_pixels), 0u);
	renderer.font_unload(font);
}
