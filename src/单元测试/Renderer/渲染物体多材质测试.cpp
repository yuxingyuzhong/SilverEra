/*
渲染物体多材质测试（第四百一十轮 · 方向四阶段 C）
覆盖：索引区间只画该区间 / 区间越界被拒绝 / 多材质分段使用两个材质。
口径：网格为覆盖视口中央的四边形（4 顶点 / 6 索引 / 两个三角形），故「两材质各一段」即两个三角形；
      颜色断言只用「外通道较大者」，不假定回读通道序。
*/
#include "渲染场景测试辅助.h"

namespace
{
	//两帧之间逐通道差达到 threshold 的像素数
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

	//登记一个带反照率色调的材质（tint_override 为真）
	engine::Material_Handle load_tinted_material(engine::Renderer& renderer, float r, float g, float b)
	{
		engine::Material_Data data{};
		data.tint_override = true;
		data.tint[0] = r;
		data.tint[1] = g;
		data.tint[2] = b;
		data.tint[3] = 1.0f;
		engine::Material_Handle material = 0;
		EXPECT_TRUE(renderer.material_load(material, data));
		return material;
	}
}

//索引区间只画该区间：只画前 3 个索引（一个三角形）与整段绘制的画面应有明显差异
TEST_F(Renderer_Scene_Test, 索引区间只画该区间)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//离屏目标（保证每帧附加清屏，避免跨帧残留干扰覆盖统计）
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	renderer.render_target_present_set(scene_target);
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//空场景基线（清屏色；清屏色可能较亮，故覆盖率一律相对基线判定）
	std::vector<uint8_t> empty(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(empty.data(), static_cast<uint32_t>(empty.size())));

	//整段
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> full(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(full.data(), static_cast<uint32_t>(full.size())));

	//仅前 3 个索引（第一个三角形）
	ASSERT_TRUE(renderer.mesh_draw_range(mesh, nullptr, nullptr, 0u, 3u));
	std::vector<uint8_t> half(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(half.data(), static_cast<uint32_t>(half.size())));

	//相对基线的覆盖像素数（与空场景同清屏色，故清屏色不会误计）
	auto covered = [&](const std::vector<uint8_t>& pixels)
	{
		uint32_t count = 0;
		for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
		{
			for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
			{
				std::array<uint8_t, 4> a = pixel_get(pixels, x, y);
				std::array<uint8_t, 4> b = pixel_get(empty, x, y);
				int maximum = 0;
				for (std::size_t index = 0; index < 3; ++index)
				{
					int delta = static_cast<int>(a[index]) - static_cast<int>(b[index]);
					if (delta < 0)
						delta = -delta;
					if (delta > maximum)
						maximum = delta;
				}
				if (maximum >= 20)
					++count;
			}
		}
		return count;
	};
	uint32_t full_lit = covered(full);
	uint32_t half_lit = covered(half);
	RecordProperty("range_full_lit", static_cast<int>(full_lit));
	RecordProperty("range_half_lit", static_cast<int>(half_lit));
	EXPECT_GT(full_lit, 200u) << "整段未覆盖视口（覆盖像素 " << full_lit << "）";
	EXPECT_LT(half_lit, full_lit * 4 / 5) << "区间绘制未只画该区间（半段 " << half_lit
		<< " / 整段 " << full_lit << "）";
	EXPECT_GT(half_lit, full_lit / 4) << "半段几乎未绘制（半段 " << half_lit << " / 整段 " << full_lit << "）";

	renderer.mesh_unload(mesh);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(texture);
}

//索引区间越界被拒绝：返回假，且不登记任何绘制项（画面与空场景一致）
TEST_F(Renderer_Scene_Test, 索引区间越界被拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//离屏目标（保证每帧附加清屏，避免跨帧残留干扰覆盖统计）
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	renderer.render_target_present_set(scene_target);
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//空场景基线
	std::vector<uint8_t> empty(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(empty.data(), static_cast<uint32_t>(empty.size())));

	//首索引超出总数 / 区间长度溢出：两次都应被拒绝
	EXPECT_FALSE(renderer.mesh_draw_range(mesh, nullptr, nullptr, 6u, 3u));
	EXPECT_FALSE(renderer.mesh_draw_range(mesh, nullptr, nullptr, 4u, 4u));
	std::vector<uint8_t> rejected(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(rejected.data(), static_cast<uint32_t>(rejected.size())));
	int difference = 0;
	for (std::size_t index = 0; index < empty.size(); ++index)
	{
		int delta = static_cast<int>(empty[index]) - static_cast<int>(rejected[index]);
		if (delta < 0)
			delta = -delta;
		if (delta > difference)
			difference = delta;
	}
	RecordProperty("range_rejected_max_diff", difference);
	EXPECT_LE(difference, 2) << "越界区间未被拒绝（最大差 " << difference << "）";

	renderer.mesh_unload(mesh);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(texture);
}

/*
多材质分段使用两个材质（第四百一十轮）
口径：两个三角形各用一段材质（红 / 蓝）；画面左半与右半的外通道应相反（一侧偏红、一侧偏蓝）。
*/
TEST_F(Renderer_Scene_Test, 多材质分段使用两个材质)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//离屏目标（保证每帧附加清屏，避免跨帧残留干扰覆盖统计）
	uint64_t scene_target = 0;
	ASSERT_TRUE(renderer.render_target_create(scene_target, renderer.extent_width(), renderer.extent_height()));
	renderer.render_target_present_set(scene_target);
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	engine::Material_Handle red = load_tinted_material(renderer, 1.0f, 0.1f, 0.1f);
	engine::Material_Handle blue = load_tinted_material(renderer, 0.1f, 0.1f, 1.0f);
	//空场景基线（清屏色）
	std::vector<uint8_t> empty(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(empty.data(), static_cast<uint32_t>(empty.size())));
	//分段绘制（两段各一个三角形，材质红 / 蓝）
	engine::Material_Handle materials[2] = { red, blue };
	ASSERT_TRUE(renderer.mesh_draw_sections(mesh, nullptr, materials, 2));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));

	//统计「相对基线有变化且红倾向 / 蓝倾向」的像素数（清屏像素不计入）
	uint32_t red_pixels = 0;
	uint32_t blue_pixels = 0;
	uint32_t changed_pixels = 0;
	for (uint32_t y = 0; y < WINDOW_HEIGHT; ++y)
	{
		for (uint32_t x = 0; x < WINDOW_WIDTH; ++x)
		{
			std::array<uint8_t, 4> value = pixel_get(pixels, x, y);
			std::array<uint8_t, 4> base = pixel_get(empty, x, y);
			//与基线差异（判定该像素被几何覆盖）
			int maximum = 0;
			for (std::size_t index = 0; index < 3; ++index)
			{
				int delta = static_cast<int>(value[index]) - static_cast<int>(base[index]);
				if (delta < 0)
					delta = -delta;
				if (delta > maximum)
					maximum = delta;
			}
			if (maximum < 20)
				continue;
			++changed_pixels;
			//红倾向：R 明显高于 B；蓝倾向反之
			if (static_cast<int>(value[0]) - static_cast<int>(value[2]) >= 20)
				++red_pixels;
			else if (static_cast<int>(value[2]) - static_cast<int>(value[0]) >= 20)
				++blue_pixels;
		}
	}
	RecordProperty("sections_changed_pixels", static_cast<int>(changed_pixels));
	RecordProperty("sections_red_pixels", static_cast<int>(red_pixels));
	RecordProperty("sections_blue_pixels", static_cast<int>(blue_pixels));
	EXPECT_GT(red_pixels, 20u) << "红段未绘制";
	EXPECT_GT(blue_pixels, 20u) << "蓝段未绘制（第二段材质未生效）";


	renderer.material_unload(red);
	renderer.material_unload(blue);
	renderer.mesh_unload(mesh);
	renderer.render_target_destroy(scene_target);
	renderer.texture_unload(texture);
}

/*
多材质分段的结构性开销（第四百一十三轮）
口径：一次 mesh_draw_sections(2) 应登记 2 个绘制项、写 2 × 实例步长（112 字节）的实例数据；
      用确定性指标（绘制项数 / 实例上传字节）而非 GPU 耗时（64×36 下噪声主导）。
*/
TEST_F(Renderer_Scene_Test, 多材质分段绘制项数与实例字节)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	engine::Material_Handle red = load_tinted_material(renderer, 1.0f, 0.1f, 0.1f);
	engine::Material_Handle blue = load_tinted_material(renderer, 0.1f, 0.1f, 1.0f);
	engine::Material_Handle materials[2] = { red, blue };
	//首个帧槽的实例上传增量（本帧两段各一个实例）
	uint64_t before = renderer.instance_upload_bytes();
	ASSERT_TRUE(renderer.mesh_draw_sections(mesh, nullptr, materials, 2));
	ASSERT_TRUE(renderer.frame_render());
	uint64_t after = renderer.instance_upload_bytes();
	engine::Frame_Stats stats = renderer.frame_stats_get();
	RecordProperty("sections_draw_items", static_cast<int>(stats.last_draw_items));
	RecordProperty("sections_instance_bytes", static_cast<int>(after - before));
	EXPECT_EQ(stats.last_draw_items, 2u) << "两段应登记两个绘制项";
	EXPECT_EQ(after - before, 2u * 112u) << "两段实例数据应为 2 × 112 字节";

	renderer.material_unload(red);
	renderer.material_unload(blue);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

/*
网格分段按描述绘制（第四百一十四轮）
口径：同一四边形 6 索引两个三角形；「均分」帧（无 sections）为第一三角形红、第二蓝；
      「分段」帧的 sections 把材质槽反向（段 0 → 蓝、段 1 → 红）；
      两帧颜色应互换（差异像素 > 100），证明分段分支按段描述取材质而非均分。
*/
TEST_F(Renderer_Scene_Test, 网格分段按描述绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_white();
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);
	engine::Material_Handle red = load_tinted_material(renderer, 1.0f, 0.1f, 0.1f);
	engine::Material_Handle blue = load_tinted_material(renderer, 0.1f, 0.1f, 1.0f);
	engine::Material_Handle materials[2] = { red, blue };

	//均分帧（网格无分段）
	engine::Mesh_Handle plain_mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(plain_mesh, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw_sections(plain_mesh, nullptr, materials, 2));
	std::vector<uint8_t> even(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(even.data(), static_cast<uint32_t>(even.size())));

	//分段帧（段 0 → 材质 1 蓝、段 1 → 材质 0 红）
	engine::Mesh_Data sectioned = quad_data();
	const engine::Mesh_Section 段描述[2] = { { 0u, 3u, 1u }, { 3u, 3u, 0u } };
	engine::Mesh_Handle section_mesh = 0;
	ASSERT_TRUE(renderer.mesh_load_sections(section_mesh, sectioned, 段描述, 2));
	ASSERT_TRUE(renderer.mesh_draw_sections(section_mesh, nullptr, materials, 2));
	std::vector<uint8_t> swapped(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(swapped.data(), static_cast<uint32_t>(swapped.size())));

	uint32_t changes = differing_count(even, swapped, 20);
	RecordProperty("section_described_changed_pixels", static_cast<int>(changes));
	EXPECT_GT(changes, 100u) << "分段描述未生效（两帧差异仅 " << changes << " 像素）";

	renderer.mesh_unload(plain_mesh);
	renderer.mesh_unload(section_mesh);
	renderer.material_unload(red);
	renderer.material_unload(blue);
	renderer.texture_unload(texture);
}

//分段区间越界（first 4 / count 3 超过 6 个索引）应在网格创建时被拒绝
TEST_F(Renderer_Scene_Test, 网格分段越界被拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Data bad = quad_data();
	const engine::Mesh_Section 段描述[1] = { { 4u, 3u, 0u } };
	engine::Mesh_Handle mesh = 0;
	EXPECT_FALSE(renderer.mesh_load_sections(mesh, bad, 段描述, 1)) << "越界分段未被拒绝";
	EXPECT_EQ(mesh, 0u);
}

//分段索引数非 3 的倍数（2）应在网格创建时被拒绝
TEST_F(Renderer_Scene_Test, 网格分段非三角形被拒绝)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Data bad = quad_data();
	const engine::Mesh_Section 段描述[1] = { { 0u, 2u, 0u } };
	engine::Mesh_Handle mesh = 0;
	EXPECT_FALSE(renderer.mesh_load_sections(mesh, bad, 段描述, 1)) << "非三角形分段未被拒绝";
	EXPECT_EQ(mesh, 0u);
}

/*
实例化上传字节按实例步长计算（第四百一十五轮 · 方向四性能基线）
口径：200 个实例（默认容量 256 内）首次登记后，首帧槽上传应为 200 × 112 字节；
      用确定性指标而非 GPU 耗时（64×36 下噪声主导）。
*/
TEST_F(Renderer_Scene_Test, 实例化上传字节按步长计算)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//200 个沿 X 轴递增平移的实例
	std::vector<float> transforms(200u * 16u, 0.0f);
	for (uint32_t index = 0; index < 200u; ++index)
	{
		float* matrix = transforms.data() + index * 16u;
		matrix[0] = 1.0f;
		matrix[5] = 1.0f;
		matrix[10] = 1.0f;
		matrix[12] = static_cast<float>(index) * 0.02f - 2.0f;
		matrix[15] = 1.0f;
	}
	uint64_t before = renderer.instance_upload_bytes();
	ASSERT_TRUE(renderer.mesh_draw_instanced(mesh, transforms.data(), 200u));
	ASSERT_TRUE(renderer.frame_render());
	uint64_t after = renderer.instance_upload_bytes();
	RecordProperty("instanced_upload_bytes", static_cast<int>(after - before));
	EXPECT_EQ(after - before, 200u * 112u) << "实例上传字节与步长不符";

	renderer.mesh_unload(mesh);
}
