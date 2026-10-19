/*
渲染实例化绘制测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化守卫、实例数为零 / 无效句柄被拒、N 个实例各自可见（含分离性）、
      实例数为 1 与逐次绘制等价。
断言口径：通道序无关 + 容差；颜色三通道与 alpha 分离（alpha 恒取第 4 字节）；
      不使用 std::min（本层包含链存在 min 宏，会触发 C2589）。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//实例化用例所用小四边形半边长（世界尺度；默认相机在 +Z 3 单位处、竖向半高 ≈ 1.732）
	constexpr float QUAD_HALF = 0.25f;
	//三个实例的竖向平移（±0.9 落在视口内且彼此分离）
	constexpr float INSTANCE_SHIFT = 0.9f;
	//纹理色判定阈值（内置默认白纹理，四通道近满值）
	constexpr uint8_t TEXTURE_WHITE_MIN = 250;

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

	//颜色三通道最小值（剔除 alpha；避免 std::min 被 min 宏破坏）
	uint8_t color_min(const std::array<uint8_t, 4>& value)
	{
		//先取三通道首元素
		uint8_t result = value[0];
		//若第二 / 第三通道更小则更新
		if (value[1] < result)
			result = value[1];
		if (value[2] < result)
			result = value[2];
		return result;
	}

	//判断像素是否为纹理白（三通道均接近满值）
	bool pixel_is_white(const std::array<uint8_t, 4>& value)
	{
		return color_min(value) >= TEXTURE_WHITE_MIN;
	}

	//统计纹理白像素数量
	uint32_t white_count(const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				//命中即计数
				if (pixel_is_white(pixel_get(pixels, x, y)))
					++count;
		return count;
	}

	//统计指定行区间内的纹理白像素数量（半开区间 [row_begin, row_end)）
	uint32_t white_count_rows(const std::vector<uint8_t>& pixels, uint32_t row_begin, uint32_t row_end)
	{
		uint32_t count = 0;
		for (uint32_t y = row_begin; y < row_end; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
				//命中即计数
				if (pixel_is_white(pixel_get(pixels, x, y)))
					++count;
		return count;
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

	//构造中心四边形（世界边长为 2 × half，位于 z = 0）
	engine::Mesh_Data quad_data(float half)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -half, -half, 0.0f }, { 0.0f, 0.0f } },
			{ { half, -half, 0.0f }, { 1.0f, 0.0f } },
			{ { half, half, 0.0f }, { 1.0f, 1.0f } },
			{ { -half, half, 0.0f }, { 0.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造平移模型矩阵（列主序：末列为平移量）
	std::array<float, 16> translation(float x, float y, float z)
	{
		return std::array<float, 16>{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f
		};
	}
}

//渲染实例化绘制测试夹具
class Renderer_Instanced_Test : public ::testing::Test
{
};

//未初始化时实例化绘制失败
TEST_F(Renderer_Instanced_Test, 未初始化实例化绘制失败)
{
	engine::Renderer renderer;
	std::array<float, 16> transform = translation(0.0f, 0.0f, 0.0f);
	EXPECT_FALSE(renderer.mesh_draw_instanced(0, transform.data(), 1));
	EXPECT_FALSE(renderer.mesh_draw_instanced(1, transform.data(), 3));
}

//实例数为零或句柄无效时被拒
TEST_F(Renderer_Instanced_Test, 实例数为零或句柄无效被拒)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(QUAD_HALF)));
	std::array<float, 16> transform = translation(0.0f, 0.0f, 0.0f);
	//实例数为零被拒
	EXPECT_FALSE(renderer.mesh_draw_instanced(handle, transform.data(), 0));
	//无效句柄（未加载过的编号）被拒
	EXPECT_FALSE(renderer.mesh_draw_instanced(handle + 100, transform.data(), 1));
	//卸载后的句柄被拒
	renderer.mesh_unload(handle);
	EXPECT_FALSE(renderer.mesh_draw_instanced(handle, transform.data(), 1));
}

//三个实例各自可见且彼此分离
TEST_F(Renderer_Instanced_Test, 三个实例各自可见且彼此分离)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//回落内置默认白纹理
	renderer.texture_bind_set(0);
	//加载中心小四边形
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(QUAD_HALF)));

	//第一帧：单实例（恒等变换，落在视口中央）作为「一份实例」的面积基准
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));
	uint32_t single_white = white_count(single_pixels);
	ASSERT_GT(single_white, 0u) << "单实例帧未出现纹理色像素：绘制路径异常";

	//第二帧：三个实例（上 / 中 / 下分离）
	std::array<float, 16> transforms[3] = {
		translation(0.0f, INSTANCE_SHIFT, 0.0f),
		translation(0.0f, 0.0f, 0.0f),
		translation(0.0f, -INSTANCE_SHIFT, 0.0f),
	};
	ASSERT_TRUE(renderer.mesh_draw_instanced(handle, transforms[0].data(), 3));
	std::vector<uint8_t> instanced_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(instanced_pixels.data(), static_cast<uint32_t>(instanced_pixels.size())));

	//① 面积近似三倍（三个分离实例；留 2 倍下限以吸收取整误差）
	uint32_t instanced_white = white_count(instanced_pixels);
	EXPECT_GE(instanced_white, single_white * 2) << "实例化帧的纹理色面积未随实例数增长："
		<< "单实例 " << single_white << " 像素，三实例 " << instanced_white << " 像素";

	//② 中心（恒等实例）为纹理色
	std::array<uint8_t, 4> center = pixel_get(instanced_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_white(center)) << "中心实例不可见：" << pixel_text(center);

	//③ 上下两个平移实例各自落在顶部 / 底部三分之一内（证明它们真的被画出来了）
	uint32_t top_white = white_count_rows(instanced_pixels, 0, WINDOW_HEIGHT / 3);
	uint32_t bottom_white = white_count_rows(instanced_pixels, WINDOW_HEIGHT - WINDOW_HEIGHT / 3, WINDOW_HEIGHT);
	EXPECT_GT(top_white, 0u) << "上方平移实例未出现（顶部三分之一无纹理色像素）";
	EXPECT_GT(bottom_white, 0u) << "下方平移实例未出现（底部三分之一无纹理色像素）";

	/*
	④ 实例彼此分离：上实例与中心实例之间的行仍为清屏色
	几何：中心实例约占第 15~21 行、上平移实例约占第 6~11 行，故取第 13 行作间隙探针。
	*/
	uint32_t gap_row = WINDOW_HEIGHT / 2 - 5;
	std::array<uint8_t, 4> gap = pixel_get(instanced_pixels, WINDOW_WIDTH / 2, gap_row);
	EXPECT_TRUE(pixel_is_clear(gap)) << "实例之间未分离（第 " << gap_row << " 行被覆盖）：" << pixel_text(gap);

	renderer.mesh_unload(handle);
}

//实例数为 1 时与逐次绘制等价
TEST_F(Renderer_Instanced_Test, 实例数为一与逐次绘制等价)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//回落内置默认白纹理
	renderer.texture_bind_set(0);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(QUAD_HALF)));

	//第一帧：逐次绘制路径（mesh_draw）
	ASSERT_TRUE(renderer.mesh_draw(handle, nullptr));
	std::vector<uint8_t> draw_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(draw_pixels.data(), static_cast<uint32_t>(draw_pixels.size())));

	//第二帧：实例化路径（实例数 = 1）
	std::array<float, 16> transform = translation(0.0f, 0.0f, 0.0f);
	ASSERT_TRUE(renderer.mesh_draw_instanced(handle, transform.data(), 1));
	std::vector<uint8_t> instanced_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(instanced_pixels.data(), static_cast<uint32_t>(instanced_pixels.size())));

	//两帧的纹理色面积一致
	EXPECT_EQ(white_count(draw_pixels), white_count(instanced_pixels)) << "实例数为 1 与逐次绘制的覆盖面不一致";
	//两帧的中心像素逐通道一致（容差内）
	std::array<uint8_t, 4> draw_center = pixel_get(draw_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	std::array<uint8_t, 4> instanced_center = pixel_get(instanced_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	for (std::size_t index = 0; index < 4; ++index)
	{
		//两路径同一像素的通道差
		int delta = static_cast<int>(draw_center[index]) - static_cast<int>(instanced_center[index]);
		EXPECT_LE(delta < 0 ? -delta : delta, 4) << "实例数为 1 与逐次绘制的中心像素不一致："
			<< pixel_text(draw_center) << " vs " << pixel_text(instanced_center);
	}

	renderer.mesh_unload(handle);
}
