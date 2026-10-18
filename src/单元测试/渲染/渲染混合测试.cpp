/*
渲染混合态测试（L2 隐藏窗口 + 像素回读）
覆盖：不透明档忽略源 alpha（alpha 原样写出）、切到 alpha 档后发生半透明叠加、
      切回不透明档可恢复；断言一律「通道色三通道与 alpha 分离、通道序无关、容差判定」，
      对 sRGB 与非 sRGB 附件、BGRA 与 RGBA 通道序均成立（半透明期望值随附件传输函数不同，
      故用区间而非定值）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含混合档枚举）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//半透明纹理的源 alpha（128/255 ≈ 0.502）
	constexpr uint8_t SOURCE_ALPHA = 128;

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
	颜色三通道的最小值（剔除 alpha 通道，避免与混合后的 alpha 值混淆）
	注意：本层包含链里存在 `min` 宏（Windows 头），`std::min({...})` 会被宏破坏（C2589），
	故这里用显式比较而不用 std::min。
	*/
	uint8_t color_min(const std::array<uint8_t, 4>& value)
	{
		//先取三通道首元素
		uint8_t result = value[0];
		//若第二通道更小
		if (value[1] < result)
			result = value[1];
		//若第三通道更小
		if (value[2] < result)
			result = value[2];
		return result;
	}

	//清屏色候选（渲染器清屏色 0.05 / 0.05 / 0.10：UNORM ≈ 13/13/26，sRGB ≈ 63/63/89）
	constexpr std::array<std::array<uint8_t, 3>, 2> CLEAR_CANDIDATES = { {
		{ { 13, 13, 26 } },
		{ { 63, 63, 89 } },
	} };
	//清屏色通道容差
	constexpr int CLEAR_TOLERANCE = 4;

	//判断像素是否近似等于清屏色（排序后与两种编码候选比对）
	bool pixel_is_clear(std::array<uint8_t, 4> value)
	{
		//排序后的四通道
		std::array<uint8_t, 4> sorted = value;
		std::sort(sorted.begin(), sorted.end());
		//末位须为不透明
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

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造半透明白纹理（RGBA8：rgb = 255，alpha = SOURCE_ALPHA）
	std::vector<uint8_t> translucent_white(uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 255;
			pixels[index + 1] = 255;
			pixels[index + 2] = 255;
			pixels[index + 3] = SOURCE_ALPHA;
		}
		return pixels;
	}
}

//渲染混合态测试夹具
class Renderer_Blend_Test : public ::testing::Test
{
};

//不透明档：源 alpha 被忽略（颜色写实白），alpha 原样写出
TEST_F(Renderer_Blend_Test, 不透明档忽略源alpha)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载 2×2 半透明白纹理并绑定
	uint64_t texture = 0;
	std::vector<uint8_t> texels = translucent_white(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//加载四边形并登记绘制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//中心像素：颜色三通道为实白（源 alpha 未参与混合）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(color_min(center), 250) << "不透明档下中心颜色非纹理色：" << pixel_text(center);
	//中心像素：alpha 原样写出（约等于源 alpha）
	//中心像素 alpha（BGRA 与 RGBA 下 alpha 均恒为第 4 字节，故直接取下标 3）
	uint8_t center_alpha = center[3];
	EXPECT_GE(center_alpha, SOURCE_ALPHA - 8) << "不透明档下 alpha 未原样写出：" << pixel_text(center);
	EXPECT_LE(center_alpha, SOURCE_ALPHA + 8) << "不透明档下 alpha 未原样写出：" << pixel_text(center);
	//角落未被四边形覆盖，仍为清屏色
	EXPECT_TRUE(pixel_is_clear(pixel_get(pixels, 1, 1))) << "角落被意外覆盖";
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

//alpha 档：发生半透明叠加（结果严格介于清屏色与纹理白之间）
TEST_F(Renderer_Blend_Test, alpha档产生半透明叠加)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载半透明纹理并绑定
	uint64_t texture = 0;
	std::vector<uint8_t> texels = translucent_white(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//切到 alpha 档（触发一次图形管线重建）
	renderer.blend_mode_set(engine::Blend_Mode::Alpha);
	//加载四边形并登记绘制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	/*
	中心像素：颜色三通道落在 [100, 240]
	下界 100 明显高于清屏色高通道（UNORM 26 / sRGB 89），上界 240 明显低于纹理白 255，
	故区间命中即证明「源 alpha 与背景真实混合」；UNORM 附件期望 ≈134、sRGB 附件期望 ≈192。
	*/
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	uint8_t min_color = color_min(center);
	EXPECT_GE(min_color, 100) << "半透明叠加结果过暗（近似清屏色）：" << pixel_text(center);
	EXPECT_LE(min_color, 240) << "半透明叠加结果过亮（近似纹理原色）：" << pixel_text(center);
	//中心像素：alpha 为「源 alpha 与背景 alpha(1.0)」混合值（≈0.75 → 191）
	//中心像素 alpha（BGRA 与 RGBA 下 alpha 均恒为第 4 字节，故直接取下标 3）
	uint8_t center_alpha = center[3];
	EXPECT_GE(center_alpha, 180) << "混合后 alpha 偏低：" << pixel_text(center);
	EXPECT_LE(center_alpha, 205) << "混合后 alpha 偏高：" << pixel_text(center);
	//角落未被四边形覆盖，仍为清屏色（混合只作用于被覆盖区域）
	EXPECT_TRUE(pixel_is_clear(pixel_get(pixels, 1, 1))) << "角落被意外覆盖";
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

//切回不透明档：半透明叠加消失，颜色回到实白（档位可往返）
TEST_F(Renderer_Blend_Test, 切回不透明档后恢复实白)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//加载半透明纹理并绑定
	uint64_t texture = 0;
	std::vector<uint8_t> texels = translucent_white(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//加载四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//第一帧：alpha 档
	renderer.blend_mode_set(engine::Blend_Mode::Alpha);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> blend_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(blend_pixels.data(), static_cast<uint32_t>(blend_pixels.size())));
	//混合帧中心应明显暗于实白
	uint8_t blend_min = color_min(pixel_get(blend_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_LE(blend_min, 240) << "混合帧中心未落在混合区间";

	//第二帧：切回不透明档（待绘制项逐帧登记，故须重新 mesh_draw）
	renderer.blend_mode_set(engine::Blend_Mode::Opaque);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> opaque_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(opaque_pixels.data(), static_cast<uint32_t>(opaque_pixels.size())));
	//恢复帧中心应为实白
	std::array<uint8_t, 4> center = pixel_get(opaque_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_GE(color_min(center), 250) << "切回不透明档后中心未被恢复为实白：" << pixel_text(center);

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}
