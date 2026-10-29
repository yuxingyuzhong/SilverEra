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

/*
场景 pass 并行录制与单线程一致（第一百九十七轮）
口径：同帧后处理链路径（场景目标 → 单级后处理 → 交换链），场景登记 64 个绘制项
      （≥ 32 项才启用并行录制；四线程下列出 4 段）；单线程与四线程的像素内容与绘制统计必须一致，
      且四线程确实派发过并行任务。
*/
TEST_F(Renderer_Thread_Test, 并行录制场景通道与单线程一致)
{
	//单次渲染：按给定线程数跑「场景目标 ＋ 单级后处理链」一帧并读回
	auto render_chain = [](uint32_t threads, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats) {
		engine::Renderer_Config config = hidden_config();
		config.render_threads = threads;
		engine::Renderer renderer;
		if (!renderer.init(config))
			return false;
		if (!renderer.readback_supported())
			return false;
		//关闭静态合批：本用例要求 64 个独立绘制项（合批会把同管线的相邻项并成 1 项）
		renderer.static_batching_set(false);
		uint64_t scene_target = 0;
		if (!renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()))
			return false;
		uint64_t pass = 0;
		if (!renderer.pipeline_load(pass, "后处理.vert.spv", "后处理.frag.spv"))
			return false;
		if (!renderer.post_chain_set(&pass, 1))
			return false;
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle quad = 0;
		if (!renderer.mesh_load(quad, quad_data(QUAD_HALF)))
			return false;
		//64 个绘制项：8×8 网格排布（每项一个独立绘制项，便于切成 4 段）
		for (uint32_t row = 0; row < 8; ++row)
		{
			for (uint32_t column = 0; column < 8; ++column)
			{
				const float x = -0.875f + 0.25f * static_cast<float>(column);
				const float y = -0.875f + 0.25f * static_cast<float>(row);
				const float matrix[16] = {
					0.09f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.09f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.09f, 0.0f,
					x, y, 0.0f, 1.0f,
				};
				if (!renderer.mesh_draw(quad, matrix))
					return false;
			}
		}
		bool ok = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		stats = renderer.frame_stats_get();
		renderer.mesh_unload(quad);
		renderer.post_chain_set(nullptr, 0);
		renderer.render_target_present_set(0);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return ok;
	};

	//单线程基线
	std::vector<uint8_t> single_pixels(pixel_bytes());
	engine::Frame_Stats single_stats{};
	ASSERT_TRUE(render_chain(0, single_pixels, single_stats));
	ASSERT_EQ(single_stats.last_draw_items, 64u) << "单线程侧未登记 64 个绘制项";

	//四线程（并行录制）
	std::vector<uint8_t> parallel_pixels(pixel_bytes());
	engine::Frame_Stats parallel_stats{};
	ASSERT_TRUE(render_chain(4, parallel_pixels, parallel_stats));

	//① 绘制项数与像素内容一致（并行只改变录制位置，不改变 GPU 执行顺序）
	EXPECT_EQ(parallel_stats.last_draw_items, single_stats.last_draw_items);
	EXPECT_EQ(parallel_stats.last_instances, single_stats.last_instances);
	EXPECT_LE(max_channel_delta(parallel_pixels, single_pixels), PIXEL_TOLERANCE)
		<< "并行录制的画面与单线程不一致";

	//② 四线程确实派发过并行任务（含剔除 / 差异扫描 / 分段录制）
	EXPECT_GT(parallel_stats.parallel_dispatches, 0u) << "四线程未派发任何并行任务";
	EXPECT_EQ(single_stats.parallel_dispatches, 0u);
	//③ 并行段数观测（第一百九十八轮）：四线程应报告 > 0 段，单线程恒 0
	EXPECT_GT(parallel_stats.parallel_segments, 0u) << "四线程未报告并行录制段数";
	EXPECT_EQ(single_stats.parallel_segments, 0u) << "单线程不应报告并行段";
}

/*
交换链直通路径的并行录制（第一百九十九轮）
口径：无后处理链（直接渲染到交换链）、64 个独立绘制项（关闭合批）；单线程与四线程的像素内容、
      绘制项 / 实例数与并行段数统计必须一致（四线程报告 > 0 段）。
*/
TEST_F(Renderer_Thread_Test, 并行录制交换链直通路径与单线程一致)
{
	//单次渲染：按给定线程数直接渲染到交换链并读回
	auto render_direct = [](uint32_t threads, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats) {
		engine::Renderer_Config config = hidden_config();
		config.render_threads = threads;
		engine::Renderer renderer;
		if (!renderer.init(config))
			return false;
		if (!renderer.readback_supported())
			return false;
		//关闭静态合批：保持 64 个独立绘制项（≥ 32 才启用并行录制）
		renderer.static_batching_set(false);
		engine::Mesh_Handle quad = 0;
		if (!renderer.mesh_load(quad, quad_data(QUAD_HALF)))
			return false;
		for (uint32_t row = 0; row < 8; ++row)
		{
			for (uint32_t column = 0; column < 8; ++column)
			{
				const float x = -0.875f + 0.25f * static_cast<float>(column);
				const float y = -0.875f + 0.25f * static_cast<float>(row);
				const float matrix[16] = {
					0.09f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.09f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.09f, 0.0f,
					x, y, 0.0f, 1.0f,
				};
				if (!renderer.mesh_draw(quad, matrix))
					return false;
			}
		}
		bool ok = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		stats = renderer.frame_stats_get();
		renderer.mesh_unload(quad);
		return ok;
	};

	//单线程基线
	std::vector<uint8_t> single_pixels(pixel_bytes());
	engine::Frame_Stats single_stats{};
	ASSERT_TRUE(render_direct(0, single_pixels, single_stats));
	ASSERT_EQ(single_stats.last_draw_items, 64u) << "单线程侧未登记 64 个绘制项";
	EXPECT_EQ(single_stats.parallel_segments, 0u);

	//四线程（并行录制）
	std::vector<uint8_t> parallel_pixels(pixel_bytes());
	engine::Frame_Stats parallel_stats{};
	ASSERT_TRUE(render_direct(4, parallel_pixels, parallel_stats));

	EXPECT_EQ(parallel_stats.last_draw_items, single_stats.last_draw_items);
	EXPECT_EQ(parallel_stats.last_instances, single_stats.last_instances);
	EXPECT_LE(max_channel_delta(parallel_pixels, single_pixels), PIXEL_TOLERANCE)
		<< "直通路径并行录制的画面与单线程不一致";
	EXPECT_GT(parallel_stats.parallel_segments, 0u) << "直通路径未启用并行录制";
}

/*
离屏目标路径的并行录制（第二百轮）
口径：render_target_set 到离屏目标（不呈现、回读目标）、64 个独立绘制项（关闭合批）；
      单线程与四线程的像素内容、绘制统计与并行段数必须一致（四线程报告 > 0 段）。
*/
TEST_F(Renderer_Thread_Test, 并行录制离屏目标路径与单线程一致)
{
	//单次渲染：按给定线程数渲染到离屏目标并读回
	auto render_offscreen = [](uint32_t threads, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats) {
		engine::Renderer_Config config = hidden_config();
		config.render_threads = threads;
		engine::Renderer renderer;
		if (!renderer.init(config))
			return false;
		if (!renderer.readback_supported())
			return false;
		//关闭静态合批：保持 64 个独立绘制项（≥ 32 才启用并行录制）
		renderer.static_batching_set(false);
		uint64_t target = 0;
		if (!renderer.render_target_create(target, renderer.extent_width(), renderer.extent_height()))
			return false;
		renderer.render_target_set(target);
		engine::Mesh_Handle quad = 0;
		if (!renderer.mesh_load(quad, quad_data(QUAD_HALF)))
			return false;
		for (uint32_t row = 0; row < 8; ++row)
		{
			for (uint32_t column = 0; column < 8; ++column)
			{
				const float x = -0.875f + 0.25f * static_cast<float>(column);
				const float y = -0.875f + 0.25f * static_cast<float>(row);
				const float matrix[16] = {
					0.09f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.09f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.09f, 0.0f,
					x, y, 0.0f, 1.0f,
				};
				if (!renderer.mesh_draw(quad, matrix))
					return false;
			}
		}
		bool ok = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		stats = renderer.frame_stats_get();
		renderer.render_target_set(0);
		renderer.mesh_unload(quad);
		renderer.render_target_destroy(target);
		return ok;
	};

	//单线程基线
	std::vector<uint8_t> single_pixels(pixel_bytes());
	engine::Frame_Stats single_stats{};
	ASSERT_TRUE(render_offscreen(0, single_pixels, single_stats));
	ASSERT_EQ(single_stats.last_draw_items, 64u) << "单线程侧未登记 64 个绘制项";
	EXPECT_EQ(single_stats.parallel_segments, 0u);

	//四线程（并行录制）
	std::vector<uint8_t> parallel_pixels(pixel_bytes());
	engine::Frame_Stats parallel_stats{};
	ASSERT_TRUE(render_offscreen(4, parallel_pixels, parallel_stats));

	EXPECT_EQ(parallel_stats.last_draw_items, single_stats.last_draw_items);
	EXPECT_EQ(parallel_stats.last_instances, single_stats.last_instances);
	EXPECT_LE(max_channel_delta(parallel_pixels, single_pixels), PIXEL_TOLERANCE)
		<< "离屏路径并行录制的画面与单线程不一致";
	EXPECT_GT(parallel_stats.parallel_segments, 0u) << "离屏路径未启用并行录制";
}

/*
MSAA 与并行录制的组合（第二百零一轮）
口径：4× MSAA ＋ 单级「深度描边」链 ＋ 64 个独立绘制项；单线程与四线程逐像素一致，
      且四线程报告并行段 > 0——验证「多重采样解析只在末段执行」在并行录制下正确
      （若中间段也解析，末段的解析目标会被中间结果覆盖，画面与单线程不一致）。
*/
TEST_F(Renderer_Thread_Test, MSAA下并行录制与单线程一致)
{
	//设备能力预检
	{
		engine::Renderer probe;
		engine::Renderer_Config probe_config = hidden_config();
		probe_config.msaa_samples = 4;
		ASSERT_TRUE(probe.init(probe_config));
		if (probe.msaa_samples() <= 1)
			GTEST_SKIP() << "当前设备不支持 MSAA";
		if (!probe.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	}

	//单次渲染：4× MSAA ＋ 深度描边链 ＋ 64 项（关闭合批）
	auto render_msaa = [](uint32_t threads, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats) {
		engine::Renderer_Config config = hidden_config();
		config.render_threads = threads;
		config.msaa_samples = 4;
		engine::Renderer renderer;
		if (!renderer.init(config))
			return false;
		if (renderer.msaa_samples() <= 1)
			return false;
		renderer.static_batching_set(false);
		uint64_t scene_target = 0;
		if (!renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()))
			return false;
		uint64_t pass = 0;
		if (!renderer.pipeline_load(pass, "后处理.vert.spv", "深度描边.frag.spv"))
			return false;
		if (!renderer.post_chain_set(&pass, 1))
			return false;
		renderer.post_stage_param_set(0, 1.0f, 0.0f, 0.5f);
		renderer.post_stage_color_set(0, 1.0f, 0.0f, 0.0f);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle quad = 0;
		if (!renderer.mesh_load(quad, quad_data(QUAD_HALF)))
			return false;
		for (uint32_t row = 0; row < 8; ++row)
		{
			for (uint32_t column = 0; column < 8; ++column)
			{
				const float x = -0.875f + 0.25f * static_cast<float>(column);
				const float y = -0.875f + 0.25f * static_cast<float>(row);
				const float matrix[16] = {
					0.09f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.09f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.09f, 0.0f,
					x, y, 0.0f, 1.0f,
				};
				if (!renderer.mesh_draw(quad, matrix))
					return false;
			}
		}
		bool ok = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		stats = renderer.frame_stats_get();
		renderer.mesh_unload(quad);
		renderer.post_chain_set(nullptr, 0);
		renderer.render_target_present_set(0);
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return ok;
	};

	//单线程基线（4×）
	std::vector<uint8_t> single_pixels(pixel_bytes());
	engine::Frame_Stats single_stats{};
	ASSERT_TRUE(render_msaa(0, single_pixels, single_stats));
	ASSERT_EQ(single_stats.last_draw_items, 64u);
	EXPECT_EQ(single_stats.msaa_samples, 4u);
	EXPECT_EQ(single_stats.parallel_segments, 0u);

	//四线程（4× + 并行录制）
	std::vector<uint8_t> parallel_pixels(pixel_bytes());
	engine::Frame_Stats parallel_stats{};
	ASSERT_TRUE(render_msaa(4, parallel_pixels, parallel_stats));

	EXPECT_EQ(parallel_stats.last_draw_items, single_stats.last_draw_items);
	EXPECT_EQ(parallel_stats.last_instances, single_stats.last_instances);
	EXPECT_EQ(parallel_stats.msaa_samples, 4u);
	EXPECT_LE(max_channel_delta(parallel_pixels, single_pixels), PIXEL_TOLERANCE)
		<< "MSAA 下并行录制的画面与单线程不一致（深度解析可能被中间段覆盖）";
	EXPECT_GT(parallel_stats.parallel_segments, 0u) << "MSAA 下未启用并行录制";
}

/*
并行录制阈值可配（第二百零二轮）
口径：同一场景（64 个独立绘制项、四线程）；阈值 100 时不应并行（parallel_segments == 0），
      阈值 16 时应并行（parallel_segments > 0），且两次画面逐像素一致。
*/
TEST_F(Renderer_Thread_Test, 并行录制阈值可配)
{
	//按给定阈值渲染一帧并读回
	auto render_with_threshold = [](uint32_t min_items, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats) {
		engine::Renderer_Config config = hidden_config();
		config.render_threads = 4;
		config.parallel_min_items = min_items;
		engine::Renderer renderer;
		if (!renderer.init(config))
			return false;
		if (!renderer.readback_supported())
			return false;
		renderer.static_batching_set(false);
		engine::Mesh_Handle quad = 0;
		if (!renderer.mesh_load(quad, quad_data(QUAD_HALF)))
			return false;
		for (uint32_t row = 0; row < 8; ++row)
		{
			for (uint32_t column = 0; column < 8; ++column)
			{
				const float x = -0.875f + 0.25f * static_cast<float>(column);
				const float y = -0.875f + 0.25f * static_cast<float>(row);
				const float matrix[16] = {
					0.09f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.09f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.09f, 0.0f,
					x, y, 0.0f, 1.0f,
				};
				if (!renderer.mesh_draw(quad, matrix))
					return false;
			}
		}
		bool ok = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		stats = renderer.frame_stats_get();
		renderer.mesh_unload(quad);
		return ok;
	};

	//阈值高于项数：不并行
	std::vector<uint8_t> high_pixels(pixel_bytes());
	engine::Frame_Stats high_stats{};
	ASSERT_TRUE(render_with_threshold(100, high_pixels, high_stats));
	EXPECT_EQ(high_stats.parallel_segments, 0u) << "阈值 100 时 64 项不应并行";

	//阈值低于项数：并行
	std::vector<uint8_t> low_pixels(pixel_bytes());
	engine::Frame_Stats low_stats{};
	ASSERT_TRUE(render_with_threshold(16, low_pixels, low_stats));
	EXPECT_GT(low_stats.parallel_segments, 0u) << "阈值 16 时 64 项应并行";

	//两条路径画面一致
	EXPECT_LE(max_channel_delta(high_pixels, low_pixels), PIXEL_TOLERANCE)
		<< "并行与单线程的画面不一致";
}

/*
并行录制段数上限可配（第二百零三轮）
口径：四线程、64 个独立绘制项（按项数 / 16 可切 4 段）；段数上限设为 2 时
      parallel_segments 应为 2（受上限约束），默认 8 时应为 4（受项数约束）。
*/
TEST_F(Renderer_Thread_Test, 并行录制段数上限可配)
{
	//按给定段数上限渲染一帧并读回
	auto render_with_max_segments = [](uint32_t max_segments, std::vector<uint8_t>& pixels, engine::Frame_Stats& stats) {
		engine::Renderer_Config config = hidden_config();
		config.render_threads = 4;
		config.parallel_max_segments = max_segments;
		engine::Renderer renderer;
		if (!renderer.init(config))
			return false;
		if (!renderer.readback_supported())
			return false;
		renderer.static_batching_set(false);
		engine::Mesh_Handle quad = 0;
		if (!renderer.mesh_load(quad, quad_data(QUAD_HALF)))
			return false;
		for (uint32_t row = 0; row < 8; ++row)
		{
			for (uint32_t column = 0; column < 8; ++column)
			{
				const float x = -0.875f + 0.25f * static_cast<float>(column);
				const float y = -0.875f + 0.25f * static_cast<float>(row);
				const float matrix[16] = {
					0.09f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.09f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.09f, 0.0f,
					x, y, 0.0f, 1.0f,
				};
				if (!renderer.mesh_draw(quad, matrix))
					return false;
			}
		}
		bool ok = renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size()));
		stats = renderer.frame_stats_get();
		renderer.mesh_unload(quad);
		return ok;
	};

	//上限 2：段数被限制为 2
	std::vector<uint8_t> limited_pixels(pixel_bytes());
	engine::Frame_Stats limited_stats{};
	ASSERT_TRUE(render_with_max_segments(2, limited_pixels, limited_stats));
	EXPECT_EQ(limited_stats.parallel_segments, 2u) << "段数上限 2 未生效";

	//默认上限 8：项数决定段数（64 / 16 = 4）
	std::vector<uint8_t> default_pixels(pixel_bytes());
	engine::Frame_Stats default_stats{};
	ASSERT_TRUE(render_with_max_segments(8, default_pixels, default_stats));
	EXPECT_EQ(default_stats.parallel_segments, 4u) << "默认上限下段数应由项数决定";

	//两种段数的画面一致
	EXPECT_LE(max_channel_delta(limited_pixels, default_pixels), PIXEL_TOLERANCE)
		<< "不同段数下的画面不一致";
}

/*
实例剔除并行阈值可配（第二百二十二轮）
口径：同一批 64 个实例（低于默认阈值 256），阈值设为 1 时走并行、保持默认时走单线程；
      两者的剔除数 / 绘制实例数 / 像素内容必须一致，且仅前者派发过并行任务；
      另验证配置默认值、运行时设置与钳制（0 → 1、超大 → 65536）。
*/
TEST_F(Renderer_Thread_Test, 实例剔除并行阈值可配)
{
	//强制并行：四线程 ＋ 阈值 1（实例容量需容纳 64 个实例）
	engine::Renderer_Config force_config = hidden_config();
	force_config.instance_capacity = 128;
	force_config.render_threads = 4;
	force_config.cull_parallel_min_instances = 1;
	engine::Renderer forced;
	ASSERT_TRUE(forced.init(force_config));
	if (!forced.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//保持默认阈值（256 > 64，同批实例走单线程）
	engine::Renderer_Config fallback_config = force_config;
	fallback_config.cull_parallel_min_instances = 256;
	engine::Renderer fallback;
	ASSERT_TRUE(fallback.init(fallback_config));

	//配置默认值与运行时钳制
	EXPECT_EQ(forced.cull_parallel_min_instances(), 1u);
	EXPECT_EQ(fallback.cull_parallel_min_instances(), 256u);
	forced.cull_parallel_min_instances_set(0);
	EXPECT_EQ(forced.cull_parallel_min_instances(), 1u) << "0 应钳到 1";
	forced.cull_parallel_min_instances_set(100000);
	EXPECT_EQ(forced.cull_parallel_min_instances(), 65536u) << "超大值应钳到 65536";
	//恢复强制并行
	forced.cull_parallel_min_instances_set(1);

	//同一批 64 个实例矩阵
	const uint32_t small_count = 64;
	std::vector<std::array<float, 16>> matrices = instance_matrices(small_count);

	//强制并行侧
	engine::Mesh_Handle forced_mesh = 0;
	ASSERT_TRUE(forced.mesh_load(forced_mesh, quad_data(QUAD_HALF)));
	ASSERT_TRUE(forced.mesh_draw_instanced(forced_mesh, matrices[0].data(), small_count));
	std::vector<uint8_t> forced_pixels(pixel_bytes());
	ASSERT_TRUE(forced.frame_render(forced_pixels.data(), static_cast<uint32_t>(forced_pixels.size())));
	const engine::Frame_Stats forced_stats = forced.frame_stats_get();

	//单线程侧
	engine::Mesh_Handle fallback_mesh = 0;
	ASSERT_TRUE(fallback.mesh_load(fallback_mesh, quad_data(QUAD_HALF)));
	ASSERT_TRUE(fallback.mesh_draw_instanced(fallback_mesh, matrices[0].data(), small_count));
	std::vector<uint8_t> fallback_pixels(pixel_bytes());
	ASSERT_TRUE(fallback.frame_render(fallback_pixels.data(), static_cast<uint32_t>(fallback_pixels.size())));
	const engine::Frame_Stats fallback_stats = fallback.frame_stats_get();

	//统计与画面对齐（阈值只影响并行度，不影响结果）
	EXPECT_EQ(forced_stats.culled_instances, fallback_stats.culled_instances);
	EXPECT_EQ(forced_stats.drawn_instances, fallback_stats.drawn_instances);
	EXPECT_LE(max_channel_delta(forced_pixels, fallback_pixels), PIXEL_TOLERANCE)
		<< "不同阈值下的画面不一致";
	//仅强制并行侧派发了任务（64 低于差异扫描阈值 512，不会额外派发）
	EXPECT_GT(forced_stats.parallel_dispatches, 0u) << "阈值 1 未触发并行剔除";
	EXPECT_EQ(fallback_stats.parallel_dispatches, 0u) << "默认阈值下 64 个实例不应派发并行任务";

	forced.mesh_unload(forced_mesh);
	fallback.mesh_unload(fallback_mesh);
}
