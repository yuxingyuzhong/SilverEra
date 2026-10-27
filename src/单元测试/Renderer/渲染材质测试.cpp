/*
渲染材质测试（L2 隐藏窗口 + 像素回读）
覆盖：未初始化登记材质失败；登记 / 卸载材质幂等与句柄槽复用；无效句柄回落默认；
      非不透明混合档材质自动登记具名管线；
      核心验收：同一网格以不同材质绘制产生可区分的像素结果。
断言一律「格式无关」：绿通道恒为字节下标 1（RGBA 与 BGRA 下均然），
      R / B 通道序随表面格式互换，故只用「外通道取较大者 / 是否恰有一高」判定，不假定具体通道序。
*/
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含材质 / 混合档枚举）与网格数据
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//窗口尺寸（与回读断言配套）
	constexpr uint32_t WINDOW_WIDTH = 64;
	constexpr uint32_t WINDOW_HEIGHT = 36;

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

	//取指定像素的四个字节
	std::array<uint8_t, 4> pixel_get(const std::vector<uint8_t>& pixels, uint32_t x, uint32_t y)
	{
		//行内偏移（每行宽 × 4 字节）
		std::size_t offset = (static_cast<std::size_t>(y) * WINDOW_WIDTH + x) * 4;
		std::array<uint8_t, 4> value{};
		for (std::size_t index = 0; index < 4; ++index)
			value[index] = pixels[offset + index];
		return value;
	}

	//失败诊断文本
	std::string pixel_text(const std::array<uint8_t, 4>& value)
	{
		return std::to_string(value[0]) + "/" + std::to_string(value[1]) + "/"
			+ std::to_string(value[2]) + "/" + std::to_string(value[3]);
	}

	//外通道是否「恰有一个饱和」（R 与 B 通道；避开 std::max —— 本层包含链有 min/max 宏）
	bool outer_saturated(const std::array<uint8_t, 4>& value)
	{
		//R / B 通道是否各自饱和
		bool red_high = value[0] >= 200;
		bool blue_high = value[2] >= 200;
		//恰有一个饱和即为真
		return red_high != blue_high;
	}

	//构造覆盖视口中央的四边形（NDC 尺度 1，位于 z = 0；默认相机在 +Z 3 单位处）
	engine::Mesh_Data quad_data(void)
	{
		engine::Mesh_Data data;
		data.vertices = {
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
			{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		};
		data.indices = { 0, 1, 2, 0, 2, 3 };
		return data;
	}

	//构造纯色纹理（RGBA8：rgb = 给定色，alpha = 255）
	std::vector<uint8_t> solid_color(uint8_t r, uint8_t g, uint8_t b)
	{
		std::vector<uint8_t> pixels(4 * 4 * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = r;
			pixels[index + 1] = g;
			pixels[index + 2] = b;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	//构造「铺满全屏」模型矩阵（列主序；放大 4 倍越过 NDC 边界被裁剪）
	std::array<float, 16> full_screen_matrix(void)
	{
		return {
			4.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 4.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	}
}

//渲染材质测试夹具
class Renderer_Material_Test : public ::testing::Test
{
};

//未初始化时登记材质失败，句柄保持无效
TEST_F(Renderer_Material_Test, 未初始化登记材质失败)
{
	engine::Renderer renderer;
	engine::Material_Handle handle = 9;
	engine::Material_Data data{};
	EXPECT_FALSE(renderer.material_load(handle, data));
	//句柄须被复位为 0（无效）
	EXPECT_EQ(handle, 0u);
}

//登记与卸载材质：句柄有效、卸载幂等、槽可复用
TEST_F(Renderer_Material_Test, 登记卸载材质幂等)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//主色纹理
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_color(255, 0, 0);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
	//登记不透明材质（沿用默认场景管线）
	engine::Material_Data data{};
	data.texture = texture;
	engine::Material_Handle first = 0;
	ASSERT_TRUE(renderer.material_load(first, data));
	EXPECT_GE(first, 1u);
	//重复卸载幂等
	renderer.material_unload(first);
	renderer.material_unload(first);
	//槽可复用：再次登记成功
	engine::Material_Handle second = 0;
	ASSERT_TRUE(renderer.material_load(second, data));
	EXPECT_GE(second, 1u);
	renderer.material_unload(second);
	renderer.texture_unload(texture);
}

//非不透明混合档材质自动登记具名管线并可正常绘制
TEST_F(Renderer_Material_Test, 混合档材质登记并绘制)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_color(0, 255, 0);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//登记 Alpha 混合档材质（门面按默认场景着色器对登记具名管线）
	engine::Material_Data data{};
	data.texture = texture;
	data.blend = engine::Blend_Mode::Alpha;
	engine::Material_Handle material = 0;
	ASSERT_TRUE(renderer.material_load(material, data));
	ASSERT_GE(material, 1u);
	//绑定材质并绘制一帧
	renderer.material_bind_set(material);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	ASSERT_TRUE(renderer.frame_render());
	//卸载材质（其自有的具名管线随之释放）
	renderer.material_unload(material);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(texture);
}

//无效材质句柄回落默认：绑定后仍可正常绘制一帧
TEST_F(Renderer_Material_Test, 无效材质句柄回落默认)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//从未登记过的句柄
	renderer.material_bind_set(9999u);
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	ASSERT_TRUE(renderer.frame_render());
	renderer.mesh_unload(mesh);
}

/*
核心验收：同一网格以不同材质绘制产生可区分的像素结果
两材质共享同一网格与同一管线，仅纹理不同；分别绘制两帧并回读，
断言：① 两帧均被纹理着色（非回落默认白纹理）；② 两帧的颜色不同（可区分）。
*/
TEST_F(Renderer_Material_Test, 同一网格不同材质像素可区分)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//同一网格
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//两材质：红 / 蓝纹理，其余一致
	uint64_t red_texture = 0;
	std::vector<uint8_t> red_texels = solid_color(255, 0, 0);
	ASSERT_TRUE(renderer.texture_load(red_texture, red_texels.data(), 4, 4));
	uint64_t blue_texture = 0;
	std::vector<uint8_t> blue_texels = solid_color(0, 0, 255);
	ASSERT_TRUE(renderer.texture_load(blue_texture, blue_texels.data(), 4, 4));
	engine::Material_Data red_data{};
	red_data.texture = red_texture;
	engine::Material_Handle red_material = 0;
	ASSERT_TRUE(renderer.material_load(red_material, red_data));
	engine::Material_Data blue_data{};
	blue_data.texture = blue_texture;
	engine::Material_Handle blue_material = 0;
	ASSERT_TRUE(renderer.material_load(blue_material, blue_data));

	//第一帧：绑红材质并铺满全屏绘制
	renderer.material_bind_set(red_material);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> red_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(red_pixels.data(), static_cast<uint32_t>(red_pixels.size())));
	std::array<uint8_t, 4> red_center = pixel_get(red_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

	//第二帧：绑蓝材质并铺满全屏绘制
	renderer.material_bind_set(blue_material);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
	std::vector<uint8_t> blue_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(blue_pixels.data(), static_cast<uint32_t>(blue_pixels.size())));
	std::array<uint8_t, 4> blue_center = pixel_get(blue_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

	//两帧均被纹理着色（非回落默认白纹理：白纹理下绿通道会饱和）
	EXPECT_LT(red_center[1], 60) << "红材质帧疑似回落默认白纹理：" << pixel_text(red_center);
	EXPECT_LT(blue_center[1], 60) << "蓝材质帧疑似回落默认白纹理：" << pixel_text(blue_center);
	//两帧外通道均饱和（确为单一外色，而非清屏色）
	EXPECT_TRUE(outer_saturated(red_center)) << "红材质帧非饱和外色：" << pixel_text(red_center);
	EXPECT_TRUE(outer_saturated(blue_center)) << "蓝材质帧非饱和外色：" << pixel_text(blue_center);
	//两帧像素可区分（外通道首字节不同；通道序无关）
	EXPECT_NE(red_center[0], blue_center[0])
		<< "两材质像素不可区分：红 " << pixel_text(red_center) << " 蓝 " << pixel_text(blue_center);
	//整帧数据亦不同（强化「可区分」）
	EXPECT_NE(red_pixels, blue_pixels);

	renderer.material_unload(red_material);
	renderer.material_unload(blue_material);
	renderer.mesh_unload(mesh);
	renderer.texture_unload(red_texture);
	renderer.texture_unload(blue_texture);
}

/*
同一混合档的多个材质共享该档的具名管线
具名管线所有权归门面（随关闭统一释放），故卸载其一不应影响另一：
本例在卸载 first 后仍以 second 绑定并绘制成功。
*/
TEST_F(Renderer_Material_Test, 同混合档材质共享管线)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_color(0, 255, 0);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, quad_data()));
	//两条同一（Alpha）混合档的材质
	engine::Material_Data data{};
	data.texture = texture;
	data.blend = engine::Blend_Mode::Alpha;
	engine::Material_Handle first = 0;
	engine::Material_Handle second = 0;
	ASSERT_TRUE(renderer.material_load(first, data));
	ASSERT_TRUE(renderer.material_load(second, data));
	EXPECT_NE(first, second);
	//卸载其一
	renderer.material_unload(first);
	//另一仍可绑定并绘制一帧（共享的具名管线未随卸载释放）
	renderer.material_bind_set(second);
	ASSERT_TRUE(renderer.mesh_draw(mesh, full_screen_matrix().data()));
        ASSERT_TRUE(renderer.frame_render());
        renderer.material_unload(second);
        renderer.mesh_unload(mesh);
        renderer.texture_unload(texture);
}

/*
逐物体 PBR 覆盖字段（第六十八轮）
口径：Material_Data 声明 pbr_override 且参数合法时，绑定后门面吸附状态与材质一致；
      参数非法（粗糙度 0 / 9、金属度 1.5、非数）按「不覆盖」登记；
      绑定句柄 0 清除覆盖；未绑定材质前默认不覆盖。
*/
TEST_F(Renderer_Material_Test, 逐物体PBR覆盖字段)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认不覆盖
	EXPECT_FALSE(renderer.material_pbr_active());
	EXPECT_FLOAT_EQ(renderer.material_pbr_roughness(), 1.0f);
	EXPECT_FLOAT_EQ(renderer.material_pbr_metallic(), 0.0f);

	//合法覆盖材质
	engine::Material_Data data;
	data.pbr_override = true;
	data.pbr_roughness = 2.5f;
	data.pbr_metallic = 0.25f;
	engine::Material_Handle handle = 0;
	ASSERT_TRUE(renderer.material_load(handle, data));
	renderer.material_bind_set(handle);
	EXPECT_TRUE(renderer.material_pbr_active());
	EXPECT_FLOAT_EQ(renderer.material_pbr_roughness(), 2.5f);
	EXPECT_FLOAT_EQ(renderer.material_pbr_metallic(), 0.25f);
	//绑定 0 清除覆盖
	renderer.material_bind_set(0);
	EXPECT_FALSE(renderer.material_pbr_active());

	//非法参数按不覆盖登记
	engine::Material_Data invalid_rough;
	invalid_rough.pbr_override = true;
	invalid_rough.pbr_roughness = 9.0f;
	engine::Material_Handle invalid_handle = 0;
	ASSERT_TRUE(renderer.material_load(invalid_handle, invalid_rough));
	renderer.material_bind_set(invalid_handle);
	EXPECT_FALSE(renderer.material_pbr_active());
	//非数同样视为非法
	engine::Material_Data nan_metallic;
	nan_metallic.pbr_override = true;
	nan_metallic.pbr_metallic = std::nanf("");
	engine::Material_Handle nan_handle = 0;
	ASSERT_TRUE(renderer.material_load(nan_handle, nan_metallic));
	renderer.material_bind_set(nan_handle);
	EXPECT_FALSE(renderer.material_pbr_active());

	renderer.material_unload(nan_handle);
	renderer.material_unload(invalid_handle);
	renderer.material_unload(handle);
}

/*
逐物体色调覆盖字段（第六十九轮）
口径：`Material_Data` 声明 tint_override 且四分量合法时，绑定后门面吸附状态与材质一致；
      分量越界（1.5）按「不覆盖」登记；绑定句柄 0 清除；未绑定时默认不启用且取值为白。
*/
TEST_F(Renderer_Material_Test, 逐物体色调覆盖字段)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认不启用、取值为白
	EXPECT_FALSE(renderer.material_tint_active());
	float white[4] = {};
	renderer.material_tint_get(white);
	for (std::size_t index = 0; index < 4; ++index)
		EXPECT_FLOAT_EQ(white[index], 1.0f);

	//合法色调材质
	engine::Material_Data data;
	data.tint_override = true;
	data.tint[0] = 1.0f;
	data.tint[1] = 0.5f;
	data.tint[2] = 0.25f;
	data.tint[3] = 1.0f;
	engine::Material_Handle handle = 0;
	ASSERT_TRUE(renderer.material_load(handle, data));
	renderer.material_bind_set(handle);
	EXPECT_TRUE(renderer.material_tint_active());
	float actual[4] = {};
	renderer.material_tint_get(actual);
	EXPECT_FLOAT_EQ(actual[1], 0.5f);
	EXPECT_FLOAT_EQ(actual[2], 0.25f);
	//绑定 0 清除
	renderer.material_bind_set(0);
	EXPECT_FALSE(renderer.material_tint_active());

	//非法分量按不覆盖登记
	engine::Material_Data invalid;
	invalid.tint_override = true;
	invalid.tint[0] = 1.5f;
	engine::Material_Handle invalid_handle = 0;
	ASSERT_TRUE(renderer.material_load(invalid_handle, invalid));
	renderer.material_bind_set(invalid_handle);
	EXPECT_FALSE(renderer.material_tint_active());

	renderer.material_unload(invalid_handle);
	renderer.material_unload(handle);
}

/*
逐物体自发光字段（第七十轮）
口径：`Material_Data.emissive` 合法值（2.5）绑定后门面取值一致；越界（9）按 0 登记；
      绑定句柄 0 清除；未绑定时默认 0。
*/
TEST_F(Renderer_Material_Test, 逐物体自发光字段)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//默认 0
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 0.0f);

	//合法自发光材质
	engine::Material_Data data;
	data.emissive = 2.5f;
	engine::Material_Handle handle = 0;
	ASSERT_TRUE(renderer.material_load(handle, data));
	renderer.material_bind_set(handle);
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 2.5f);
	//绑定 0 清除
	renderer.material_bind_set(0);
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 0.0f);

	//越界按 0 登记
	engine::Material_Data invalid;
	invalid.emissive = 9.0f;
	engine::Material_Handle invalid_handle = 0;
	ASSERT_TRUE(renderer.material_load(invalid_handle, invalid));
	renderer.material_bind_set(invalid_handle);
	EXPECT_FLOAT_EQ(renderer.material_emissive(), 0.0f);

	renderer.material_unload(invalid_handle);
	renderer.material_unload(handle);
}

/*
双面渲染开关（第一百七十二轮）
口径：四边形索引反转为背面；默认材质被背面剔除（中心为清屏色），
     double_sided 材质关闭剔除后照常着色（中心为白色）。
*/
TEST_F(Renderer_Material_Test, 双面渲染使背面可见)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_color(255, 255, 255);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 4, 4));
	//背面网格（索引反转为顺时针，正面变背面）
	engine::Mesh_Data data = quad_data();
	data.indices = { 0, 2, 1, 0, 3, 2 };
	engine::Mesh_Handle mesh = 0;
	ASSERT_TRUE(renderer.mesh_load(mesh, data));
	engine::Camera_Data camera{};
	engine::Light_Data light{};
	engine::Fog_Data fog{};
	renderer.scene_apply(camera, light, fog);

	//默认材质（背面剔除）
	engine::Material_Data single{};
	single.texture = texture;
	engine::Material_Handle single_handle = 0;
	ASSERT_TRUE(renderer.material_load(single_handle, single));
	renderer.material_bind_set(single_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> single_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(single_pixels.data(), static_cast<uint32_t>(single_pixels.size())));

	//双面材质（关闭背面剔除）
	engine::Material_Data double_sided{};
	double_sided.texture = texture;
	double_sided.double_sided = true;
	engine::Material_Handle double_handle = 0;
	ASSERT_TRUE(renderer.material_load(double_handle, double_sided));
	renderer.material_bind_set(double_handle);
	ASSERT_TRUE(renderer.mesh_draw(mesh, nullptr));
	std::vector<uint8_t> double_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(double_pixels.data(), static_cast<uint32_t>(double_pixels.size())));

	const std::array<uint8_t, 4> single_center = pixel_get(single_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	const std::array<uint8_t, 4> double_center = pixel_get(double_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	//外通道较大者（R 与 B；避开通道序）
	const uint8_t single_outer = single_center[0] > single_center[2] ? single_center[0] : single_center[2];
	const uint8_t double_outer = double_center[0] > double_center[2] ? double_center[0] : double_center[2];
	RecordProperty("double_sided_single_outer", static_cast<uint32_t>(single_outer));
	RecordProperty("double_sided_double_outer", static_cast<uint32_t>(double_outer));

	EXPECT_LE(single_outer, 150u) << "背面未被剔除（" << pixel_text(single_center) << "）";
	EXPECT_GE(double_outer, 200u) << "双面渲染未使背面可见（" << pixel_text(double_center) << "）";

	renderer.mesh_unload(mesh);
	renderer.material_unload(single_handle);
	renderer.material_unload(double_handle);
	renderer.texture_unload(texture);
}
