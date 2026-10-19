/*
渲染采样器各向异性测试（L2 隐藏窗口 + 像素回读）
覆盖：采样器各向异性能力的查询入口；开启各向异性与收窄细节层级后绘制链路仍正常
      （结构性断言 + 像素级旁证：中心像素被纹理覆盖而非清屏色）。
各向异性对像素的**具体效果**依赖具体硬件（各向异性质量与驱动相关），
故本用例不断言「滤镜效果差异」，只断言「状态可设、采样器有效、绘制与回读正常」。
断言一律「格式无关」：清屏色用双编码候选 ＋ 容差 ＋ 通道排序判定。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含采样器状态与寻址模式）
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
}

//渲染采样器各向异性测试夹具
class Renderer_Sampler_Anisotropy_Test : public ::testing::Test
{
};

//未初始化时设置采样器状态无副作用（能力查询返回保守值）
TEST_F(Renderer_Sampler_Anisotropy_Test, 未初始化设置采样器状态无副作用)
{
	engine::Renderer renderer;
	//未初始化时能力查询应为保守值
	EXPECT_FALSE(renderer.sampler_anisotropy_supported());
	EXPECT_FLOAT_EQ(renderer.sampler_max_anisotropy(), 1.0f);
	//未初始化时设置状态应静默返回（不崩溃）
	engine::Texture_Sampler_State state;
	state.anisotropy = true;
	state.max_anisotropy = 16.0f;
	state.min_lod = 0.0f;
	state.max_lod = 3.0f;
	renderer.texture_sampler_state_set(state);
	EXPECT_FALSE(renderer.valid());
}

//各向异性与细节层级能力可查询且取值有效
TEST_F(Renderer_Sampler_Anisotropy_Test, 各向异性能力可查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//设备上限恒不小于 1
	float max_anisotropy = renderer.sampler_max_anisotropy();
	EXPECT_GE(max_anisotropy, 1.0f);
	//未启用该特性时必须为 1（本层归一结果）
	if (!renderer.sampler_anisotropy_supported())
		EXPECT_FLOAT_EQ(max_anisotropy, 1.0f);
	//打印便于人工核对（本机若支持通常为 16）
	RecordProperty("sampler_anisotropy_supported", renderer.sampler_anisotropy_supported() ? "true" : "false");
	RecordProperty("sampler_max_anisotropy", max_anisotropy);
}

//开启各向异性并收窄细节层级后，采样器仍有效且绘制 / 回读正常
TEST_F(Renderer_Sampler_Anisotropy_Test, 各向异性与细节层级设置后仍可正常绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//程序生成棋盘格纹理（两种灰度交替，中心像素必为纹理色而非清屏色）
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, 32, 32, 4));
	//加载四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//第一帧：开启各向异性（倍数取设备上限）并启用 mip 过滤
	engine::Texture_Sampler_State anisotropic;
	anisotropic.anisotropy = true;
	anisotropic.max_anisotropy = renderer.sampler_max_anisotropy();
	anisotropic.mip_filter = true;
	renderer.texture_sampler_state_set(anisotropic);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> first(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(first.data(), static_cast<uint32_t>(first.size())));
	//中心像素应被纹理覆盖（非清屏色）——旁证采样器有效且绘制链路通畅
	std::array<uint8_t, 4> first_center = pixel_get(first, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_FALSE(pixel_is_clear(first_center)) << "开启各向异性后中心像素仍为清屏色：" << pixel_text(first_center);
	//四角未被四边形覆盖，仍为清屏色
	EXPECT_TRUE(pixel_is_clear(pixel_get(first, 1, 1))) << "角落被意外覆盖";

	//第二帧：显式收窄细节层级（minLod = maxLod = 0，等价于只看第 0 级）并重复设置同一状态（应为空操作）
	engine::Texture_Sampler_State narrowed;
	narrowed.anisotropy = true;
	narrowed.max_anisotropy = renderer.sampler_max_anisotropy();
	narrowed.mip_filter = true;
	narrowed.min_lod = 0.0f;
	narrowed.max_lod = 0.0f;
	renderer.texture_sampler_state_set(narrowed);
	//重复设置同一状态（空操作路径）
	renderer.texture_sampler_state_set(narrowed);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> second(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(second.data(), static_cast<uint32_t>(second.size())));
	std::array<uint8_t, 4> second_center = pixel_get(second, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_FALSE(pixel_is_clear(second_center)) << "收窄细节层级后中心像素仍为清屏色：" << pixel_text(second_center);

	//第三帧：切回「不采样 mip」的默认档，仍应正常绘制（状态可往返）
	engine::Texture_Sampler_State plain;
	renderer.texture_sampler_state_set(plain);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> third(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(third.data(), static_cast<uint32_t>(third.size())));
	EXPECT_FALSE(pixel_is_clear(pixel_get(third, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "切回默认采样器状态后中心像素为清屏色";

	//三帧均应正常推进
	EXPECT_EQ(renderer.frame_count(), 3u);

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}
