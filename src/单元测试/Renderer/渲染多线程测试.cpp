/*
渲染多线程测试（第一百七十七轮；L2 隐藏窗口 + 像素回读）
覆盖：未初始化守卫、默认单线程口径、配置 / 运行时切换与钳制、
      并行剔除与单线程的统计与画面一致、并行派发计数、实例容量钳制后的大批量登记。
断言口径：并行只影响耗时，不影响结果，故「线程数 0」与「线程数 N」的统计与像素必须一致；
      像素比对用通道容差（GPU 光栅化在同一指令流下应逐字节相同，容差仅为吸收驱动取整差异）。
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
	//实例化用例所用小四边形半边长
	constexpr float QUAD_HALF = 0.25f;
	//大批量实例数（超过并行阈值 256，且超过差异扫描阈值 512）
	constexpr uint32_t INSTANCE_COUNT = 1000;
	//实例横向铺开的左右边界（远超水平视野半宽 ≈ 3.08，故约六成实例被剔除）
	constexpr float INSTANCE_SPAN = 8.0f;
	//像素通道容差
	constexpr int PIXEL_TOLERANCE = 2;
	//纹理色判定阈值（内置默认白纹理）
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

	//构造中心小四边形（两个三角形；UV 满铺）
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

	/*
	构造横向铺开的大批实例矩阵
	横向均匀铺满 [−INSTANCE_SPAN, INSTANCE_SPAN]，故一部分落在视锥内、一部分被剔除；
	顺序固定，保证两个渲染器拿到逐字节相同的输入。
	*/
	std::vector<std::array<float, 16>> instance_matrices(uint32_t count)
	{
		std::vector<std::array<float, 16>> matrices(count);
		for (uint32_t index = 0; index < count; ++index)
		{
			//归一化横向位置（0..1 → −span..span）
			const float ratio = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.5f;
			matrices[index] = translation(-INSTANCE_SPAN + 2.0f * INSTANCE_SPAN * ratio, 0.0f, 0.0f);
		}
		return matrices;
	}

	//统计纹理色像素数量（三通道均接近满值且不透明）
	uint32_t white_count(const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			//三通道最小值
			uint8_t lowest = pixels[offset];
			if (pixels[offset + 1] < lowest)
				lowest = pixels[offset + 1];
			if (pixels[offset + 2] < lowest)
				lowest = pixels[offset + 2];
			//命中即计数
			if (lowest >= TEXTURE_WHITE_MIN && pixels[offset + 3] >= TEXTURE_WHITE_MIN)
				++count;
		}
		return count;
	}

	//两帧像素的最大通道差（诊断用）
	int max_channel_delta(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		int maximum = 0;
		const std::size_t shared = left.size() < right.size() ? left.size() : right.size();
		for (std::size_t index = 0; index < shared; ++index)
		{
			//本通道差
			int delta = static_cast<int>(left[index]) - static_cast<int>(right[index]);
			if (delta < 0)
				delta = -delta;
			if (delta > maximum)
				maximum = delta;
		}
		return maximum;
	}
}

//渲染多线程测试夹具
class Renderer_Thread_Test : public ::testing::Test
{
};

//未初始化时线程数查询与设置均安全
TEST_F(Renderer_Thread_Test, 未初始化线程数查询与设置安全)
{
	engine::Renderer renderer;
	//未初始化查询返回 0
	EXPECT_EQ(renderer.render_threads(), 0u);
	//未初始化设置静默忽略
	renderer.render_threads_set(4);
	EXPECT_EQ(renderer.render_threads(), 0u);
}

//默认配置为单线程且不派发并行任务
TEST_F(Renderer_Thread_Test, 默认单线程且无并行派发)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	EXPECT_EQ(renderer.render_threads(), 0u);
	//统计口径与门面一致
	engine::Frame_Stats stats = renderer.frame_stats_get();
	EXPECT_EQ(stats.render_threads, 0u);
	EXPECT_EQ(stats.parallel_dispatches, 0u);
	//关闭后仍为单线程
	renderer.shutdown();
	EXPECT_EQ(renderer.render_threads(), 0u);
}

//配置线程数在初始化时生效，运行时切换与钳制有效
TEST_F(Renderer_Thread_Test, 配置线程数生效与运行时切换)
{
	engine::Renderer_Config config = hidden_config();
	config.render_threads = 2;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	EXPECT_EQ(renderer.render_threads(), 2u);
	EXPECT_EQ(renderer.frame_stats_get().render_threads, 2u);

	//运行时上调
	renderer.render_threads_set(4);
	EXPECT_EQ(renderer.render_threads(), 4u);
	//超大值钳到上限 64
	renderer.render_threads_set(1000);
	EXPECT_EQ(renderer.render_threads(), 64u);
	//回落到单线程
	renderer.render_threads_set(0);
	EXPECT_EQ(renderer.render_threads(), 0u);
}

/*
并行剔除与单线程结果一致（第一百七十七轮核心验收）
同一批 1000 个实例矩阵分别交给「线程数 0」与「线程数 4」的渲染器：
剔除数 / 绘制实例数 / 纹理色面积 / 像素内容必须一致，且并行侧确实派发过任务。
*/
TEST_F(Renderer_Thread_Test, 并行剔除与单线程结果一致)
{
	//两个渲染器：单线程与四线程（容量同为 1024，足以容纳 1000 个实例）
	engine::Renderer_Config single_config = hidden_config();
	single_config.instance_capacity = 1024;
	engine::Renderer single;
	ASSERT_TRUE(single.init(single_config));
	if (!single.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Renderer_Config parallel_config = single_config;
	parallel_config.render_threads = 4;
	engine::Renderer parallel;
	ASSERT_TRUE(parallel.init(parallel_config));

	//两渲染器使用同一批实例矩阵（顺序固定）
	std::vector<std::array<float, 16>> matrices = instance_matrices(INSTANCE_COUNT);

	//单线程帧
	single.texture_bind_set(0);
	engine::Mesh_Handle single_mesh = 0;
	ASSERT_TRUE(single.mesh_load(single_mesh, quad_data(QUAD_HALF)));
	ASSERT_TRUE(single.mesh_draw_instanced(single_mesh, matrices[0].data(), INSTANCE_COUNT));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(single.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));
	const engine::Frame_Stats single_stats = single.frame_stats_get();

	//四线程帧
	parallel.texture_bind_set(0);
	engine::Mesh_Handle parallel_mesh = 0;
	ASSERT_TRUE(parallel.mesh_load(parallel_mesh, quad_data(QUAD_HALF)));
	ASSERT_TRUE(parallel.mesh_draw_instanced(parallel_mesh, matrices[0].data(), INSTANCE_COUNT));
	std::vector<uint8_t> parallel_pixels(pixel_bytes());
	ASSERT_TRUE(parallel.frame_render(parallel_pixels.data(), static_cast<uint32_t>(parallel_pixels.size())));
	const engine::Frame_Stats parallel_stats = parallel.frame_stats_get();

	//① 两侧都真的发生了剔除与绘制（否则一致是平凡成立）
	EXPECT_GT(single_stats.culled_instances, 0u) << "大批实例未被剔除：视锥判定异常";
	EXPECT_GT(single_stats.drawn_instances, 0u) << "大批实例全部被剔除：视锥判定异常";

	//② 剔除 / 绘制实例数逐值一致
	EXPECT_EQ(parallel_stats.culled_instances, single_stats.culled_instances);
	EXPECT_EQ(parallel_stats.drawn_instances, single_stats.drawn_instances);

	//③ 像素内容一致（容差内）
	EXPECT_EQ(white_count(parallel_pixels), white_count(single_pixels)) << "并行与单线程的纹理色面积不一致";
	EXPECT_LE(max_channel_delta(parallel_pixels, single_pixels), PIXEL_TOLERANCE)
		<< "并行与单线程的像素内容不一致";

	//④ 并行侧确实派发过任务，单线程侧一次也没有
	EXPECT_GT(parallel_stats.parallel_dispatches, 0u) << "四线程未派发任何并行任务";
	EXPECT_EQ(single_stats.parallel_dispatches, 0u) << "单线程不应派发并行任务";

	single.mesh_unload(single_mesh);
	parallel.mesh_unload(parallel_mesh);
}

/*
实例容量钳制与大批量登记
配置 8 会被钳到 16：一次登记 16 个实例成功、17 个被整体拒绝（不留半个实例化绘制）。
*/
TEST_F(Renderer_Thread_Test, 实例容量钳制与大批量登记)
{
	engine::Renderer_Config config = hidden_config();
	config.instance_capacity = 8;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data(QUAD_HALF)));
	std::vector<std::array<float, 16>> matrices = instance_matrices(17);

	//关闭剔除，使实例数严格按输入计（否则视锥外实例先被剔除，容量判定不可测）
	renderer.cull_enabled_set(false);

	//17 个实例超出钳制后的容量 16，被整批拒绝
	EXPECT_FALSE(renderer.mesh_draw_instanced(handle, matrices[0].data(), 17));
	//16 个实例恰好填满容量
	EXPECT_TRUE(renderer.mesh_draw_instanced(handle, matrices[0].data(), 16));

	renderer.mesh_unload(handle);
}
