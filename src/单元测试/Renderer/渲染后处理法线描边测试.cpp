/*
渲染后处理法线描边测试（第四百零七轮 · 方向三候选 D）
覆盖：强度 0 原样输出 / 折边处产生描边差异 / 与深度描边线条不同 / 法线阈值可配 / 半径可配 / 天空不描边。
口径：场景为「左半面朝相机 ＋ 右半面绕 Y 内折 45°」的折角网格——接缝两侧深度连续而法线突变，
      正是深度描边较弱、法线描边较强的位置；颜色比较只用逐通道最大差与像素计数，不假定回读通道序。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	//折角网格（顶点 1 / 2 为两块面共享，故接缝处深度连续）
	engine::Mesh_Data folded_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.7f, -1.0f, -0.7f }, { 1.0f, 1.0f } },
			{ { 0.7f, 1.0f, -0.7f }, { 1.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2 };
		return data;
	}

	//准备「目标组 ＋ 单级后处理 ＋ 折角网格」并渲染一帧；without_mesh 为 true 时只留天空（第四百零七轮）
	std::vector<uint8_t> render_outline_scene(const char* shader, const std::array<float, 4>& params,
		bool without_mesh = false, const std::array<float, 3>& color = { 0.0f, 0.0f, 0.0f })
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, shader);
		EXPECT_NE(pass, 0u);
		//链先于 present_set 设置（与既有描边 / 反射用例同序）
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		//后处理参数走通用四参入口（强度 / 法线阈值 / 深度阈值 / 半径）
		renderer.post_stage_vec4_set(0, params[0], params[1], params[2], params[3]);
		//描边色（默认黑 = 压暗轮廓）
		renderer.post_stage_color_set(0, color[0], color[1], color[2]);
		renderer.render_target_present_set(scene_target);
		engine::Mesh_Handle folded = 0;
		//无网格组：只留天空（深度恒 1，法线无效，应原样输出）
		if (!without_mesh)
		{
			EXPECT_TRUE(renderer.mesh_load(folded, folded_data()));
			EXPECT_TRUE(renderer.mesh_draw(folded, nullptr));
			renderer.mesh_unload(folded);
		}
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}

	//两帧之间「本帧比参考帧暗」的亮度差总和（阈值灵敏度用：像素数会被饱和掩盖，总量不会）
	double darkened_sum(const std::vector<uint8_t>& tested, const std::vector<uint8_t>& reference)
	{
		double sum = 0.0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				float difference = luma_of(pixel_get(reference, x, y)) - luma_of(pixel_get(tested, x, y));
				//只累计被压暗的部分
				if (difference > 0.0f)
					sum += static_cast<double>(difference);
			}
		}
		return sum;
	}

	//统计两帧之间逐通道差达到 threshold 的像素数（通道序无关）
	uint32_t differing_count(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right, int threshold)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> a = pixel_get(left, x, y);
				std::array<uint8_t, 4> b = pixel_get(right, x, y);
				//逐通道比较（前三个通道）
				int maximum = 0;
				for (std::size_t index = 0; index < 3; ++index)
				{
					int delta = static_cast<int>(a[index]) - static_cast<int>(b[index]);
					if (delta < 0)
						delta = -delta;
					if (delta > maximum)
						maximum = delta;
				}
				if (maximum >= threshold)
					++count;
			}
		}
		return count;
	}
}

//强度 0 时法线描边原样输出：与直通画面逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 法线描边强度为零原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> reference = render_outline_scene("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> disabled = render_outline_scene("法线描边.frag.spv", { 0.0f, 0.1f, 2.0f, 1.0f });
	int difference = max_channel_diff(reference, disabled);
	RecordProperty("normal_outline_disabled_max_diff", difference);
	EXPECT_LE(difference, 2) << "强度 0 未原样输出（最大差 " << difference << "）";
}

/*
折边处产生描边（第四百零七轮）
口径：折角接缝处法线突变、深度连续，法线项应压暗一批接缝像素；
      断言压暗像素数 > 10，且不超过画面四成（避免整体压黑）。
*/
TEST_F(Renderer_Post_Chain_Test, 法线描边在折边处产生差异)
{
	std::vector<uint8_t> reference = render_outline_scene("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> outlined = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.10f, 2.0f, 1.0f });
	uint32_t changes = darkened_count(outlined, reference, 0.05f);
	RecordProperty("normal_outline_changed_pixels", static_cast<int>(changes));
	EXPECT_GT(changes, 10u) << "法线描边未在折边处产生可见差异";
	EXPECT_LT(changes, WINDOW_WIDTH * WINDOW_HEIGHT * 2 / 5) << "法线描边压暗了过多像素（疑似整体变黑）";
}

/*
与深度描边的线条不同（第四百零七轮）
口径：同一折角场景下两条管线各出一帧；法线项会把接缝两侧的折边也描出来，
      故两帧在接缝附近应有明显差异，断言差异像素数 > 5。
*/
TEST_F(Renderer_Post_Chain_Test, 法线描边与深度描边线条不同)
{
	std::vector<uint8_t> normal = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.10f, 2.0f, 1.0f });
	std::vector<uint8_t> depth = render_outline_scene("深度描边.frag.spv", { 1.0f, 1.0f, 2.0f, 1.0f });
	uint32_t changes = differing_count(normal, depth, 12);
	RecordProperty("normal_vs_depth_pixels", static_cast<int>(changes));
	EXPECT_GT(changes, 5u) << "法线描边与深度描边输出几乎一致（差异像素 " << changes << "）";
}

/*
法线阈值可配（第 407 轮）
口径：阈值 0.02（极敏感，法线项饱和）应比阈值 1.0（迟钝，法线项仅部分生效）压暗更多；
      像素数会被「深度项同时生效」掩盖，故比较整帧压暗量总和。
*/
TEST_F(Renderer_Post_Chain_Test, 法线描边阈值可配)
{
	std::vector<uint8_t> reference = render_outline_scene("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> sensitive = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.02f, 2.0f, 1.0f });
	std::vector<uint8_t> dull = render_outline_scene("法线描边.frag.spv", { 1.0f, 1.0f, 2.0f, 1.0f });
	double sensitive_sum = darkened_sum(sensitive, reference);
	double dull_sum = darkened_sum(dull, reference);
	RecordProperty("normal_threshold_sensitive_sum", static_cast<int>(sensitive_sum));
	RecordProperty("normal_threshold_dull_sum", static_cast<int>(dull_sum));
	EXPECT_GT(sensitive_sum, dull_sum * 1.2) << "阈值未生效（0.02 压暗量 " << sensitive_sum
		<< " / 1.0 压暗量 " << dull_sum << "）";
}

/*
描边半径可配（第四百零七轮）
口径：半径缺省（w = 0）必须与半径 1 逐像素一致；半径 2 应覆盖更宽的线条（压暗像素更多）。
*/
TEST_F(Renderer_Post_Chain_Test, 法线描边半径可配)
{
	std::vector<uint8_t> reference = render_outline_scene("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f });
	std::vector<uint8_t> radius_zero = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.10f, 2.0f, 0.0f });
	std::vector<uint8_t> radius_one = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.10f, 2.0f, 1.0f });
	std::vector<uint8_t> radius_two = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.10f, 2.0f, 2.0f });
	//缺省值必须回落单像素
	int default_diff = max_channel_diff(radius_zero, radius_one);
	RecordProperty("normal_radius_default_diff", default_diff);
	EXPECT_EQ(default_diff, 0) << "半径缺省未回落单像素（最大差 " << default_diff << "）";
	uint32_t thin = darkened_count(radius_one, reference, 0.05f);
	uint32_t thick = darkened_count(radius_two, reference, 0.05f);
	RecordProperty("normal_radius1_pixels", static_cast<int>(thin));
	RecordProperty("normal_radius2_pixels", static_cast<int>(thick));
	EXPECT_GT(thick, thin) << "半径 2 未加宽描边（半径 1 像素 " << thin << " / 半径 2 像素 " << thick << "）";
}

//天空不描边：无几何时深度恒 1，输出应与直通逐通道差不超过 2
TEST_F(Renderer_Post_Chain_Test, 法线描边天空不描边)
{
	std::vector<uint8_t> reference = render_outline_scene("后处理.frag.spv", { 0.0f, 0.0f, 0.0f, 0.0f }, true);
	std::vector<uint8_t> sky = render_outline_scene("法线描边.frag.spv", { 1.0f, 0.10f, 2.0f, 1.0f }, true);
	int difference = max_channel_diff(reference, sky);
	RecordProperty("normal_outline_sky_max_diff", difference);
	EXPECT_LE(difference, 2) << "天空被误描边（最大差 " << difference << "）";
}
