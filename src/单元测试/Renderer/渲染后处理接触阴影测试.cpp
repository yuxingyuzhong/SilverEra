/*
渲染后处理接触阴影测试（第六百七十三轮 · 方向八 D3）
覆盖：强度 0 原样输出（与直通逐通道差 ≤ 4）/ 强度 > 0 时侧后方遮挡压暗接收面（结构性差异）。
口径：场景为「左侧接收面（z = 0）」＋「右侧偏后遮挡板（z = -0.4）」；方向光自相机一侧斜射，
      接触阴影沿主光方向扫过接收面右侧边缘。
*/
#include "渲染后处理链测试辅助.h"

namespace
{
	/*
	构造水平遮挡板（y = height 平面，xz 半宽 half）
	顶点顺序与 quad_data 一致（同一套索引）。
	*/
	engine::Mesh_Data horizontal_quad(float half, float height)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -half, height, -half }, { 0.0f, 1.0f } },
			{ { -half, height, half }, { 0.0f, 0.0f } },
			{ { half, height, half }, { 1.0f, 0.0f } },
			{ { half, height, -half }, { 1.0f, 1.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	/*
	构造位于指定中心与深度的竖直四边形（朝向相机 +Z；无 UV 依赖）
	*/
	engine::Mesh_Data offset_quad(float half, float center_x, float center_z)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { center_x - half, -half, center_z }, { 0.0f, 1.0f } },
			{ { center_x + half, -half, center_z }, { 1.0f, 1.0f } },
			{ { center_x + half, half, center_z }, { 1.0f, 0.0f } },
			{ { center_x - half, half, center_z }, { 0.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	/*
	渲染一帧接触阴影
	frag = 后处理片元名；strength = 接触阴影强度（0 = 关闭）；seed = 抖动种子（0 = 关闭抖动）；
	thickness = 厚度上限（NDC；0 = 自动，第七百零三轮）；
	参数（强度 / 最大距离 4 / 步数缩放 2 / 深度偏置 0.01）走通用四参入口；
	阴影色调取黑，第四分量携带抖动种子（0 = 固定中点采样，与 673 逐字节一致）。
	*/
	std::vector<uint8_t> render_contact(const std::string& frag, float strength, float seed = 0.0f,
		float thickness = 0.0f)
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, frag);
		EXPECT_NE(pass, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		//强度 / 最大距离 / 步数缩放 / 深度偏置
		renderer.post_stage_vec4_set(0, strength, 4.0f, 2.0f, 0.01f);
		//阴影色调（黑：直接压暗）＋ 抖动种子（第四分量）
		renderer.post_stage_color4_set(0, 0.0f, 0.0f, 0.0f, seed);
		//厚度上限（0 = 自动；经 offset 128 的逐级 extra 下发）
		renderer.post_stage_extra_set(0, thickness, 0.0f, 0.0f, 0.0f);
		renderer.render_target_present_set(scene_target);

		//方向光自相机一侧斜射（光线朝左后方，便于接收面射线扫过侧后方的遮挡板）
		engine::Light_Data light{};
		light.direction[0] = -0.5f;
		light.direction[2] = 0.866f;
		light.intensity = 1.0f;
		light.ambient = 0.3f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);

		//左侧接收面（z = 0）与右侧偏后遮挡板（z = -0.4）
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		EXPECT_TRUE(renderer.mesh_load(receiver, offset_quad(0.5f, -0.45f, 0.0f)));
		EXPECT_TRUE(renderer.mesh_load(occluder, offset_quad(0.5f, 0.45f, -0.4f)));
		EXPECT_TRUE(renderer.mesh_draw(receiver, nullptr));
		EXPECT_TRUE(renderer.mesh_draw(occluder, nullptr));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);

		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}

	//两帧画面的最大逐通道差（前三个通道）
	int max_color_diff(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		int maximum = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> first = pixel_get(left, x, y);
				std::array<uint8_t, 4> second = pixel_get(right, x, y);
				for (std::size_t index = 0; index < 3; ++index)
				{
					int delta = static_cast<int>(first[index]) - static_cast<int>(second[index]);
					maximum = (std::max)(maximum, delta < 0 ? -delta : delta);
				}
			}
		return maximum;
	}

	/*
	渲染一帧聚光灯接触阴影（第七百零四轮）
	strength = 接触阴影强度；slot = 传给 post_extra.z 的槽位（0..7）；
	spot_direction = 聚光灯传播方向（朝向 / 背离接收面分别验证生效与不生效）；
	方向光强度置 0（聚光灯模式不依赖方向光，接收面由聚光灯 ＋ 环境光照亮）。
	*/
	std::vector<uint8_t> render_contact_spot(float strength, uint32_t slot,
		const float spot_direction[3])
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "接触阴影.frag.spv");
		EXPECT_NE(pass, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		//强度 / 最大距离 / 步数缩放 / 深度偏置
		renderer.post_stage_vec4_set(0, strength, 4.0f, 2.0f, 0.01f);
		//阴影色调（黑）＋ 抖动种子 0（固定中点）
		renderer.post_stage_color4_set(0, 0.0f, 0.0f, 0.0f, 0.0f);
		//厚度 0（自动）/ 光源模式 1（聚光灯）/ 槽位
		renderer.post_stage_extra_set(0, 0.0f, 1.0f, static_cast<float>(slot), 0.0f);
		renderer.render_target_present_set(scene_target);

		//方向光关闭（强度 0），环境光 0.3
		engine::Light_Data light{};
		light.intensity = 0.0f;
		light.ambient = 0.3f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);

		//聚光灯：位于接收面右后方（照射方向与方向光口径接近，便于扫过遮挡板；半径 6，锥 30° / 45°）
		const float spot_position[3] = { 0.8f, 0.0f, -2.5f };
		const float spot_color[3] = { 1.0f, 1.0f, 1.0f };
		EXPECT_TRUE(renderer.light_add_spot(spot_position, spot_direction, spot_color,
			6.0f, 1.0f, 30.0f, 45.0f, 0.0f));

		//左侧接收面（z = 0）与右侧偏后遮挡板（z = -0.4）
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		EXPECT_TRUE(renderer.mesh_load(receiver, offset_quad(0.5f, -0.45f, 0.0f)));
		EXPECT_TRUE(renderer.mesh_load(occluder, offset_quad(0.5f, 0.45f, -0.4f)));
		EXPECT_TRUE(renderer.mesh_draw(receiver, nullptr));
		EXPECT_TRUE(renderer.mesh_draw(occluder, nullptr));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);

		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}

	/*
	渲染一帧点光源接触阴影（第七百零五轮）
	strength = 强度；slot = 传给 post_extra.z 的槽位（0..7）；spot_slot = 真则槽位登记为聚光灯（用于类型不符用例）。
	方向光强度置 0，接收面由点光源 ＋ 环境光照亮。
	*/
	std::vector<uint8_t> render_contact_point(float strength, uint32_t slot, bool spot_slot)
	{
		engine::Renderer renderer;
		EXPECT_TRUE(renderer.init(hidden_config()));
		uint64_t scene_target = 0;
		EXPECT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
		uint64_t pass = post_pipeline_register(renderer, "接触阴影.frag.spv");
		EXPECT_NE(pass, 0u);
		EXPECT_TRUE(renderer.post_chain_set(&pass, 1));
		//强度 / 最大距离 / 步数缩放 / 深度偏置
		renderer.post_stage_vec4_set(0, strength, 4.0f, 2.0f, 0.01f);
		//阴影色调（黑）＋ 抖动种子 0（固定中点）
		renderer.post_stage_color4_set(0, 0.0f, 0.0f, 0.0f, 0.0f);
		//厚度 0（自动）/ 光源模式 2（点光源）/ 槽位
		renderer.post_stage_extra_set(0, 0.0f, 2.0f, static_cast<float>(slot), 0.0f);
		renderer.render_target_present_set(scene_target);

		//方向光关闭（强度 0），环境光 0.3
		engine::Light_Data light{};
		light.intensity = 0.0f;
		light.ambient = 0.3f;
		engine::Camera_Data camera{};
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);

		//点光源：与聚光灯用例同灯位（右后方、半径 6）
		const float point_position[3] = { 0.8f, 0.0f, -2.5f };
		const float point_color[3] = { 1.0f, 1.0f, 1.0f };
		if (spot_slot)
		{
			//登记为聚光灯（传播方向指向接收面），用于「模式与槽位类型不符」用例
			const float spot_direction[3] = { -1.25f, 0.0f, 2.5f };
			EXPECT_TRUE(renderer.light_add_spot(point_position, spot_direction, point_color,
				6.0f, 1.0f, 30.0f, 45.0f, 0.0f));
		}
		else
			EXPECT_TRUE(renderer.light_add_point(point_position, point_color, 6.0f, 1.0f, 0.0f));

		//左侧接收面（z = 0）与右侧偏后遮挡板（z = -0.4）
		engine::Mesh_Handle receiver = 0;
		engine::Mesh_Handle occluder = 0;
		EXPECT_TRUE(renderer.mesh_load(receiver, offset_quad(0.5f, -0.45f, 0.0f)));
		EXPECT_TRUE(renderer.mesh_load(occluder, offset_quad(0.5f, 0.45f, -0.4f)));
		EXPECT_TRUE(renderer.mesh_draw(receiver, nullptr));
		EXPECT_TRUE(renderer.mesh_draw(occluder, nullptr));
		renderer.mesh_unload(receiver);
		renderer.mesh_unload(occluder);

		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
		renderer.pipeline_unload(pass);
		renderer.render_target_destroy(scene_target);
		return pixels;
	}
}

//强度 0 时接触阴影阶段原样输出：与直通管线逐通道差不超过 4
TEST_F(Renderer_Post_Chain_Test, 接触阴影关闭原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> pass_through = render_contact("后处理.frag.spv", 0.0f);
	std::vector<uint8_t> disabled = render_contact("接触阴影.frag.spv", 0.0f);
	EXPECT_LE(max_color_diff(pass_through, disabled), 4) << "关闭态未原样输出";
}

//强度 > 0 时近处遮挡在接收面上形成压暗（差异像素与压暗像素均超过阈值）
TEST_F(Renderer_Post_Chain_Test, 接触阴影开启压暗接收面)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> disabled = render_contact("接触阴影.frag.spv", 0.0f);
	std::vector<uint8_t> enabled = render_contact("接触阴影.frag.spv", 1.0f);
	//差异像素（逐通道差 ≥ 8）
	uint32_t difference = 0;
	uint32_t darkened = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
		{
			std::array<uint8_t, 4> before = pixel_get(disabled, x, y);
			std::array<uint8_t, 4> after = pixel_get(enabled, x, y);
			int maximum = 0;
			for (std::size_t index = 0; index < 3; ++index)
			{
				int delta = static_cast<int>(before[index]) - static_cast<int>(after[index]);
				maximum = (std::max)(maximum, delta < 0 ? -delta : delta);
			}
			if (maximum >= 8)
				++difference;
			//压暗判定（亮度下降至少 8）
			if (luma_of(before) - luma_of(after) >= 8.0f)
				++darkened;
		}
	RecordProperty("contact_difference_pixels", difference);
	RecordProperty("contact_darkened_pixels", darkened);
	EXPECT_GT(difference, 10u) << "接触阴影未改变画面（差异像素 " << difference << "）";
	EXPECT_GT(darkened, 10u) << "接触阴影未压暗任何像素（压暗像素 " << darkened << "）";
}

/*
步进抖动与互补相位平均（第六百七十五轮）
口径：强度 1、遮挡场景；种子 0（固定中点）→ 种子 0.37：抖动相位改变半影采样，画面出现差异像素。
*/
TEST_F(Renderer_Post_Chain_Test, 接触阴影抖动相位生效)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> fixed = render_contact("接触阴影.frag.spv", 1.0f, 0.0f);
	std::vector<uint8_t> jittered = render_contact("接触阴影.frag.spv", 1.0f, 0.37f);
	//差异像素（逐通道差 ≥ 1）
	uint32_t difference = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			if (pixel_get(fixed, x, y) != pixel_get(jittered, x, y))
				++difference;
	RecordProperty("contact_jitter_difference_pixels", difference);
	EXPECT_GT(difference, 0u) << "抖动种子未改变接触阴影采样";
}

/*
厚度上限参数化（第七百零三轮；Q-B）
口径：强度 1、遮挡场景；post_extra.x = 0（自动，≈ max_distance / 近远跨度 ≈ 0.04）为基准；
      0.002（仅近距接触）应显著减少压暗像素；0.2（大于自动）应覆盖自动的全部样本（逐像素单调不减）。
*/
TEST_F(Renderer_Post_Chain_Test, 接触阴影厚度上限参数化)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//关闭态参考（强度 0）
	std::vector<uint8_t> disabled = render_contact("接触阴影.frag.spv", 0.0f);
	std::vector<uint8_t> automatic = render_contact("接触阴影.frag.spv", 1.0f, 0.0f, 0.0f);
	std::vector<uint8_t> thin = render_contact("接触阴影.frag.spv", 1.0f, 0.0f, 0.002f);
	std::vector<uint8_t> thick = render_contact("接触阴影.frag.spv", 1.0f, 0.0f, 0.2f);
	//相对关闭态的三帧压暗像素数（亮度下降至少 8）
	int auto_count = static_cast<int>(darkened_count(automatic, disabled, 8.0f));
	int thin_count = static_cast<int>(darkened_count(thin, disabled, 8.0f));
	int thick_count = static_cast<int>(darkened_count(thick, disabled, 8.0f));
	RecordProperty("contact_thickness_auto_darkened", auto_count);
	RecordProperty("contact_thickness_thin_darkened", thin_count);
	RecordProperty("contact_thickness_thick_darkened", thick_count);
	EXPECT_GT(auto_count, thin_count) << "偏小厚度未抑制远处遮挡";
	EXPECT_GE(thick_count, auto_count) << "偏大厚度未覆盖自动样本（破坏单调性）";
}

/*
聚光灯接触阴影（第七百零四轮；D3 扩展）
口径：方向光关闭、只开一盏聚光灯（槽位 0），post_extra.y = 1、z = 槽位；
      朝向接收面时产生压暗；空槽位 / 锥外一律原样输出（逐字节一致）。
*/
TEST_F(Renderer_Post_Chain_Test, 接触阴影聚光灯生效)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//聚光灯位于接收面右后方，传播方向指向接收面中心（-0.45, 0, 0）
	const float toward_receiver[3] = { -1.25f, 0.0f, 2.5f };
	std::vector<uint8_t> disabled = render_contact_spot(0.0f, 0, toward_receiver);
	std::vector<uint8_t> enabled = render_contact_spot(1.0f, 0, toward_receiver);
	//相对关闭态的压暗像素数（亮度下降至少 8）
	uint32_t darkened = darkened_count(enabled, disabled, 8.0f);
	RecordProperty("contact_spot_darkened_pixels", darkened);
	EXPECT_GT(darkened, 10u) << "聚光灯接触阴影未压暗接收面（压暗像素 " << darkened << "）";
}

/*
空槽位 / 锥外原样输出（第七百零四轮）
口径：槽位 7 未登记光源；聚光灯方向背离接收面时锥遮罩为 0；两帧应与强度 0 逐字节一致。
*/
TEST_F(Renderer_Post_Chain_Test, 接触阴影聚光灯空槽位与锥外原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	//槽位 7 未登记光源
	const float toward_receiver[3] = { -1.25f, 0.0f, 2.5f };
	std::vector<uint8_t> empty_off = render_contact_spot(0.0f, 7, toward_receiver);
	std::vector<uint8_t> empty_on = render_contact_spot(1.0f, 7, toward_receiver);
	EXPECT_EQ(max_color_diff(empty_off, empty_on), 0) << "空槽位未原样输出";
	//聚光灯方向背离接收面（锥外）
	const float away_from_receiver[3] = { 1.25f, 0.0f, -2.5f };
	std::vector<uint8_t> away_off = render_contact_spot(0.0f, 0, away_from_receiver);
	std::vector<uint8_t> away_on = render_contact_spot(1.0f, 0, away_from_receiver);
	EXPECT_EQ(max_color_diff(away_off, away_on), 0) << "锥外未原样输出";
}

/*
点光源接触阴影（第七百零五轮；D3 扩展）
口径：方向光关闭、只开一盏点光源（槽位 0），post_extra.y = 2、z = 槽位；
      点光源产生压暗；点光模式下槽位实为聚光灯时原样输出（类型不符）。
*/
TEST_F(Renderer_Post_Chain_Test, 接触阴影点光源生效)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> disabled = render_contact_point(0.0f, 0, false);
	std::vector<uint8_t> enabled = render_contact_point(1.0f, 0, false);
	//相对关闭态的压暗像素数（亮度下降至少 8）
	uint32_t darkened = darkened_count(enabled, disabled, 8.0f);
	RecordProperty("contact_point_darkened_pixels", darkened);
	EXPECT_GT(darkened, 10u) << "点光源接触阴影未压暗接收面（压暗像素 " << darkened << "）";
}

/*
点光模式与槽位类型不符（第七百零五轮）
口径：槽位 0 登记为聚光灯、模式 2（点光源）→ 类型不符，应与强度 0 逐字节一致。
*/
TEST_F(Renderer_Post_Chain_Test, 接触阴影点光模式聚光槽位原样输出)
{
	engine::Renderer probe;
	ASSERT_TRUE(probe.init(hidden_config()));
	if (!probe.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	std::vector<uint8_t> disabled = render_contact_point(0.0f, 0, true);
	std::vector<uint8_t> enabled = render_contact_point(1.0f, 0, true);
	EXPECT_EQ(max_color_diff(disabled, enabled), 0) << "类型不符未原样输出";
}
