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
	参数（强度 / 最大距离 4 / 步数缩放 2 / 深度偏置 0.01）走通用四参入口；
	阴影色调取黑，第四分量携带抖动种子（0 = 固定中点采样，与 673 逐字节一致）。
	*/
	std::vector<uint8_t> render_contact(const std::string& frag, float strength, float seed = 0.0f)
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
