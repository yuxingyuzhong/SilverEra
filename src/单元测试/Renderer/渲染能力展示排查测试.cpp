/*
能力展示显示排查（第四百零二轮：展示重构后的可见性回归）
用途：以「多距离 ＋ 多角度」扫过整条展廊，量化每格画面与「纯天空」的差异，定位「场景无显示」。
口径：
1. 参考帧 = 同一相机、不调用 frame_draw（只有清屏 ＋ 天空）；
2. 展示帧 = 调用 frame_draw 后回读；统计与参考帧逐通道差 ≥ 6 的像素数；
3. 每格取 12 / 20 / 30 三个距离与 −20° / 0° / +20° 三个角度，另加一个「高空俯视」角度；
4. 断言每格都存在可见覆盖（差像素数 > 200；地面 ＋ 地台是底色，正常情况应远大于此）。
文字口径：23 条展区标题一次性绘制后 font_skipped_codepoints == 0。
*/
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Application/Renderer/src/render/Renderer/渲染器.h"
#include "src/主调/能力展示/能力展示场景.h"

namespace
{
	//排查窗口尺寸（小尺寸让数十帧仍可在秒级跑完）
	constexpr uint32_t PROBE_WIDTH = 128;
	constexpr uint32_t PROBE_HEIGHT = 72;
	//「与参考帧不同」的逐通道阈值
	constexpr int VISIBLE_THRESHOLD = 6;
	//每格可见覆盖的判定下限（差像素数）
	constexpr uint32_t VISIBLE_MIN_PIXELS = 200;

	engine::Renderer_Config probe_config(void)
	{
		engine::Renderer_Config config;
		config.width = PROBE_WIDTH;
		config.height = PROBE_HEIGHT;
		config.hidden = true;
		return config;
	}

	//本帧像素字节数
	uint32_t probe_bytes(void)
	{
		return PROBE_WIDTH * PROBE_HEIGHT * 4;
	}

	//整帧平均亮度（Rec.709 加权）
	float average_luma(const std::vector<uint8_t>& pixels)
	{
		double sum = 0.0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
			sum += 0.2126 * pixels[index] + 0.7152 * pixels[index + 1] + 0.0722 * pixels[index + 2];
		return static_cast<float>(sum / static_cast<double>(PROBE_WIDTH * PROBE_HEIGHT));
	}

	//两帧之间逐通道差 ≥ 阈值的像素数
	uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				if (delta < 0)
					delta = -delta;
				if (maximum < delta)
					maximum = delta;
			}
			if (maximum >= VISIBLE_THRESHOLD)
				++count;
		}
		return count;
	}

	//把相机放到展区周围：yaw 让视线偏移，dist 为观察距离；俯视角度用于第 4 种角度
	void aim_camera(engine::Renderer& renderer, float x, float z, float dist, float yaw_deg, bool overhead)
	{
		const float 弧度 = yaw_deg * 3.14159265f / 180.0f;
		const float 眼x = x + std::sin(弧度) * dist;
		const float 眼z = z + std::cos(弧度) * dist;
		//高空俯视：抬高眼位并向下看
		const float 眼y = overhead ? 18.0f : 2.2f;
		renderer.camera_position_set(眼x, 眼y, 眼z);
		renderer.camera_target_set(x, overhead ? 0.5f : 1.4f, z);
	}
}

/*
多距离 ＋ 多角度可见性（第四百零二轮）
展廊为 3 列（x = −14 / 0 / 14）× 8 排（z = −10 … −150），逐格扫描并输出覆盖像素数。
*/
TEST(Renderer_Showcase_Probe_Test, 各格多距离多角度均有显示)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(probe_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::能力展示场景 scene;
	ASSERT_TRUE(scene.init(renderer));

	const float columns[3] = { -14.0f, 0.0f, 14.0f };
	//四种距离 ＋ 五种朝向（含侧视 90° 与背视 180°）＋ 高空俯视＝ 21 种组合
	const float distances[4] = { 12.0f, 20.0f, 30.0f, 50.0f };
	const float yaws[5] = { -20.0f, 0.0f, 20.0f, 90.0f, 180.0f };
	uint32_t empty_cases = 0;
	for (uint32_t row = 0; row < 8; ++row)
	{
		const float z = -10.0f - 20.0f * static_cast<float>(row);
		for (uint32_t column = 0; column < 3; ++column)
		{
			const float x = columns[column];
			uint32_t minimum_count = 0xFFFFFFFFu;
			uint32_t maximum_count = 0;
			float minimum_luma = 1.0e9f;
			//四种距离 × 五种朝向 ＋ 高空俯视
			for (uint32_t sample = 0; sample < 21; ++sample)
			{
				if (sample < 20)
					aim_camera(renderer, x, z, distances[sample / 5], yaws[sample % 5], false);
				else
					aim_camera(renderer, x, z, 18.0f, 0.0f, true);
				//参考帧（同相机、只清屏 ＋ 天空）
				std::vector<uint8_t> reference(probe_bytes());
				ASSERT_TRUE(renderer.frame_render(reference.data(), static_cast<uint32_t>(reference.size())));
				//展示帧
				scene.frame_draw(renderer);
				std::vector<uint8_t> shown(probe_bytes());
				ASSERT_TRUE(renderer.frame_render(shown.data(), static_cast<uint32_t>(shown.size())));
				const uint32_t count = changed_pixels(reference, shown);
				const float luma = average_luma(shown);
				if (count < minimum_count)
					minimum_count = count;
				if (count > maximum_count)
					maximum_count = count;
				if (luma < minimum_luma)
					minimum_luma = luma;
			}
			std::cout << "[展示排查] 格 (" << x << ", " << z << ") 覆盖 " << minimum_count
				<< " .. " << maximum_count << " 最低亮度 " << minimum_luma << std::endl;
			//画面过暗也视为「看不清」（条形灯与地台都应有可见亮度）
			if (minimum_count < VISIBLE_MIN_PIXELS || minimum_luma < 18.0f)
				++empty_cases;
		}
	}
	EXPECT_EQ(empty_cases, 0u) << "存在显示覆盖不足的展区格";
	scene.shutdown(renderer);
}

/*
文字字形完整性（第四百零二轮）
把 23 条展区标题一次性绘制（22 px 字号），断言无码点被跳过。
*/
TEST(Renderer_Showcase_Probe_Test, 展区标题无缺字)
{
	std::error_code code;
	const char* candidates[] = {
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
	};
	std::string font_path;
	for (const char* candidate : candidates)
	{
		if (std::filesystem::exists(std::filesystem::path(candidate), code))
		{
			font_path = candidate;
			break;
		}
	}
	//本机无中文字体：跳过
	if (font_path.empty())
		GTEST_SKIP() << "本机无测试用中文字体";

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(probe_config()));
	uint64_t font = 0;
	ASSERT_TRUE(renderer.font_load_file(font, font_path, 22u));
	engine::能力展示场景 scene;
	ASSERT_TRUE(scene.init(renderer));
	//23 条标题一次性绘制（长中文串连绘，验证图集扩容）
	for (uint32_t index = 0; index < scene.展区数量(); ++index)
		(void)renderer.text_draw(font, scene.展区标题(index), 4.0f, 4.0f + 24.0f * static_cast<float>(index));
	std::vector<uint8_t> pixels(probe_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_EQ(renderer.font_skipped_codepoints(font), 0u) << "展区标题存在被跳过的字形";
	renderer.font_unload(font);
	scene.shutdown(renderer);
}
