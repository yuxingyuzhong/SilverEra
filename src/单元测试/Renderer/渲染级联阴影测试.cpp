/*
渲染级联阴影测试（第一百二十一轮）
覆盖：默认单级画面逐字节不变 / 双级远端覆盖 / 双级下近区仍遮蔽 / 切回单级恢复 / 非法参数忽略。
口径：方向光沿 +Z、强度 1、环境光 0；接收面与遮挡面使用带 -Z 法线的四边形；
     单级为「以世界原点为中心、半宽 12」的固定视锥，双级为「以相机为中心、半径 12 / 36」的两片包围球。
*/
#include "渲染场景测试辅助.h"

#include <cstring>

namespace
{
	//构造沿 X、Z 平移的模型矩阵（列主序）
	std::array<float, 16> translate_matrix(float x, float y, float z)
	{
		return {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//构造缩放并平移到遮挡位置的模型矩阵（列主序）
	std::array<float, 16> occluder_matrix(float x, float y, float z)
	{
		return {
			0.6f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.6f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.6f, 0.0f,
			x, y, z, 1.0f,
		};
	}

	//两帧像素是否逐字节一致
	bool pixels_equal(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		return left.size() == right.size()
			&& std::memcmp(left.data(), right.data(), left.size()) == 0;
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

	//应用相机与光照（雾保持默认）
	void lights_apply(engine::Renderer& renderer, const engine::Camera_Data& camera, const engine::Light_Data& light)
	{
		engine::Fog_Data fog{};
		renderer.scene_apply(camera, light, fog);
	}

	//在指定相机与场景下渲染一帧并返回整帧像素
	std::vector<uint8_t> render_once(engine::Renderer& renderer)
	{
		std::vector<uint8_t> pixels(pixel_bytes());
		EXPECT_TRUE(renderer.frame_render(pixels.data(), static_cast<std::uint32_t>(pixels.size())));
		return pixels;
	}
}

/*
默认单级画面逐字节不变（第一百二十一轮）
口径：开阴影、遮挡面居中；未显式设置级联与显式设为「1 级 / 3 / 12」两帧逐字节一致，
     且中心仍被遮蔽（单级行为未受级联实现影响）。
*/
TEST_F(Renderer_Scene_Test, 级联阴影默认单级不变)
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

	//场景：方向光沿 +Z、环境光 0、阴影开启
	engine::Camera_Data camera{};
	lights_apply(renderer, camera, positive_z_light());
	renderer.shadow_enable_set(true);

	//默认级联配置（1 级）下渲染
	std::array<float, 16> occluder_transform = occluder_matrix(0.0f, 0.0f, -0.5f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> default_pixels = render_once(renderer);

	//显式设为 1 级后渲染
	renderer.shadow_cascade_set(1, 3.0f, 12.0f);
	uint32_t cascade_count = 0;
	float far_scale = 0.0f;
	float split_distance = 0.0f;
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 1u);
	EXPECT_FLOAT_EQ(far_scale, 3.0f);
	EXPECT_FLOAT_EQ(split_distance, 12.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> explicit_pixels = render_once(renderer);

	RecordProperty("cascade_default_center", center_brightness(default_pixels));
	RecordProperty("cascade_explicit_center", center_brightness(explicit_pixels));

	EXPECT_TRUE(pixels_equal(default_pixels, explicit_pixels))
		<< "显式设为 1 级后画面与默认不一致（单级行为被改变）";
	EXPECT_LT(center_brightness(default_pixels), 60u) << "单级下中心未被遮蔽";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
双级远端覆盖（第一百二十一轮）
口径：相机移到 (20, 0, 3) 注视 (20, 0, 0)，接收面与遮挡面均位于 x = 20；
     单级（原点为中心、半宽 12）覆盖不到该区域 → 中心受光；
     切双级（以相机为中心，半径 12 / 36）后该区域落入近区 → 中心被遮蔽。
*/
TEST_F(Renderer_Scene_Test, 级联阴影远端覆盖)
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

	//相机移到 x = 20 处，观察同一位置上的接收面与遮挡面
	engine::Camera_Data camera{};
	camera.position[0] = 20.0f;
	camera.position[2] = 3.0f;
	camera.target[0] = 20.0f;
	lights_apply(renderer, camera, positive_z_light());
	renderer.shadow_enable_set(true);

	//接收面（x = 20）与遮挡面（x = 20、z = -0.5）
	std::array<float, 16> receiver_transform = translate_matrix(20.0f, 0.0f, 0.0f);
	std::array<float, 16> occluder_transform = occluder_matrix(20.0f, 0.0f, -0.5f);

	//单级：视锥以原点为中心，x = 20 落在范围外 → 受光
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> single_pixels = render_once(renderer);

	//双级：两片以相机为中心 → x = 20 落入近区 → 遮蔽
	renderer.shadow_cascade_set(2, 3.0f, 12.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> dual_pixels = render_once(renderer);

	const uint32_t single_brightness = center_brightness(single_pixels);
	const uint32_t dual_brightness = center_brightness(dual_pixels);
	RecordProperty("cascade_far_single", single_brightness);
	RecordProperty("cascade_far_dual", dual_brightness);

	EXPECT_GE(single_brightness, 150u) << "单级视锥外的接收面被意外遮蔽（" << single_brightness << "）";
	EXPECT_LT(dual_brightness, 60u) << "双级未覆盖相机周边区域（" << dual_brightness << "）";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
双级下近区仍遮蔽且切回单级恢复（第一百二十一轮）
口径：默认相机、遮挡面居中；切双级后中心仍被遮蔽（近区半片工作正常），
     再切回单级后画面与最初单级帧逐字节一致（无残留副作用）。
*/
TEST_F(Renderer_Scene_Test, 级联阴影切回单级恢复)
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

	//默认相机、方向光沿 +Z、阴影开启
	engine::Camera_Data camera{};
	lights_apply(renderer, camera, positive_z_light());
	renderer.shadow_enable_set(true);

	std::array<float, 16> occluder_transform = occluder_matrix(0.0f, 0.0f, -0.5f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> single_pixels = render_once(renderer);

	//切双级：近区半片应继续遮蔽中心
	renderer.shadow_cascade_set(2, 3.0f, 12.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> dual_pixels = render_once(renderer);

	//切回单级：画面应与最初单级帧一致
	renderer.shadow_cascade_set(1, 3.0f, 12.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, nullptr));
	std::vector<uint8_t> restored_pixels = render_once(renderer);

	RecordProperty("cascade_near_single", center_brightness(single_pixels));
	RecordProperty("cascade_near_dual", center_brightness(dual_pixels));
	RecordProperty("cascade_near_restored", center_brightness(restored_pixels));

	EXPECT_LT(center_brightness(single_pixels), 60u) << "单级下中心未被遮蔽";
	EXPECT_LT(center_brightness(dual_pixels), 60u) << "双级近区未遮蔽中心（"
		<< center_brightness(dual_pixels) << "）";
	EXPECT_TRUE(pixels_equal(single_pixels, restored_pixels)) << "切回单级后画面未恢复";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
三级级联覆盖中距并切回单级恢复（第一百四十六轮）
口径：接收面与遮挡面移到 z = -20（与相机距离约 23）；级联配置 3 级 / 远区倍数 3 / 切换 12：
      第 2 片半径为 36，故中距应被遮蔽；单级视锥以原点为中心、深度覆盖不到该处 → 受光；
      切回单级后画面与最初单级帧逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 三级级联覆盖中距并切回单级恢复)
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

	//相机注视 z = -20 处，远平面放宽以覆盖中距
	engine::Camera_Data camera{};
	camera.position[2] = 3.0f;
	camera.target[2] = -20.0f;
	camera.far_plane = 200.0f;
	lights_apply(renderer, camera, positive_z_light());
	renderer.shadow_enable_set(true);

	//接收面与遮挡面均位于 z = -20（遮挡面再退 0.5）
	const std::array<float, 16> receiver_transform = translate_matrix(0.0f, 0.0f, -20.0f);
	const std::array<float, 16> occluder_transform = occluder_matrix(0.0f, 0.0f, -20.5f);

	//单级：光源视锥以原点为中心，深度覆盖不到 z = -20 → 受光
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> single_pixels = render_once(renderer);

	//三级：阈值 12 / 36，距离 23 落入第 2 片（半径 36）→ 遮蔽
	renderer.shadow_cascade_set(3, 3.0f, 12.0f);
	uint32_t cascade_count = 0;
	float far_scale = 0.0f;
	float split_distance = 0.0f;
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 3u) << "三级配置未生效";
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> triple_pixels = render_once(renderer);

	//切回单级：画面应与最初单级帧一致
	renderer.shadow_cascade_set(1, 3.0f, 12.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> restored_pixels = render_once(renderer);

	RecordProperty("cascade3_single", center_brightness(single_pixels));
	RecordProperty("cascade3_triple", center_brightness(triple_pixels));
	RecordProperty("cascade3_restored", center_brightness(restored_pixels));

	EXPECT_GE(center_brightness(single_pixels), 150u) << "单级视锥外的接收面被意外遮蔽";
	EXPECT_LT(center_brightness(triple_pixels), 80u) << "三级中距未被第 2 片覆盖（"
		<< center_brightness(triple_pixels) << "）";
	EXPECT_TRUE(pixels_equal(single_pixels, restored_pixels)) << "切回单级后画面未恢复";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
级联接缝淡化（第一百四十八轮）
口径：接收面与遮挡面位于 z = -9.5（与相机距离约 12.5，恰在第 1 个阈值 12 附近）；
      三级、split 12 / 倍数 3：硬切换（淡化 0）时片元归第 2 片（半径 36）→ 中心被遮蔽；
      淡化 0.3 时带内与第 1 片（半径 12，覆盖不到该处 → 受光）混合 → 中心明显变亮；
      淡化归零后画面与最初硬切换帧逐字节一致。
*/
TEST_F(Renderer_Scene_Test, 级联接缝淡化)
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

	//默认相机（z = 3）；接收面与遮挡面位于 z = -9.5
	engine::Camera_Data camera{};
	camera.far_plane = 200.0f;
	lights_apply(renderer, camera, positive_z_light());
	renderer.shadow_enable_set(true);
	const std::array<float, 16> receiver_transform = translate_matrix(0.0f, 0.0f, -9.5f);
	const std::array<float, 16> occluder_transform = occluder_matrix(0.0f, 0.0f, -10.0f);

	//三级 + 硬切换
	renderer.shadow_cascade_set(3, 3.0f, 12.0f);
	ASSERT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.0f) << "淡化默认值应为 0";
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> hard_pixels = render_once(renderer);

	//淡化 0.3：带内混合 → 中心变亮
	renderer.shadow_cascade_blend_set(0.3f);
	EXPECT_FLOAT_EQ(renderer.shadow_cascade_blend(), 0.3f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> blended_pixels = render_once(renderer);

	//归零：逐字节回到硬切换
	renderer.shadow_cascade_blend_set(0.0f);
	ASSERT_TRUE(renderer.mesh_draw(occluder, occluder_transform.data()));
	ASSERT_TRUE(renderer.mesh_draw(receiver, receiver_transform.data()));
	std::vector<uint8_t> restored_pixels = render_once(renderer);

	const uint32_t hard_brightness = center_brightness(hard_pixels);
	const uint32_t blended_brightness = center_brightness(blended_pixels);
	RecordProperty("cascade_blend_hard", hard_brightness);
	RecordProperty("cascade_blend_mixed", blended_brightness);
	EXPECT_LT(hard_brightness, 60u) << "硬切换下中心未被第 2 片遮蔽";
	EXPECT_GT(blended_brightness, hard_brightness + 30u) << "淡化未在阈值附近混合（"
		<< hard_brightness << " → " << blended_brightness << "）";
	EXPECT_TRUE(pixels_equal(hard_pixels, restored_pixels)) << "淡化归零后画面未恢复";

	renderer.mesh_unload(receiver);
	renderer.mesh_unload(occluder);
	renderer.texture_unload(texture);
}

/*
级联参数非法忽略（第一百二十一轮；第一百四十六轮级联数上限扩为 3）
口径：级联数只接受 1 / 2 / 3，远区倍数须 > 1，切换距离须 > 0；
     四类非法调用都应整次忽略，配置保持调用前的值。
*/
TEST_F(Renderer_Scene_Test, 级联参数非法忽略)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));

	//初始为 1 级
	uint32_t cascade_count = 0;
	float far_scale = 0.0f;
	float split_distance = 0.0f;
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 1u);

	//非法级联数（0 与超过上限的 4）
	renderer.shadow_cascade_set(0, 3.0f, 12.0f);
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 1u) << "级联数 0 未被忽略";
	renderer.shadow_cascade_set(4, 3.0f, 12.0f);
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 1u) << "非法级联数未被忽略";

	//非法远区倍数
	renderer.shadow_cascade_set(2, 0.5f, 12.0f);
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 1u) << "非法远区倍数未被忽略";

	//非法切换距离
	renderer.shadow_cascade_set(2, 3.0f, -1.0f);
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 1u) << "非法切换距离未被忽略";

	//合法调用生效
	renderer.shadow_cascade_set(2, 2.5f, 8.0f);
	renderer.shadow_cascade_get(&cascade_count, &far_scale, &split_distance);
	EXPECT_EQ(cascade_count, 2u);
	EXPECT_FLOAT_EQ(far_scale, 2.5f);
	EXPECT_FLOAT_EQ(split_distance, 8.0f);
}
