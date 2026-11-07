/*
渲染阴影稳定性回归测试（第六百四十六轮；深研方向八 B4）
覆盖：亚纹素平移轨迹下阴影图逐字节不变（644 纹素对齐回归锁定）、静止相机帧间阴影复用、
     同一相机重复应用 / 重复绘制的幂等性；以及阴影统计（redraws / reuses）随之推进。
口径：双级级联、方向光沿 +Z、阴影开启；阴影图按 2048 上限分配回读缓冲。
*/
#include "渲染场景测试辅助.h"

#include <cstring>

namespace
{
	//构造沿 X、Z 平移的模型矩阵（列主序）
	std::array<float, 16> translate_model(float x, float y, float z)
	{
		return {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造缩放并平移到遮挡位置的模型矩阵（列主序）
	std::array<float, 16> occluder_model(float x, float y, float z)
	{
		return {
			0.6f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.6f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.6f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造「方向光沿 +Z、环境光 0」的光照组
	engine::Light_Data positive_z_light(void)
	{
		engine::Light_Data light{};
		light.direction[2] = 1.0f;
		light.intensity = 1.0f;
		light.ambient = 0.0f;
		return light;
	}

	//应用相机与光照（雾保持默认；scene_apply 会按 Light_Data 重置阴影，故调用方随后补开）
	//第六百五十四轮起级联配置由 Light_Data 驱动，故此处按需传入级联数
	void scene_apply(engine::Renderer& renderer, const engine::Camera_Data& camera, int cascade_count = 1)
	{
		engine::Light_Data light = positive_z_light();
		light.shadow_cascade_count = cascade_count;
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
		renderer.shadow_enable_set(true);
	}

	//绘制固定场景并推进一帧（遮挡面 z = -10、接收面 z = -9.5）
	void draw_scene(engine::Renderer& renderer, engine::Mesh_Handle occluder, engine::Mesh_Handle receiver)
	{
		EXPECT_TRUE(renderer.mesh_draw(occluder, occluder_model(0.0f, 0.0f, -10.0f).data()));
		EXPECT_TRUE(renderer.mesh_draw(receiver, translate_model(0.0f, 0.0f, -9.5f).data()));
		EXPECT_TRUE(renderer.frame_render());
	}

	//读取整张阴影图（按实际尺寸裁剪）
	std::vector<uint8_t> shadow_map_read(engine::Renderer& renderer, uint32_t& width_out, uint32_t& height_out)
	{
		const uint32_t capacity = 2048u * 2048u * 4u;
		std::vector<uint8_t> map(capacity);
		EXPECT_TRUE(renderer.shadow_readback(map.data(), capacity, &width_out, &height_out));
		map.resize(static_cast<std::size_t>(width_out) * height_out * 4u);
		return map;
	}
}

/*
亚纹素平移轨迹（第六百四十六轮）
口径：双级级联、相机沿 X 每帧位移 0.004（约为近区纹素 24 / 512 ≈ 0.047 的 1/12），往返 8 帧；
     纹素对齐后每帧阴影图与首帧逐字节一致，且轨迹不产生整幅重采样（644 修复的回归锁定）。
*/
TEST_F(Renderer_Scene_Test, 亚纹素平移轨迹阴影图不变)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//双级级联（数据侧配置）
	engine::Camera_Data camera{};
	scene_apply(renderer, camera, 2);
	uint32_t width = 0;
	uint32_t height = 0;
	draw_scene(renderer, occluder, receiver);
	std::vector<uint8_t> reference = shadow_map_read(renderer, width, height);
	ASSERT_FALSE(reference.empty()) << "阴影图回读为空";

	//往返 8 帧的亚纹素轨迹
	const float offsets[8] = { 0.004f, 0.008f, 0.012f, 0.016f, 0.012f, 0.008f, 0.004f, 0.0f };
	uint32_t stable_frames = 0;
	for (float offset : offsets)
	{
		camera.position[0] = offset;
		scene_apply(renderer, camera, 2);
		draw_scene(renderer, occluder, receiver);
		std::vector<uint8_t> current = shadow_map_read(renderer, width, height);
		if (current == reference)
			++stable_frames;
	}
	RecordProperty("stability_frames", stable_frames);
	RecordProperty("stability_width", width);
	EXPECT_EQ(stable_frames, 8u) << "亚纹素平移轨迹下阴影图发生重采样（纹素对齐回归失败）";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
静止相机阴影复用（第六百四十六轮）
口径：同一相机与场景连续推进 3 帧：首帧重绘、其后按签名复用；
     阴影图逐帧逐字节一致，shadow_redraws 只增 1、shadow_reuses 至少增 2。
*/
TEST_F(Renderer_Scene_Test, 静止相机阴影复用)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//双级级联（数据侧配置）
	engine::Camera_Data camera{};
	scene_apply(renderer, camera, 2);

	//首帧：必然重绘
	draw_scene(renderer, occluder, receiver);
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t redraws_after_first = static_cast<uint32_t>(renderer.frame_stats_get().shadow_redraws);
	std::vector<uint8_t> reference = shadow_map_read(renderer, width, height);
	ASSERT_FALSE(reference.empty()) << "阴影图回读为空";

	//其后两帧：签名未变 → 复用，阴影图不变
	for (uint32_t frame = 0; frame < 2; ++frame)
	{
		draw_scene(renderer, occluder, receiver);
		std::vector<uint8_t> current = shadow_map_read(renderer, width, height);
		EXPECT_TRUE(current == reference) << "静止相机下阴影图在第 " << frame + 2 << " 帧发生变化";
	}
	const engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("static_redraws", redraws_after_first);
	RecordProperty("static_reuses", stats.shadow_reuses);
	EXPECT_LE(stats.shadow_redraws, redraws_after_first) << "静止相机下阴影被重复重绘";
	EXPECT_GE(stats.shadow_reuses, 2u) << "静止相机下阴影未按签名复用";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}

/*
同一相机重复应用幂等（第六百四十六轮）
口径：同一相机连续两次 scene_apply 后重新登记并推进：阴影图逐字节一致，
     证明选择 / 登记 / 阴影签名对重复输入无副作用。
*/
TEST_F(Renderer_Scene_Test, 同一相机重复应用幂等)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle receiver = 0;
	engine::Mesh_Handle occluder = 0;
	ASSERT_TRUE(renderer.mesh_load(receiver, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	ASSERT_TRUE(renderer.mesh_load(occluder, quad_data_with_normal(0.0f, 0.0f, -1.0f)));
	renderer.texture_bind_set(texture);

	//同一相机 ＋ 同一份数据（含 2 级级联配置，第六百五十四轮起级联配置由 Light_Data 驱动）
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	light.direction[2] = 1.0f;
	light.shadow_cascade_count = 2;
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	draw_scene(renderer, occluder, receiver);
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> first = shadow_map_read(renderer, width, height);
	ASSERT_FALSE(first.empty()) << "阴影图回读为空";

	//同一相机与同一份数据再应用一次后重绘
	renderer.scene_apply(camera, light, fog);
	renderer.shadow_enable_set(true);
	draw_scene(renderer, occluder, receiver);
	std::vector<uint8_t> second = shadow_map_read(renderer, width, height);
	EXPECT_TRUE(first == second) << "同一相机重复应用后阴影图不一致（存在残留副作用）";

	renderer.mesh_unload(occluder);
	renderer.mesh_unload(receiver);
	renderer.texture_unload(texture);
}
