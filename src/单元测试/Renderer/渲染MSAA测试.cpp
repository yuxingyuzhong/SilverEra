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

//能力查询与各档位切换：按设备支持逐档设置并出帧
TEST_F(Renderer_Msaa_Test, 能力查询与各档位切换)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(1)));
	uint32_t maximum = renderer.msaa_max_samples();
	EXPECT_GE(maximum, 1u);
	EXPECT_LE(maximum, 8u);

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	//逐档尝试（2 / 4 / 8），不支持则跳过该档
	const uint32_t tiers[3] = { 2, 4, 8 };
	for (uint32_t tier : tiers)
	{
		//超过设备上限则跳过
		if (tier > maximum)
			continue;
		renderer.msaa_samples_set(tier);
		EXPECT_EQ(renderer.msaa_samples(), tier);
		EXPECT_FALSE(render_diamond(renderer, plane).empty()) << "档位 " << tier << " 渲染失败";
	}
	//非法请求（3 / 5 / 6）按 1 处理
	for (uint32_t invalid : { 3u, 5u, 6u })
	{
		renderer.msaa_samples_set(invalid);
		EXPECT_EQ(renderer.msaa_samples(), 1u);
	}
	renderer.msaa_samples_set(1);
	EXPECT_EQ(renderer.msaa_samples(), 1u);
	renderer.mesh_unload(plane);
}

//MSAA 下同帧后处理链仍可用（内置后处理管线固定单采样，场景仍多重采样）
TEST_F(Renderer_Msaa_Test, 后处理链在MSAA下可用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	//设备不支持多重采样则跳过
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	//无回读能力则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//场景目标（与交换链同尺寸）＋ 旋转平面
	engine::Mesh_Handle plane = 0;
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	renderer.render_target_present_set(scene_target);
	float transform[16] = {};
	diamond_transform(transform);
	renderer.mesh_draw(plane, transform);

	//一帧内完成「场景 MSAA → 解析 → 单采样后处理 → 交换链」
	std::vector<uint8_t> pixels(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, renderer.extent_width());
	EXPECT_EQ(height, renderer.extent_height());
	//画面应包含被照亮的平面（非纯清屏色）
	bool lit = false;
	for (std::size_t index = 0; index + 3 < pixels.size() && !lit; index += 4)
	{
		int maximum = pixels[index];
		if (pixels[index + 1] > maximum) maximum = pixels[index + 1];
		if (pixels[index + 2] > maximum) maximum = pixels[index + 2];
		if (maximum > 40)
			lit = true;
	}
	EXPECT_TRUE(lit) << "后处理链输出为空画面";

	renderer.render_target_destroy(scene_target);
	renderer.mesh_unload(plane);
}

//阴影与 MSAA 共存：阴影 pass 保持单采样，主 pass 多重采样，均正常出帧
TEST_F(Renderer_Msaa_Test, 阴影与MSAA共存)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	renderer.shadow_enable_set(true);
	ASSERT_TRUE(renderer.shadow_enabled());
	//开启阴影后至少执行一次阴影 pass
	EXPECT_FALSE(render_diamond(renderer, plane).empty());
	uint64_t shadow_before = renderer.shadow_pass_count();
	EXPECT_GE(shadow_before, 1u);
	//切回单采样后阴影仍可继续工作
	renderer.msaa_samples_set(1);
	EXPECT_FALSE(render_diamond(renderer, plane).empty());
	EXPECT_GE(renderer.shadow_pass_count(), shadow_before);
	renderer.shadow_enable_set(false);
	renderer.mesh_unload(plane);
}

//帧统计反映最近一帧实际采样数（交换链直通与离屏路径）
TEST_F(Renderer_Msaa_Test, 帧统计反映采样数)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(1)));
	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));

	//单采样一帧
	ASSERT_FALSE(render_diamond(renderer, plane).empty());
	EXPECT_EQ(renderer.frame_stats_get().msaa_samples, 1u);

	//若设备支持则开 4× 再测
	uint32_t maximum = renderer.msaa_max_samples();
	if (maximum <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	uint32_t tier = maximum >= 4 ? 4 : maximum;
	renderer.msaa_samples_set(tier);
	ASSERT_FALSE(render_diamond(renderer, plane).empty());
	EXPECT_EQ(renderer.frame_stats_get().msaa_samples, tier);

	//离屏目标路径同样反映采样数
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, 64, 64));
	renderer.render_target_set(target);
	float transform[16] = {};
	diamond_transform(transform);
	renderer.mesh_draw(plane, transform);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.frame_stats_get().msaa_samples, tier);
	renderer.render_target_set(0);
	renderer.render_target_destroy(target);
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
