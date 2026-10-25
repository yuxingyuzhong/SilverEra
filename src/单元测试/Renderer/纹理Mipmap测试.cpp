/*
纹理 mipmap 与采样器状态测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化设置采样器状态无副作用、启用 mip 过滤后仍可正常绘制、
      缩小绘制时 mip 过滤真实生效（对比「关 / 开」两帧的对比度）、寻址模式可配（重复 vs 边缘钳制）。
断言口径：通道序无关（按三通道排序比较，或用「全通道同时偏高 / 偏低」判定）+ 只用白 / 黑两色，
      避免依赖表面格式的通道序；不使用 std::min（本层包含链存在 min 宏，会触发 C2589）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

//获取渲染器门面（含采样器状态类型）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//棋盘格纹理规格（64×64、格 8 → 共 8×8 格，mip 级数 7）
	constexpr uint32_t CHECKER_SIZE = 64;
	constexpr uint32_t CHECKER_CELL = 8;
	//缩小绘制所用小四边形半边长（世界尺度；屏幕上约 5~6 像素宽，采样密度远高于 1）
	constexpr float SMALL_QUAD_HALF = 0.2f;
	/*
	细节层级偏置用例的中等四边形半边长（屏幕约 15~18 像素）
	基准 LOD 落在 mip 链中段（棋盘格纹理仍有可见明暗差），故 -2 更锐、+2 更糊两个方向都可观察。
	*/
	constexpr float MEDIUM_QUAD_HALF = 0.6f;

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

	//三通道升序（用于通道序无关判定）
	std::array<uint8_t, 3> color_sorted(const std::array<uint8_t, 4>& value)
	{
		std::array<uint8_t, 3> color = { value[0], value[1], value[2] };
		std::sort(color.begin(), color.end());
		return color;
	}

	//三通道亮度和（用于区域对比度统计）
	uint32_t color_sum(const std::array<uint8_t, 4>& value)
	{
		return static_cast<uint32_t>(value[0]) + value[1] + value[2];
	}

	/*
	清屏色候选与判定（只统计「落在四边形内」的像素，避免清屏色拉大对比度）
	清屏色 0.05 / 0.05 / 0.10：非 sRGB 附件 ≈ 13/13/26，sRGB 附件 ≈ 63/63/89。
	*/
	constexpr std::array<std::array<uint8_t, 3>, 2> CLEAR_CANDIDATES = { {
		{ { 13, 13, 26 } },
		{ { 63, 63, 89 } },
	} };
	//清屏色通道容差
	constexpr int CLEAR_TOLERANCE = 6;

	//判断像素是否近似等于清屏色（排序后与两种编码候选比对；判定与通道序无关）
	bool pixel_is_clear(const std::array<uint8_t, 4>& value)
	{
		//排序后的三通道
		std::array<uint8_t, 3> sorted = color_sorted(value);
		//末通道须为不透明
		if (value[3] < 250)
			return false;
		//逐候选比对
		for (const std::array<uint8_t, 3>& candidate : CLEAR_CANDIDATES)
		{
			//三个通道均在容差内即命中
			bool matched = true;
			for (std::size_t index = 0; index < 3; ++index)
			{
				//本通道与候选值的差
				int delta = static_cast<int>(sorted[index]) - static_cast<int>(candidate[index]);
				//若超出容差
				if (delta < -CLEAR_TOLERANCE || delta > CLEAR_TOLERANCE)
					matched = false;
			}
			if (matched)
				return true;
		}
		return false;
	}

	/*
	四边形区域对比度
	取以 (center_x, center_y) 为中心、半径 radius 的方形区域内**非清屏色**像素的
	「三通道亮度和」的极值差；越接近 0 表示该区域越均匀（mip 生效后趋于均匀）。
	只统计非清屏色像素，避免四边形外背景把对比度拉大而掩盖真实差异。
	*/
	uint32_t region_spread(const std::vector<uint8_t>& pixels, uint32_t center_x, uint32_t center_y, uint32_t radius)
	{
		//区域内（仅四边形内像素）亮度和的极小 / 极大值
		uint32_t minimum = 0xFFFFFFFFu;
		uint32_t maximum = 0;
		//参与统计的像素数
		uint32_t sample_count = 0;
		for (uint32_t y = center_y - radius; y <= center_y + radius; ++y)
			for (uint32_t x = center_x - radius; x <= center_x + radius; ++x)
			{
				//当前像素
				std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
				//非四边形内像素（清屏色）不参与统计
				if (pixel_is_clear(value))
					continue;
				//当前亮度和
				uint32_t sum = color_sum(value);
				//更新极值
				if (sum < minimum)
					minimum = sum;
				if (sum > maximum)
					maximum = sum;
				++sample_count;
			}
		//样本不足（区域未被覆盖）时返回 0
		if (sample_count < 2)
			return 0;
		return maximum - minimum;
	}

	//构造中心四边形（世界边长为 2 × half；纹理坐标由参数给出）
	engine::Mesh_Data quad_data(float half, float u0, float u1, float v0, float v1)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -half, -half, 0.0f }, { u0, v1 } },
			{ { half, -half, 0.0f }, { u1, v1 } },
			{ { half, half, 0.0f }, { u1, v0 } },
			{ { -half, half, 0.0f }, { u0, v0 } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造左右两色 2×2 纹理（左列白、右列黑；RGBA8）
	std::vector<uint8_t> two_tone_2x2(void)
	{
		std::vector<uint8_t> pixels(2 * 2 * 4, 255);
		//右列置黑（通道 0/1 为列索引 1 的两行）
		for (uint32_t row = 0; row < 2; ++row)
		{
			//右列像素起始偏移
			std::size_t index = (static_cast<std::size_t>(row) * 2 + 1) * 4;
			pixels[index + 0] = 0;
			pixels[index + 1] = 0;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}
}

//纹理 mipmap 与采样器状态测试夹具
class Renderer_Mipmap_Test : public ::testing::Test
{
};

//未初始化时设置采样器状态无副作用
TEST_F(Renderer_Mipmap_Test, 未初始化设置采样器状态无副作用)
{
	engine::Renderer renderer;
	//未初始化即调用（应静默返回，不崩溃）
	engine::Texture_Sampler_State state;
	state.address_mode = engine::Texture_Address_Mode::Clamp_To_Edge;
	state.mip_filter = true;
	renderer.texture_sampler_state_set(state);
	//后续调用仍按未初始化处理
	uint64_t texture = 0;
	EXPECT_FALSE(renderer.texture_load_checkerboard(texture, 8, 8, 2));
}

//启用 mip 过滤后棋盘格纹理仍可正常绘制
TEST_F(Renderer_Mipmap_Test, 启用mip过滤后仍可正常绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//棋盘格纹理（64×64 → 生成 7 级 mip）
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, CHECKER_SIZE, CHECKER_SIZE, CHECKER_CELL));
	renderer.texture_bind_set(texture);
	//启用 mip 过滤
	engine::Texture_Sampler_State state;
	state.mip_filter = true;
	renderer.texture_sampler_state_set(state);
	//满屏覆盖的四边形 + 跑帧
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(1.0f, 0.0f, 1.0f, 0.0f, 1.0f)));
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
	//中心像素应为棋盘格的两种灰度之一（非清屏色），即绘制路径未被 mip 破坏
	std::array<uint8_t, 3> center = color_sorted(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_TRUE(center[0] >= 30 && center[0] == center[1] && center[1] == center[2])
		<< "启用 mip 过滤后中心非棋盘格灰度：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

//缩小绘制时 mip 过滤真实生效（关 / 开两帧对比）
TEST_F(Renderer_Mipmap_Test, 缩小绘制时mip生效)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//棋盘格纹理（64×64、格 8）
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, CHECKER_SIZE, CHECKER_SIZE, CHECKER_CELL));
	renderer.texture_bind_set(texture);
	//小四边形（屏幕覆盖约 5~6 像素 → 每像素跨约 13 个纹素，属明显缩小绘制）
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(SMALL_QUAD_HALF, 0.0f, 1.0f, 0.0f, 1.0f)));

	//第一帧：关闭 mip 过滤（maxLod = 0，等价于只采第 0 级）
	engine::Texture_Sampler_State without_mip;
	without_mip.mip_filter = false;
	renderer.texture_sampler_state_set(without_mip);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> no_mip_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(no_mip_pixels.data(), static_cast<uint32_t>(no_mip_pixels.size())));

	//第二帧：启用 mip 过滤（几何与纹理完全不变，只改采样器状态）
	engine::Texture_Sampler_State with_mip;
	with_mip.mip_filter = true;
	renderer.texture_sampler_state_set(with_mip);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> mip_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(mip_pixels.data(), static_cast<uint32_t>(mip_pixels.size())));

	/*
	对比度对比
	关闭 mip 时，小四边形内应出现明显黑白交替（高对比）；启用 mip 后采到较高级别，
	棋盘格在低级别趋于均值（**区域内近乎均匀**）。故断言「关帧对比度显著、开帧近乎均匀」。
	*/
	uint32_t no_mip_spread = region_spread(no_mip_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	uint32_t mip_spread = region_spread(mip_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	EXPECT_GE(no_mip_spread, 150u) << "关闭 mip 时该区域对比度过低，无法证明对比有效（值 " << no_mip_spread << "）";
	EXPECT_LE(mip_spread, 60u) << "启用 mip 后该区域仍不均匀（值 " << mip_spread << "），mip 链未生效";
	EXPECT_LT(mip_spread, no_mip_spread) << "启用 mip 后对比度未下降（关 " << no_mip_spread
		<< " vs 开 " << mip_spread << "）";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

//寻址模式可配（重复 vs 边缘钳制）
TEST_F(Renderer_Mipmap_Test, 寻址模式可配)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//左右两色纹理（左白右黑）
	uint64_t texture = 0;
	std::vector<uint8_t> texels = two_tone_2x2();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	//纹理坐标 u ∈ [1, 2]（越界一倍；v 正常），用于区分重复与边缘钳制
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(1.0f, 1.0f, 2.0f, 0.0f, 1.0f)));

	//第一帧：重复寻址（默认）→ u = 1.5 回绕到 0.5，落在白 / 黑交界 → 中间灰
	engine::Texture_Sampler_State repeat_state;
	repeat_state.address_mode = engine::Texture_Address_Mode::Repeat;
	renderer.texture_sampler_state_set(repeat_state);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> repeat_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(repeat_pixels.data(), static_cast<uint32_t>(repeat_pixels.size())));

	//第二帧：边缘钳制 → u = 1.5 钳到 1.0，取最右一列（黑）
	engine::Texture_Sampler_State clamp_state;
	clamp_state.address_mode = engine::Texture_Address_Mode::Clamp_To_Edge;
	renderer.texture_sampler_state_set(clamp_state);
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> clamp_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(clamp_pixels.data(), static_cast<uint32_t>(clamp_pixels.size())));

	//重复寻址：三通道均在中间区间（白黑各半的混合灰）
	std::array<uint8_t, 3> repeat_center = color_sorted(pixel_get(repeat_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_GE(repeat_center[0], 60) << "重复寻址下中心非混合灰：" << pixel_text(pixel_get(repeat_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_LE(repeat_center[2], 220) << "重复寻址下中心非混合灰：" << pixel_text(pixel_get(repeat_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));

	//边缘钳制：三通道全部偏低（取到最右一列的黑）
	std::array<uint8_t, 3> clamp_center = color_sorted(pixel_get(clamp_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_LE(clamp_center[2], 40) << "边缘钳制下中心未取到边缘黑色：" << pixel_text(pixel_get(clamp_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));

	//两种模式结果必须不同（证寻址模式可配且真实生效）
	EXPECT_NE(color_sum(pixel_get(repeat_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)),
		color_sum(pixel_get(clamp_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2))) << "两种寻址模式结果相同，寻址配置未生效";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

/*
采样器 · 细节层级偏置（第七十七轮）
口径：复用「缩小绘制」场景（64×64 棋盘格 ＋ 屏幕约 5~6 像素的小四边形）；
      同一几何与纹理下只改 `lod_bias`：负值使硬件 LOD 下移、采到更高级别（更锐利），
      正值使 LOD 上移、采到更低级别（更模糊）；以区域对比度为观测量。
*/
TEST_F(Renderer_Mipmap_Test, 细节层级偏置改变缩小采样)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//棋盘格纹理与小四边形（与「缩小绘制时mip生效」同一前提）
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load_checkerboard(texture, CHECKER_SIZE, CHECKER_SIZE, CHECKER_CELL));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(MEDIUM_QUAD_HALF, 0.0f, 1.0f, 0.0f, 1.0f)));

	//按给定偏置渲染一帧（void λ 以允许 ASSERT）
	auto render_with_bias = [&](float bias, std::vector<uint8_t>& pixels)
	{
		engine::Texture_Sampler_State state;
		state.mip_filter = true;
		state.lod_bias = bias;
		renderer.texture_sampler_state_set(state);
		ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
		pixels.assign(pixel_bytes(), 0);
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	};

	//三帧：更锐利（-2）／基准（0）／更模糊（+2）
	std::vector<uint8_t> sharp_pixels;
	std::vector<uint8_t> baseline_pixels;
	std::vector<uint8_t> blur_pixels;
	render_with_bias(-2.0f, sharp_pixels);
	render_with_bias(0.0f, baseline_pixels);
	render_with_bias(2.0f, blur_pixels);
	uint32_t sharp_spread = region_spread(sharp_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	uint32_t baseline_spread = region_spread(baseline_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	uint32_t blur_spread = region_spread(blur_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, 3);
	RecordProperty("lod_bias_sharp_spread", static_cast<int>(sharp_spread));
	RecordProperty("lod_bias_baseline_spread", static_cast<int>(baseline_spread));
	RecordProperty("lod_bias_blur_spread", static_cast<int>(blur_spread));

	//负偏置应比基准更锐利（区域对比度更高）
	EXPECT_GT(sharp_spread, baseline_spread) << "负偏置未提升细节（锐 "
		<< sharp_spread << " vs 基准 " << baseline_spread << "）";
	//正偏置应比基准更模糊（区域对比度更低）
	EXPECT_LT(blur_spread, baseline_spread) << "正偏置未降低细节（糊 "
		<< blur_spread << " vs 基准 " << baseline_spread << "）";
	//锐利帧实际发生了画面变化（排除偏置被忽略）
	EXPECT_GT(sharp_spread, baseline_spread + 10u) << "偏置影响过弱，疑未生效";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

/*
采样器状态可读与钳制（第七十七轮；第九十七轮改按设备上限钳制）
口径：默认状态即既有行为；设置后 `texture_sampler_state_get` 返回生效值；
      lod_bias 越界钳到设备上限 ±maxSamplerLodBias（可经 sampler_max_lod_bias 查询）、非数归 0；
      未初始化返回默认状态。
*/
TEST_F(Renderer_Mipmap_Test, 采样器状态可读与钳制)
{
	engine::Renderer renderer;
	//未初始化：返回默认状态
	engine::Texture_Sampler_State before_init = renderer.texture_sampler_state_get();
	EXPECT_EQ(before_init.lod_bias, 0.0f);
	EXPECT_FALSE(before_init.mip_filter);

	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认值（与本轮之前行为一致）
	engine::Texture_Sampler_State default_state = renderer.texture_sampler_state_get();
	EXPECT_EQ(default_state.address_mode, engine::Texture_Address_Mode::Repeat);
	EXPECT_FALSE(default_state.mip_filter);
	EXPECT_EQ(default_state.min_lod, 0.0f);
	EXPECT_EQ(default_state.max_lod, -1.0f);
	EXPECT_EQ(default_state.lod_bias, 0.0f);

	//合法值原样保留
	engine::Texture_Sampler_State legal;
	legal.mip_filter = true;
	legal.lod_bias = -2.0f;
	renderer.texture_sampler_state_set(legal);
	EXPECT_EQ(renderer.texture_sampler_state_get().lod_bias, -2.0f);
	EXPECT_TRUE(renderer.texture_sampler_state_get().mip_filter);

	//设备上限（第九十七轮：钳制上界随设备能力）
	float lod_bias_limit = renderer.sampler_max_lod_bias();
	ASSERT_GT(lod_bias_limit, 0.0f);

	//上界钳制
	engine::Texture_Sampler_State high;
	high.lod_bias = 99.0f;
	renderer.texture_sampler_state_set(high);
	EXPECT_EQ(renderer.texture_sampler_state_get().lod_bias, lod_bias_limit);

	//下界钳制
	engine::Texture_Sampler_State low;
	low.lod_bias = -99.0f;
	renderer.texture_sampler_state_set(low);
	EXPECT_EQ(renderer.texture_sampler_state_get().lod_bias, -lod_bias_limit);

	//非数归 0
	engine::Texture_Sampler_State not_a_number;
	not_a_number.lod_bias = std::numeric_limits<float>::quiet_NaN();
	renderer.texture_sampler_state_set(not_a_number);
	EXPECT_EQ(renderer.texture_sampler_state_get().lod_bias, 0.0f);
}
