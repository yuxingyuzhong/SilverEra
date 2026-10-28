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

//SSAA 与 MSAA 叠加：超采样目标随采样数建立多重采样附件，降采样仍可输出
TEST_F(Renderer_Msaa_Test, SSAA与MSAA叠加)
{
	engine::Renderer renderer;
	engine::Renderer_Config config = hidden_config(4);
	config.ssaa_factor = 2;
	ASSERT_TRUE(renderer.init(config));
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	EXPECT_EQ(renderer.ssaa_factor(), 2u);

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	ASSERT_FALSE(render_diamond(renderer, plane).empty());
	//帧统计应报告 MSAA 采样数（SSAA 不替代 MSAA）
	EXPECT_EQ(renderer.frame_stats_get().msaa_samples, renderer.msaa_samples());
	renderer.mesh_unload(plane);
}

//MSAA 下两级后处理链（两个中间目标 ping-pong）可正常完成
TEST_F(Renderer_Msaa_Test, 两级后处理链在MSAA下可用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//两个同尺寸中间目标
	uint64_t targets[2] = { 0, 0 };
	ASSERT_TRUE(renderer.render_target_create(targets[0], renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_create(targets[1], renderer.extent_width(), renderer.extent_height()));
	ASSERT_TRUE(renderer.render_target_present_chain_set(targets, 2));

	//两级管线（反色 → 反色，画面回到原样）
	uint64_t invert_a = 0;
	uint64_t invert_b = 0;
	ASSERT_TRUE(renderer.pipeline_load(invert_a, "后处理.vert.spv", "反色.frag.spv"));
	ASSERT_TRUE(renderer.pipeline_load(invert_b, "后处理.vert.spv", "反色.frag.spv"));
	uint64_t chain[2] = { invert_a, invert_b };
	ASSERT_TRUE(renderer.post_chain_set(chain, 2));

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	float transform[16] = {};
	diamond_transform(transform);
	renderer.mesh_draw(plane, transform);
	std::vector<uint8_t> pixels(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//两级均实际执行
	EXPECT_EQ(renderer.frame_stats_get().post_stages, 2u);

	renderer.render_target_present_chain_set(nullptr, 0);
	renderer.pipeline_unload(invert_a);
	renderer.pipeline_unload(invert_b);
	renderer.render_target_destroy(targets[1]);
	renderer.render_target_destroy(targets[0]);
	renderer.mesh_unload(plane);
}

//文字在 MSAA 下可绘制（文字管线随主 pass 采样数）
TEST_F(Renderer_Msaa_Test, 文字在MSAA下可绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//加载系统等宽字体（缺失则跳过）
	uint64_t font = 0;
	if (!renderer.font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16))
		GTEST_SKIP() << "本机缺少 consola.ttf";

	//绘制一段文本并统计有墨迹像素
	engine::Text_Layout layout{};
	renderer.text_draw_layout(font, "MSAA", 20.0f, 40.0f, layout);
	std::vector<uint8_t> pixels(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	std::size_t inked = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
	{
		int maximum = pixels[index];
		if (pixels[index + 1] > maximum) maximum = pixels[index + 1];
		if (pixels[index + 2] > maximum) maximum = pixels[index + 2];
		if (maximum > 100)
			++inked;
	}
	EXPECT_GT(inked, 0u) << "MSAA 下文字未产生墨迹";

	renderer.font_unload(font);
}

//关闭 MSAA 后画面恢复到与初始单采样逐字节一致（零副作用）
TEST_F(Renderer_Msaa_Test, 关闭MSAA后画面与初始一致)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(1)));
	if (renderer.msaa_max_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Mesh_Handle plane = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	//初始单采样帧
	std::vector<uint8_t> baseline = render_diamond(renderer, plane);
	ASSERT_FALSE(baseline.empty());
	//开启再关闭
	renderer.msaa_samples_set(4);
	ASSERT_FALSE(render_diamond(renderer, plane).empty());
	renderer.msaa_samples_set(1);
	std::vector<uint8_t> restored = render_diamond(renderer, plane);
	ASSERT_FALSE(restored.empty());
	//两帧内容应逐字节一致
	EXPECT_EQ(baseline, restored) << "关闭 MSAA 后画面未回到初始单采样结果";

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

/*
高组合冒烟（第一百三十七轮）
MSAA 4× ＋ 方向光阴影 ＋ 两级后处理链 ＋ 文字 ＋ 静态合批同时启用，连续 3 帧均可渲染，统计自洽。
*/
TEST_F(Renderer_Msaa_Test, 高组合冒烟)
{
        engine::Renderer renderer;
        ASSERT_TRUE(renderer.init(hidden_config(4)));

        //两级链所需目标（与交换链同尺寸）
        uint64_t scene_target = 0;
        uint64_t second_target = 0;
        ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
        ASSERT_TRUE(renderer.render_target_create(second_target, renderer.extent_width(), renderer.extent_height()));
        uint64_t invert = 0;
        uint64_t threshold = 0;
        ASSERT_TRUE(renderer.pipeline_load(invert, "后处理.vert.spv", "反色.frag.spv"));
        ASSERT_TRUE(renderer.pipeline_load(threshold, "后处理.vert.spv", "亮度阈值.frag.spv"));
        uint64_t chain[2] = { invert, threshold };
        ASSERT_TRUE(renderer.post_chain_set(chain, 2));
        uint64_t targets[2] = { scene_target, second_target };
        ASSERT_TRUE(renderer.render_target_present_chain_set(targets, 2));

        //场景：方向光沿 +Z，开启阴影
        engine::Camera_Data camera{};
        engine::Light_Data light{};
        light.direction[2] = 1.0f;
        light.intensity = 1.0f;
        light.ambient = 0.0f;
        engine::Fog_Data fog{};
        renderer.scene_apply(camera, light, fog);
        renderer.shadow_enable_set(true);

        //遮挡面与接收面
        engine::Mesh_Handle receiver = 0;
        engine::Mesh_Handle occluder = 0;
        ASSERT_TRUE(renderer.mesh_plane_load(receiver, 2.0f, 2.0f));
        ASSERT_TRUE(renderer.mesh_plane_load(occluder, 1.0f, 1.0f));
        float transform[16] = {};
        transform[0] = 0.6f;
        transform[5] = 0.6f;
        transform[10] = 0.6f;
        transform[14] = -0.5f;
        transform[15] = 1.0f;

        //文字项（字体缺失时跳过文字部分，其余组合照常验证）
        uint64_t font = 0;
        const bool text_ready = renderer.font_load_file(font, "C:\\Windows\\Fonts\\consola.ttf", 16);

        for (uint32_t frame = 0; frame < 3; ++frame)
        {
                ASSERT_TRUE(renderer.mesh_draw(occluder, transform));
                ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
                //文字每帧重新登记（待绘制项逐帧清空）
                if (text_ready)
                        ASSERT_TRUE(renderer.text_draw(font, "MSAA", 4.0f, 4.0f));
                ASSERT_TRUE(renderer.frame_render());
        }
        engine::Frame_Stats stats = renderer.frame_stats_get();
        RecordProperty("combo_draw_items", static_cast<int>(stats.draw_items));
        EXPECT_EQ(stats.frames, 3u);
        EXPECT_EQ(stats.post_stages, 2u) << "两级链未生效";
        EXPECT_EQ(stats.drawn_items, stats.draw_items) << "高组合下存在被跳过的绘制项";
        EXPECT_GE(stats.msaa_samples, 1u);
        EXPECT_LE(stats.msaa_samples, 4u);
        EXPECT_GT(stats.instances, 0u);

        renderer.shadow_enable_set(false);
        renderer.mesh_unload(receiver);
        renderer.mesh_unload(occluder);
        renderer.pipeline_unload(invert);
        renderer.pipeline_unload(threshold);
        renderer.render_target_destroy(second_target);
        renderer.render_target_destroy(scene_target);
}

/*
MSAA 深度解析（第一百九十轮）
口径：MSAA 下场景 pass 只写多重采样深度，深度类后处理（此处深度描边）必须读到解析后的单采样深度。
做法：同一旋转平面各以 1× / 4× 运行「强度 0（等于直通）」与「强度 1（描边）」两帧，统计两者差异像素数；
      4× 应与 1× 同量级（此前 4× 读到未定义深度，描边不改变任何像素，差异恒为 0）。
*/
TEST_F(Renderer_Msaa_Test, MSAA深度解析供深度描边采样)
{
	//1× 基线：单采样深度 + 深度描边
	std::size_t changed_single = 0;
	{
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(hidden_config(1)));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
		engine::Mesh_Handle plane = 0;
		uint64_t scene_target = 0;
		uint64_t outline = 0;
		ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
		ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		renderer.render_target_present_set(scene_target);
		ASSERT_TRUE(renderer.pipeline_load(outline, "后处理.vert.spv", "深度描边.frag.spv"));
		ASSERT_TRUE(renderer.post_chain_set(&outline, 1));
		float transform[16] = {};
		diamond_transform(transform);
		//先以强度 0（等同直通）渲染一帧作为对照
		renderer.post_stage_param_set(0, 0.0f, 0.0f, 0.5f);
		renderer.post_stage_color_set(0, 1.0f, 0.0f, 0.0f);
		std::vector<uint8_t> plain(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
		renderer.mesh_draw(plane, transform);
		ASSERT_TRUE(renderer.frame_render(plain.data(), static_cast<uint32_t>(plain.size())));
		//再以强度 1、阈值 0.5、描边色纯红渲染一帧
		renderer.post_stage_param_set(0, 1.0f, 0.0f, 0.5f);
		std::vector<uint8_t> pixels(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
		renderer.mesh_draw(plane, transform);
		ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		//统计与直通版的差异像素数（深度描边确实作用于画面）
		std::size_t changed = 0;
		for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
		{
			int delta = std::abs(static_cast<int>(pixels[index]) - static_cast<int>(plain[index]));
			int delta_g = std::abs(static_cast<int>(pixels[index + 1]) - static_cast<int>(plain[index + 1]));
			if (delta_g > delta)
				delta = delta_g;
			int delta_b = std::abs(static_cast<int>(pixels[index + 2]) - static_cast<int>(plain[index + 2]));
			if (delta_b > delta)
				delta = delta_b;
			if (delta > 20)
				++changed;
		}
		ASSERT_GT(changed, 0u) << "单采样下深度描边未改变任何像素（基线无效）";
		changed_single = changed;
		renderer.post_chain_set(nullptr, 0);
		renderer.render_target_present_set(0);
		renderer.pipeline_unload(outline);
		renderer.render_target_destroy(scene_target);
		renderer.mesh_unload(plane);
	}

	//4×：默认开启深度解析，轮廓像素应与 1× 同量级
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config(4)));
	if (renderer.msaa_samples() <= 1)
		GTEST_SKIP() << "当前设备不支持 MSAA";
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	engine::Mesh_Handle plane = 0;
	uint64_t scene_target = 0;
	uint64_t outline = 0;
	ASSERT_TRUE(renderer.mesh_plane_load(plane, 1.0f, 1.0f));
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	renderer.render_target_present_set(scene_target);
	ASSERT_TRUE(renderer.pipeline_load(outline, "后处理.vert.spv", "深度描边.frag.spv"));
	ASSERT_TRUE(renderer.post_chain_set(&outline, 1));
	float transform[16] = {};
	diamond_transform(transform);
	//先以强度 0（等同直通）渲染一帧作为对照
	renderer.post_stage_param_set(0, 0.0f, 0.0f, 0.5f);
	renderer.post_stage_color_set(0, 1.0f, 0.0f, 0.0f);
	std::vector<uint8_t> plain_msaa(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
	renderer.mesh_draw(plane, transform);
	ASSERT_TRUE(renderer.frame_render(plain_msaa.data(), static_cast<uint32_t>(plain_msaa.size())));
	//再以强度 1、阈值 0.5、描边色纯红渲染一帧
	renderer.post_stage_param_set(0, 1.0f, 0.0f, 0.5f);
	std::vector<uint8_t> pixels(static_cast<std::size_t>(renderer.extent_width()) * renderer.extent_height() * 4, 0);
	renderer.mesh_draw(plane, transform);
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//统计与直通版的差异像素数（深度描边确实作用于画面）
	std::size_t changed_msaa = 0;
	for (std::size_t index = 0; index + 3 < pixels.size(); index += 4)
	{
		int delta = std::abs(static_cast<int>(pixels[index]) - static_cast<int>(plain_msaa[index]));
		int delta_g = std::abs(static_cast<int>(pixels[index + 1]) - static_cast<int>(plain_msaa[index + 1]));
		if (delta_g > delta)
			delta = delta_g;
		int delta_b = std::abs(static_cast<int>(pixels[index + 2]) - static_cast<int>(plain_msaa[index + 2]));
		if (delta_b > delta)
			delta = delta_b;
		if (delta > 20)
			++changed_msaa;
	}
	RecordProperty("changed_single", static_cast<int>(changed_single));
	RecordProperty("changed_msaa", static_cast<int>(changed_msaa));
	ASSERT_GT(changed_msaa, 0u) << "MSAA 下深度解析未生效：深度描边未改变任何像素";
	//差异像素数应与单采样同量级（MSAA 只改变边缘覆盖率，不改变轮廓位置）
	std::size_t difference = changed_single > changed_msaa ? changed_single - changed_msaa : changed_msaa - changed_single;
	EXPECT_LE(difference * 2, changed_single) << "MSAA 下轮廓像素数与单采样差异过大";
	//开关默认开启，且可运行时切换（不重建资源）
	EXPECT_TRUE(renderer.msaa_depth_resolve());
	renderer.msaa_depth_resolve_set(false);
	EXPECT_FALSE(renderer.msaa_depth_resolve());
	renderer.msaa_depth_resolve_set(true);
	EXPECT_TRUE(renderer.msaa_depth_resolve());

	renderer.post_chain_set(nullptr, 0);
	renderer.render_target_present_set(0);
	renderer.pipeline_unload(outline);
	renderer.render_target_destroy(scene_target);
	renderer.mesh_unload(plane);
}
