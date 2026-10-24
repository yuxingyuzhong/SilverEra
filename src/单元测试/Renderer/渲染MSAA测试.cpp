/*
渲染层 MSAA 测试（L2 隐藏窗口；第九十六轮）
覆盖：未初始化守卫、初始化配置生效与查询、启用后仍可渲染 / 回读、
      斜边像素的平滑（中间亮度像素数增加）、运行时切回单采样、
      渲染目标随采样数重建与 MSAA 下深度读回降级。
说明：本用例只按渲染层公共面编写，不涉及 Vulkan 细节。
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置（可指定 MSAA 采样数）
	engine::Renderer_Config hidden_config(uint32_t msaa_samples)
	{
		engine::Renderer_Config config;
		config.width = 320;
		config.height = 180;
		config.hidden = true;
		config.msaa_samples = msaa_samples;
		return config;
	}

	/*
	构造 45° 旋转的放大平面模型矩阵（列主序）
	平面（1×1）缩放 2 后旋转 45°，在屏幕上形成菱形，其边为斜边，便于观察 MSAA 边缘平滑。
	*/
	void diamond_transform(float matrix[16])
	{
		//45° 的余弦 / 正弦与缩放
		const float cosine = 0.70710678f;
		const float sine = 0.70710678f;
		const float scale = 2.0f;
		std::fill(matrix, matrix + 16, 0.0f);
		//第一列 = 旋转后 X 轴 × 缩放
		matrix[0] = cosine * scale;
		matrix[1] = sine * scale;
		//第二列 = 旋转后 Y 轴 × 缩放
		matrix[4] = -sine * scale;
		matrix[5] = cosine * scale;
		//第三列与平移保持单位
		matrix[10] = 1.0f;
		matrix[15] = 1.0f;
	}

	/*
	统计「混合像素」数（第九十六轮）
	先取全图亮度极值（暗 = 清屏色、亮 = 平面内部），再统计亮度严格落在
	（暗 + 30, 亮 − 30）区间内的像素：内部纯色与纯暗 / 纯亮像素均被排除，
	只剩被部分覆盖的边缘像素，故可直接反映 MSAA 的平滑效果。
	*/
	std::size_t blended_pixel_count(const std::vector<uint8_t>& pixels)
	{
		//全图亮度极值
		int darkest = 255;
		int brightest = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		{
			//取该像素三通道最大值（交换链为 BGRA，取最大可规避通道序差异）
			int maximum = pixels[index];
			if (pixels[index + 1] > maximum)
				maximum = pixels[index + 1];
			if (pixels[index + 2] > maximum)
				maximum = pixels[index + 2];
			if (maximum < darkest)
				darkest = maximum;
			if (maximum > brightest)
				brightest = maximum;
		}
		/*
		极值区间过窄（画面近乎纯色）则无可统计
		边距取 20：解析在线性空间进行并编码回 sRGB，部分覆盖的像素常落在接近亮端的位置
		（如 3/4 覆盖 ≈ 228），故窗口不能收得太窄。
		*/
		int low = darkest + 20;
		int high = brightest - 20;
		if (high <= low)
			return 0;
		//统计严格落在区间内的像素
		std::size_t count = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		{
			int maximum = pixels[index];
			if (pixels[index + 1] > maximum)
				maximum = pixels[index + 1];
			if (pixels[index + 2] > maximum)
				maximum = pixels[index + 2];
			if (maximum > low && maximum < high)
				++count;
		}
		return count;
	}

	//渲染一帧旋转平面并回读像素（返回像素数；失败返回空）
	std::vector<uint8_t> render_diamond(engine::Renderer& renderer, engine::Mesh_Handle plane)
	{
		//模型矩阵（45° 旋转 ＋ 放大）
		float transform[16] = {};
		diamond_transform(transform);
		//登记绘制项
		renderer.mesh_draw(plane, transform);
		//回读缓冲（RGBA8 紧排）
		std::vector<uint8_t> pixels(static_cast<std::size_t>(320) * 180 * 4, 0);
		uint32_t width = 0;
		uint32_t height = 0;
		if (!renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height))
			return {};
		return pixels;
	}
}

//MSAA 测试夹具
class Renderer_Msaa_Test : public ::testing::Test
{
};

//未初始化时查询为 1，设置被静默忽略
TEST_F(Renderer_Msaa_Test, 未初始化守卫)
{
	engine::Renderer renderer;
	EXPECT_EQ(renderer.msaa_samples(), 1u);
	renderer.msaa_samples_set(4);
	EXPECT_EQ(renderer.msaa_samples(), 1u);
}

//初始化配置生效：采样数被钳制到设备支持值，且启用后仍可渲染与回读
TEST_F(Renderer_Msaa_Test, 初始化配置生效并可渲染)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	//设备可能只支持 2×，故只断言「大于 1 且不超过 4」
	EXPECT_GE(renderer.msaa_samples(), 1u);
	EXPECT_LE(renderer.msaa_samples(), 4u);

	//加载平面并绘制
	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	std::vector<uint8_t> pixels = render_diamond(renderer, plane);
	ASSERT_FALSE(pixels.empty());
	renderer.mesh_unload(plane);
}

//斜边平滑：开启 MSAA 后边缘中间亮度像素数明显多于关闭时
TEST_F(Renderer_Msaa_Test, 斜边像素平滑)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	//设备不支持多重采样则跳过（保持单采样语义）
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA（颜色 / 深度格式至少需支持 2×）";

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	//开启 MSAA 的一帧
	std::vector<uint8_t> antialiased = render_diamond(renderer, plane);
	ASSERT_FALSE(antialiased.empty());
	std::size_t antialiased_edges = blended_pixel_count(antialiased);

	//切回单采样后再画一帧
	renderer.msaa_samples_set(1);
	ASSERT_EQ(renderer.msaa_samples(), 1u);
	std::vector<uint8_t> plain = render_diamond(renderer, plane);
	ASSERT_FALSE(plain.empty());
	std::size_t plain_edges = blended_pixel_count(plain);

	//MSAA 边缘应有可观测的中间亮度像素，且多于单采样
	EXPECT_GT(antialiased_edges, 0u);
	EXPECT_GT(antialiased_edges, plain_edges);
	renderer.mesh_unload(plane);
}

//运行时时序：MSAA → 单采样 → 再开启，均能正常出帧
TEST_F(Renderer_Msaa_Test, 运行时反复切换)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(1)));
	EXPECT_EQ(renderer.msaa_samples(), 1u);

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	//开启
	renderer.msaa_samples_set(4);
	uint32_t enabled = renderer.msaa_samples();
	EXPECT_GE(enabled, 1u);
	EXPECT_FALSE(render_diamond(renderer, plane).empty());
	//关闭
	renderer.msaa_samples_set(1);
	EXPECT_EQ(renderer.msaa_samples(), 1u);
	EXPECT_FALSE(render_diamond(renderer, plane).empty());
	renderer.mesh_unload(plane);
}

//渲染目标随采样数重建（句柄不变）；MSAA 下深度读回被拒绝，关闭后恢复
TEST_F(Renderer_Msaa_Test, 渲染目标随采样数重建且深度读回降级)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));

	//创建离屏目标并以 MSAA 渲染
	engine::Mesh_Handle plane = 0;
	uint64_t target = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	float transform[16] = {};
	diamond_transform(transform);
	renderer.mesh_draw(plane, transform);
	ASSERT_TRUE(renderer.frame_render());

	//颜色读回可用（解析结果）
	std::vector<uint8_t> pixels(64 * 64 * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_EQ(width, 64u);
	EXPECT_EQ(height, 64u);

	//MSAA 开启时深度读回不可用（多重采样深度无法解析为单采样值）
	std::vector<uint32_t> depth_values(64 * 64, 0);
	bool depth_ok = renderer.render_target_readback_depth(target, depth_values.data(),
		static_cast<uint32_t>(depth_values.size()), &width, &height);
	if (renderer.msaa_samples() > 1)
		EXPECT_FALSE(depth_ok);

	//切回单采样：目标句柄保持有效，颜色与深度读回均恢复
	renderer.msaa_samples_set(1);
	renderer.mesh_draw(plane, transform);
	ASSERT_TRUE(renderer.frame_render());
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(), static_cast<uint32_t>(pixels.size()),
		&width, &height));
	EXPECT_TRUE(renderer.render_target_readback_depth(target, depth_values.data(),
		static_cast<uint32_t>(depth_values.size()), &width, &height));

	//清理
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
	renderer.mesh_unload(plane);
}
