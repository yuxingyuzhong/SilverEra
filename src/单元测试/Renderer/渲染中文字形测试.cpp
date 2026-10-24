/*
渲染中文字形测试（L2 隐藏窗口 + 像素回读）
覆盖：中文字体加载、非 ASCII 码点（中文）在首次绘制时**惰性补烘**进字形图集并产生非背景像素、
      同一文本重复绘制复用缓存、以及两串不同中文各自产生墨迹。
做法：字体取自系统中文字体（SimHei / Microsoft YaHei，本机不存在时用例跳过）；
      加载时只烘焙 ASCII，中文码点在 text_draw 时补烘并重建图集纹理。
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

//获取渲染器门面
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（给中文两字留出空间）
	constexpr uint32_t WINDOW_WIDTH = 128;
	constexpr uint32_t WINDOW_HEIGHT = 64;
	//字号（字形像素高度）
	constexpr uint32_t FONT_PIXEL_HEIGHT = 24;

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

	//取当前系统可用的中文字体路径（都不存在返回空串）
	std::string cjk_font_path(void)
	{
		std::error_code code;
		for (const char* candidate : FONT_CANDIDATES)
		{
			//命中第一个存在者
			if (std::filesystem::exists(std::filesystem::path(candidate), code))
				return candidate;
		}
		return std::string();
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

	//清屏色候选（渲染器清屏色 0.05 / 0.05 / 0.10：UNORM ≈ 13/13/26，sRGB ≈ 63/63/89）
	constexpr std::array<std::array<uint8_t, 3>, 2> CLEAR_CANDIDATES = { {
		{ { 13, 13, 26 } },
		{ { 63, 63, 89 } },
	} };
	//清屏色通道容差
	constexpr int CLEAR_TOLERANCE = 6;

	//判断像素是否近似等于清屏色（排序后与两种编码候选比对）
	bool pixel_is_clear(std::array<uint8_t, 4> value)
	{
		//排序后的四通道
		std::array<uint8_t, 4> sorted = value;
		std::sort(sorted.begin(), sorted.end());
		//末位须为不透明（文字墨迹的 alpha 会被混合压低，故不满足此条）
		if (sorted[3] < 250)
			return false;
		//逐候选比对前三个通道
		for (const std::array<uint8_t, 3>& candidate : CLEAR_CANDIDATES)
		{
			//三个通道均落在容差内即命中
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

	//统计指定矩形区域内的「非清屏色像素」数（即墨迹像素）
	uint32_t ink_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
	{
		uint32_t count = 0;
		for (uint32_t y = y0; y < y1 && y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = x0; x < x1 && x < WINDOW_WIDTH; ++x)
			{
				//非清屏色即计为墨迹
				if (!pixel_is_clear(pixel_get(pixels, x, y)))
					++count;
			}
		}
		return count;
	}
}

//中文字形测试夹具
class Renderer_Cjk_Text_Test : public ::testing::Test
{
};

//中文文本在指定位置产生非背景像素（证非 ASCII 码点已被惰性补烘）
TEST_F(Renderer_Cjk_Text_Test, 中文产生非背景像素)
{
	//若本机没有中文字体则跳过
	std::string font_path = cjk_font_path();
	if (font_path.empty())
		GTEST_SKIP() << "本机无测试用中文字体";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载中文字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, font_path, FONT_PIXEL_HEIGHT));
	EXPECT_GE(font, 1u);

	//在左上附近画两个中文
	ASSERT_TRUE(renderer.text_draw(font, "中文", 8.0f, 8.0f));
	//渲染并回读
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t read_width = 0;
	uint32_t read_height = 0;
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()),
		&read_width, &read_height));
	ASSERT_EQ(read_width, WINDOW_WIDTH);
	ASSERT_EQ(read_height, WINDOW_HEIGHT);

	//全文墨迹（确认真画出了东西）
	uint32_t whole_ink = ink_count(pixels, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	EXPECT_GT(whole_ink, 0u) << "全窗口未出现任何中文字形墨迹";
	//文本被要求画在左上角附近，故左上区应有墨迹
	uint32_t left_ink = ink_count(pixels, 0, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GT(left_ink, 0u) << "左上区未出现中文墨迹（文本位置不符）";
	//远离文本的右下角一带应仍为清屏色
	uint32_t far_ink = ink_count(pixels, WINDOW_WIDTH * 3 / 4, WINDOW_HEIGHT / 2, WINDOW_WIDTH, WINDOW_HEIGHT);
	EXPECT_EQ(far_ink, 0u) << "远离文本处出现非背景像素";

	//同一文本重复绘制复用缓存网格，仍应出帧并保持墨迹
	ASSERT_TRUE(renderer.text_draw(font, "中文", 8.0f, 8.0f));
	std::vector<uint8_t> again(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(again.data(), static_cast<uint32_t>(again.size())));
	EXPECT_GT(ink_count(again, 0, 0, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), 0u) << "重复绘制后中文墨迹消失";

	renderer.font_unload(font);
}

//两串不同中文各自产生墨迹（证补烘按码点进行，而非固定图样）
TEST_F(Renderer_Cjk_Text_Test, 不同中文各自产生墨迹)
{
	std::string font_path = cjk_font_path();
	if (font_path.empty())
		GTEST_SKIP() << "本机无测试用中文字体";
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, font_path, FONT_PIXEL_HEIGHT));

	//第一串：默认尺寸、无额外变换，先画一组
	ASSERT_TRUE(renderer.text_draw(font, "日月", 8.0f, 8.0f));
	std::vector<uint8_t> first(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(first.data(), static_cast<uint32_t>(first.size())));
	uint32_t first_ink = ink_count(first, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	//第二串：另一组不同汉字（触发新的惰性补烘）
	ASSERT_TRUE(renderer.text_draw(font, "山水", 8.0f, 8.0f));
	std::vector<uint8_t> second(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(second.data(), static_cast<uint32_t>(second.size())));
	uint32_t second_ink = ink_count(second, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	//两帧都应有墨迹
	EXPECT_GT(first_ink, 0u) << "第一串中文无墨迹";
	EXPECT_GT(second_ink, 0u) << "第二串中文无墨迹";

	renderer.font_unload(font);
}
