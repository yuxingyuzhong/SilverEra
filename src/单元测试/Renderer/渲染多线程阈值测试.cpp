/*
渲染多线程阈值测试（第二百二十六轮；L2 隐藏窗口 + 像素回读）
覆盖：实例缓冲差异扫描 / 缓存回写的并行阈值可配——阈值调低后第 2 帧走并行，默认阈值走单线程；
      两侧画面与剔除 / 绘制统计一致；配置默认值与运行时钳制（0 → 1、超大 → 65536）。
口径：并行只影响主机侧耗时，结果逐字节一致；像素比对用通道容差吸收驱动取整差异。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

//获取渲染器门面与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;
	//小四边形半边长
	constexpr float QUAD_HALF = 0.25f;
	//实例数（低于默认阈值 512，高于调低后的阈值 1）
	constexpr uint32_t INSTANCE_COUNT = 64;
	//像素通道容差
	constexpr int PIXEL_TOLERANCE = 2;

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

	//横向铺开的一批实例矩阵（顺序固定，两渲染器输入逐字节相同）
	std::vector<std::array<float, 16>> instance_matrices(uint32_t count)
	{
		std::vector<std::array<float, 16>> matrices(count);
		for (uint32_t index = 0; index < count; ++index)
		{
			const float t = static_cast<float>(index) / static_cast<float>(count > 1 ? count - 1 : 1);
			const float x = (t * 2.0f - 1.0f) * 2.0f;
			matrices[index] = translation(x, 0.0f, 0.0f);
		}
		return matrices;
	}

	//逐通道最大差
	int max_channel_delta(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs)
	{
		int result = 0;
		const std::size_t size = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
		for (std::size_t index = 0; index < size; ++index)
		{
			const int diff = static_cast<int>(lhs[index]) - static_cast<int>(rhs[index]);
			const int magnitude = diff < 0 ? -diff : diff;
			if (magnitude > result)
				result = magnitude;
		}
		return result;
	}
}

/*
实例扫描 / 回写并行阈值可配（第二百二十六轮）
口径：同一批 64 实例渲染两帧（第 1 帧整块写入并建立缓存，第 2 帧修改一个实例触发差异扫描与回写）；
      阈值 1 时第 2 帧走并行（扫描 / 回写派发 > 0），默认阈值 512 时保持单线程（两者均为 0）；
      两侧剔除 / 绘制实例数与画面一致；另验证默认值与钳制。
*/
TEST(Renderer_Thread_Threshold_Test, 实例扫描并行阈值可配)
{
	//强制并行：四线程 ＋ 扫描阈值 1
	engine::Renderer_Config force_config = hidden_config();
	force_config.instance_capacity = 128;
	force_config.render_threads = 4;
	force_config.instance_scan_parallel_min = 1;
	engine::Renderer forced;
	ASSERT_TRUE(forced.init(force_config));
	if (!forced.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//默认阈值（512 > 64，同批实例走单线程）
	engine::Renderer_Config fallback_config = force_config;
	fallback_config.instance_scan_parallel_min = 512;
	engine::Renderer fallback;
	ASSERT_TRUE(fallback.init(fallback_config));

	//默认值与运行时钳制
	EXPECT_EQ(forced.instance_scan_parallel_min(), 1u);
	EXPECT_EQ(fallback.instance_scan_parallel_min(), 512u);
	EXPECT_EQ(engine::Renderer_Config{}.instance_scan_parallel_min, 512u) << "配置默认值应为 512";
	fallback.instance_scan_parallel_min_set(0);
	EXPECT_EQ(fallback.instance_scan_parallel_min(), 1u) << "0 应钳到 1";
	fallback.instance_scan_parallel_min_set(100000);
	EXPECT_EQ(fallback.instance_scan_parallel_min(), 65536u) << "超大值应钳到 65536";
	fallback.instance_scan_parallel_min_set(512);

	//同一批实例矩阵
	std::vector<std::array<float, 16>> matrices = instance_matrices(INSTANCE_COUNT);

	//两个渲染器各自登记网格并渲染三帧（第 3 帧回到首个帧槽并改动实例，触发差异扫描）
	auto render_three_frames = [&](engine::Renderer& renderer, engine::Mesh_Handle& mesh,
		std::vector<uint8_t>& pixels, engine::Frame_Stats& stats)
	{
		EXPECT_TRUE(renderer.mesh_load(mesh, quad_data(QUAD_HALF)));
		//第 1 / 2 帧（两个帧槽各做一次整块写入并建立缓存）
		EXPECT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), INSTANCE_COUNT));
		EXPECT_TRUE(renderer.frame_render());
		EXPECT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), INSTANCE_COUNT));
		EXPECT_TRUE(renderer.frame_render());
		//第 3 帧（回到首个帧槽：改动一个实例，触发差异扫描与分段写入）
		matrices[0] = translation(-2.0f, 0.2f, 0.0f);
		EXPECT_TRUE(renderer.mesh_draw_instanced(mesh, matrices[0].data(), INSTANCE_COUNT));
		pixels.resize(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		stats = renderer.frame_stats_get();
	};

	engine::Mesh_Handle forced_mesh = 0;
	engine::Mesh_Handle fallback_mesh = 0;
	std::vector<uint8_t> forced_pixels;
	std::vector<uint8_t> fallback_pixels;
	engine::Frame_Stats forced_stats{};
	engine::Frame_Stats fallback_stats{};
	render_three_frames(forced, forced_mesh, forced_pixels, forced_stats);
	render_three_frames(fallback, fallback_mesh, fallback_pixels, fallback_stats);

	//统计与画面对齐（阈值只影响并行度）
	EXPECT_EQ(forced_stats.culled_instances, fallback_stats.culled_instances);
	EXPECT_EQ(forced_stats.drawn_instances, fallback_stats.drawn_instances);
	EXPECT_LE(max_channel_delta(forced_pixels, fallback_pixels), PIXEL_TOLERANCE)
		<< "不同扫描阈值下的画面不一致";
	//阈值 1：第 2 帧的差异扫描与缓存回写都走并行
	EXPECT_GT(forced_stats.scan_parallel_dispatches, 0u) << "扫描阈值 1 未触发并行差异扫描";
	EXPECT_GT(forced_stats.cache_parallel_dispatches, 0u) << "扫描阈值 1 未触发并行缓存回写";
	//默认阈值：64 实例保持单线程
	EXPECT_EQ(fallback_stats.scan_parallel_dispatches, 0u) << "默认阈值下 64 实例不应并行扫描";
	EXPECT_EQ(fallback_stats.cache_parallel_dispatches, 0u) << "默认阈值下 64 实例不应并行回写";

	//清理（mesh_unload 无返回值）
	forced.mesh_unload(forced_mesh);
	fallback.mesh_unload(fallback_mesh);
}
