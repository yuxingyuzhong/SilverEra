/*
渲染文字测试（L2 隐藏窗口 + 像素回读）
覆盖：字体加载入口（未初始化 / 文件缺失）、文字绘制在指定位置产生非背景像素、
      不同文本产生不同的像素分布。
做法：字体取自系统 TTF（`C:\Windows\Fonts\consola.ttf`，本机不存在时用例跳过）；
      字形经 stb_truetype 光栅化并打包为图集，文字以「像素坐标 → NDC」矩阵经文字管线绘制。
断言一律「格式无关」：以「是否近似清屏色」判定墨迹（双编码候选 ＋ 容差 ＋ 通道排序；alpha 恒取第 4 字节）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面
#include "Application/Renderer/src/render/Renderer/渲染器.h"
//文件是否存在（决定是否跳过依赖系统字体的用例）
#include <filesystem>

namespace
{
	//窗口尺寸（比既有用例宽，给文本留出空间）
	constexpr uint32_t WINDOW_WIDTH = 128;
	constexpr uint32_t WINDOW_HEIGHT = 64;
	//字号（字形像素高度）
	constexpr uint32_t FONT_PIXEL_HEIGHT = 16;
	//系统字体路径（本机存在才跑文字绘制类用例）
	const char* const FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

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
				//本通道与候选值的差
				int delta = static_cast<int>(sorted[index]) - static_cast<int>(candidate[index]);
				//若超出容差
				if (delta < -CLEAR_TOLERANCE || delta > CLEAR_TOLERANCE)
					matched = false;
			}
			//命中则返回真
			if (matched)
				return true;
		}
		return false;
	}

	//统计指定矩形区域内的「非清屏色像素」数（即墨迹像素）
	uint32_t ink_count(const std::vector<uint8_t>& pixels, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
	{
		//累计墨迹像素
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

//渲染文字测试夹具
class Renderer_Text_Test : public ::testing::Test
{
};

//未初始化时加载字体失败
TEST_F(Renderer_Text_Test, 未初始化加载字体失败)
{
	engine::Renderer renderer;
	//未 init 直接加载
	uint64_t font = 12345;
	EXPECT_FALSE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	//句柄须被复位为 0
	EXPECT_EQ(font, 0u);
}

//字体文件不存在时加载失败
TEST_F(Renderer_Text_Test, 字体文件缺失加载失败)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t font = 0;
	EXPECT_FALSE(renderer.font_load_file(font, "C:\\不存在的字体文件.ttf", FONT_PIXEL_HEIGHT));
	EXPECT_EQ(font, 0u);
	//帧仍正常推进（字体加载失败不破坏帧循环）
	std::vector<uint8_t> pixels(pixel_bytes());
	EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
}

//文本在指定位置产生非背景像素，且远离文本处仍为清屏色
TEST_F(Renderer_Text_Test, 文本在指定位置产生非背景像素)
{
	//若本机没有测试用系统字体则跳过
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	EXPECT_GE(font, 1u);

	//在左上附近画两个大写字母
	ASSERT_TRUE(renderer.text_draw(font, "AB", 4.0f, 4.0f));
	//渲染并回读
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t read_width = 0;
	uint32_t read_height = 0;
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()),
		&read_width, &read_height));
	ASSERT_EQ(read_width, WINDOW_WIDTH);
	ASSERT_EQ(read_height, WINDOW_HEIGHT);

	//全文墨迹（用于「确实画出了东西」的粗判）
	uint32_t whole_ink = ink_count(pixels, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	EXPECT_GT(whole_ink, 0u) << "全窗口未出现任何文字墨迹";
	//文本被要求画在左上角附近，故上半区应有墨迹
	uint32_t upper_ink = ink_count(pixels, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT / 2);
	EXPECT_GT(upper_ink, 0u) << "上半区未出现文字墨迹（文本位置不符）";
	//远离文本的区域（右下角一带）应仍为清屏色
	uint32_t far_ink = ink_count(pixels, WINDOW_WIDTH * 3 / 4, WINDOW_HEIGHT / 2, WINDOW_WIDTH, WINDOW_HEIGHT);
	EXPECT_EQ(far_ink, 0u) << "远离文本处出现非背景像素";

	//卸载字体与重绘：卸载后再画应失败（句柄失效）
	renderer.font_unload(font);
	EXPECT_FALSE(renderer.text_draw(font, "AB", 4.0f, 4.0f));
}

//不同文本产生不同的像素分布（证明确实按文本光栅化，而非固定图样）
TEST_F(Renderer_Text_Test, 不同文本产生不同像素分布)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载字体
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//第一帧：三个窄字形
	ASSERT_TRUE(renderer.text_draw(font, "III", 4.0f, 4.0f));
	std::vector<uint8_t> narrow(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(narrow.data(), static_cast<uint32_t>(narrow.size())));
	//窄字形的墨迹量
	uint32_t narrow_ink = ink_count(narrow, 0, 0, 64, 24);

	//第二帧：三个宽字形
	ASSERT_TRUE(renderer.text_draw(font, "WWW", 4.0f, 4.0f));
	std::vector<uint8_t> wide(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(wide.data(), static_cast<uint32_t>(wide.size())));
	//宽字形的墨迹量
	uint32_t wide_ink = ink_count(wide, 0, 0, 64, 24);

	//两帧都应有墨迹，且宽字形明显多于窄字形
	EXPECT_GT(narrow_ink, 0u) << "窄字形帧无墨迹";
	EXPECT_GT(wide_ink, narrow_ink)
		<< "宽窄字形墨迹量未区分（窄 " << narrow_ink << " / 宽 " << wide_ink << "）";

	renderer.font_unload(font);
}

//空文本返回真但不登记绘制项；无效字体句柄返回假
TEST_F(Renderer_Text_Test, 空文本与无效句柄行为)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//无效字体句柄 → 假
	EXPECT_FALSE(renderer.text_draw(99999u, "AB", 0.0f, 0.0f));
	//加载字体后空文本 → 真（无可绘制项）
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	EXPECT_TRUE(renderer.text_draw(font, "", 0.0f, 0.0f));
	//帧仍正常推进
	std::vector<uint8_t> pixels(pixel_bytes());
	EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	renderer.font_unload(font);
}

/*
纯 ASCII 绘制不产生「跳过码点」计数（处置 R-TEXT-01 的基线口径）
未初始化 / 无效句柄一律返回 0，加载后计数从 0 起。
*/
TEST_F(Renderer_Text_Test, 纯ASCII不产生跳过计数)
{
	engine::Renderer renderer;
	//未初始化时任何句柄都返回 0
	EXPECT_EQ(renderer.font_skipped_codepoints(1u), 0u);
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	//加载后计数为 0（ASCII 已全部烘焙）
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 0u) << "加载阶段不应产生跳过计数";
	//画纯 ASCII 字符串（含空白字符，空白不应被计为跳过）
	ASSERT_TRUE(renderer.text_draw(font, "AB C1", 4.0f, 4.0f));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 0u) << "纯 ASCII 与空白字符不应计入跳过数";
	//重复绘制同样不增
	ASSERT_TRUE(renderer.text_draw(font, "AB C1", 4.0f, 4.0f));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 0u);
	//无效句柄仍为 0
	EXPECT_EQ(renderer.font_skipped_codepoints(99999u), 0u);
	renderer.font_unload(font);
}

/*
字体不含的码点计入「跳过码点」计数（每码点至多计一次）
文本混合 ASCII 与一个私有区码点（U+E000，consola 等常规字体不含），
故计数 +1；同一码点重复绘制不再增加（补烘只做一次）。
*/
TEST_F(Renderer_Text_Test, 不支持码点计入跳过计数)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	ASSERT_EQ(renderer.font_skipped_codepoints(font), 0u);

	//「A ＋ U+E000 ＋ B」：私有区码点不在字体覆盖范围
	std::string unsupported = "A";
	unsupported += "\xEE\x80\x80";
	unsupported += "B";
	//ASCII 部分仍产生墨迹，故绘制整体返回真
	ASSERT_TRUE(renderer.text_draw(font, unsupported, 4.0f, 4.0f));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 1u)
		<< "字体不含的码点应计入跳过数（实际 "
		<< renderer.font_skipped_codepoints(font) << "）";
	//重复绘制同一文本：该码点已登记（含 0 推进的空度量），不再重复计数
	ASSERT_TRUE(renderer.text_draw(font, unsupported, 4.0f, 4.0f));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 1u) << "同一码点不应重复计数";
	//另一串纯 ASCII 不改变计数
	ASSERT_TRUE(renderer.text_draw(font, "AB", 4.0f, 4.0f));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 1u);
	//卸载后句柄失效，读取归 0
	renderer.font_unload(font);
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 0u);
}

/*
文字颜色：默认白色为三通道中性；设置红色后墨迹明显偏色（通道序无关）
口径：取全窗口「最亮」像素（三分量之和最大）作墨迹样本；
      默认白色帧该样本三通道差 ≤ 6；红色帧该样本三通道差 ≥ 100（着色确实生效）。
*/
TEST_F(Renderer_Text_Test, 文字颜色着色生效)
{
        if (!font_available())
                GTEST_SKIP() << "本机无 " << FONT_PATH;
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config()));
        if (!renderer.readback_supported())
                GTEST_SKIP() << "本机交换链图像不具备传输源用法";
        uint64_t font = 0;
        ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
        //第一帧：默认白色
        ASSERT_TRUE(renderer.text_draw(font, "HH", 8.0f, 8.0f));
        std::vector<uint8_t> white_pixels(pixel_bytes());
        ASSERT_TRUE(renderer.frame_render(white_pixels.data(), static_cast<uint32_t>(white_pixels.size())));
        //第二帧：红色（重新登记绘制项）
        renderer.text_color_set(std::array<float, 4>{ 1.0f, 0.0f, 0.0f, 1.0f }.data());
        ASSERT_TRUE(renderer.text_draw(font, "HH", 8.0f, 8.0f));
        std::vector<uint8_t> red_pixels(pixel_bytes());
        ASSERT_TRUE(renderer.frame_render(red_pixels.data(), static_cast<uint32_t>(red_pixels.size())));

        //取两帧最亮像素
        std::array<uint8_t, 4> white_pixel{};
        std::array<uint8_t, 4> red_pixel{};
        int white_sum = -1;
        int red_sum = -1;
        for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
                for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
                {
                        std::array<uint8_t, 4> white_value = pixel_get(white_pixels, x, y);
                        std::array<uint8_t, 4> red_value = pixel_get(red_pixels, x, y);
                        int current_white = white_value[0] + white_value[1] + white_value[2];
                        int current_red = red_value[0] + red_value[1] + red_value[2];
                        if (current_white > white_sum)
                        {
                                white_sum = current_white;
                                white_pixel = white_value;
                        }
                        if (current_red > red_sum)
                        {
                                red_sum = current_red;
                                red_pixel = red_value;
                        }
                }
        //白色帧：三通道中性
        int white_min = *std::min_element(white_pixel.begin(), white_pixel.begin() + 3);
        int white_max = *std::max_element(white_pixel.begin(), white_pixel.begin() + 3);
        EXPECT_LE(white_max - white_min, 6) << "默认白色被着色：" << pixel_text(white_pixel);
        //红色帧：明显偏色（通道序无关，只看最大与最小通道之差）
        int red_min = *std::min_element(red_pixel.begin(), red_pixel.begin() + 3);
        int red_max = *std::max_element(red_pixel.begin(), red_pixel.begin() + 3);
        RecordProperty("text_color_red_max", red_max);
        RecordProperty("text_color_red_min", red_min);
	EXPECT_GE(red_max - red_min, 100) << "文字着色未生效（仍为中性色）：" << pixel_text(red_pixel);
	EXPECT_GE(red_max, 200) << "着色后墨迹过暗：" << pixel_text(red_pixel);

	renderer.font_unload(font);
}

/*
文字 alpha（第二十轮）：颜色的第 4 分量参与混合
口径：白字（alpha = 1）与半透明白字（alpha = 0.5）各画一帧；
      半透明帧的最亮像素应比不透明帧暗、但明显亮于清屏色（介于两者之间）。
*/
TEST_F(Renderer_Text_Test, 文字透明度生效)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));
	//不透明白字
	renderer.text_color_set(std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f }.data());
	ASSERT_TRUE(renderer.text_draw(font, "HH", 8.0f, 8.0f));
	std::vector<uint8_t> opaque_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(opaque_pixels.data(), static_cast<uint32_t>(opaque_pixels.size())));
	//半透明白字
	renderer.text_color_set(std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 0.5f }.data());
	ASSERT_TRUE(renderer.text_draw(font, "HH", 8.0f, 8.0f));
	std::vector<uint8_t> alpha_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(alpha_pixels.data(), static_cast<uint32_t>(alpha_pixels.size())));

	//两帧各自的最亮像素
	auto brightest = [](const std::vector<uint8_t>& pixels)
	{
		std::array<uint8_t, 4> best{};
		int best_sum = -1;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				int sum = value[0] + value[1] + value[2];
				if (sum > best_sum)
				{
					best_sum = sum;
					best = value;
				}
			}
		return best;
	};
	std::array<uint8_t, 4> opaque = brightest(opaque_pixels);
	std::array<uint8_t, 4> alpha = brightest(alpha_pixels);
	std::array<uint8_t, 4> background = pixel_get(opaque_pixels, 1, 1);
	int opaque_luma = opaque[0] + opaque[1] + opaque[2];
	int alpha_luma = alpha[0] + alpha[1] + alpha[2];
	int background_luma = background[0] + background[1] + background[2];
	RecordProperty("alpha_opaque_luma", opaque_luma);
	RecordProperty("alpha_half_luma", alpha_luma);
	EXPECT_LT(alpha_luma, opaque_luma) << "半透明文字未变暗：" << pixel_text(alpha) << " / " << pixel_text(opaque);
	EXPECT_GT(alpha_luma, background_luma + 30) << "半透明文字过暗（已接近背景）：" << pixel_text(alpha);

        renderer.font_unload(font);
}

//文本网格缓存上限（第四十八轮）：连画 70 段不同文本触发 LRU 淘汰，淘汰后重画最早文本仍应成功出墨
TEST_F(Renderer_Text_Test, 文本网格缓存淘汰后仍可重画)
{
	if (!font_available())
		GTEST_SKIP() << "本机无 " << FONT_PATH;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, FONT_PATH, FONT_PIXEL_HEIGHT));

	//连续登记 70 段不同文本（缓存上限 64，必有 6 条被淘汰）
	for (uint32_t index = 0; index < 70; ++index)
	{
		const std::string text = "T" + std::to_string(index);
		ASSERT_TRUE(renderer.text_draw(font, text, 4.0f, 4.0f)) << "第 " << index << " 段文本登记失败";
	}
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_GT(ink_count(pixels, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), 0u) << "大量文本下未出现墨迹";

	//重画最早的一段（其网格已被淘汰，应经重建再次成功）
	ASSERT_TRUE(renderer.text_draw(font, "T0", 4.0f, 4.0f)) << "淘汰后的文本无法重画";
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_GT(ink_count(pixels, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), 0u) << "淘汰重建后未出现墨迹";

	renderer.font_unload(font);
}
