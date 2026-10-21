/*
渲染离屏目标测试（L2 隐藏窗口 + 像素回读）
覆盖：渲染目标的创建 / 销毁；把场景渲染到离屏目标并回读目标内容；
      以目标作纹理采样做「离屏 → 采样合成」闭环；无效目标句柄回落交换链路径。
断言一律「格式无关」：绿判定只依赖绿通道（RGBA 与 BGRA 下恒为第 2 字节，下标 1），
      R / B 通道序随表面格式互换，故用「外通道取较大者」判定。
纯绿（0 与 1 两种端点）在 UNORM 与 sRGB 附件下编码一致，故判定不随附件传输函数变化。
*/
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//获取渲染器门面（含混合档枚举）与网格数据
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

	//外通道较大者（R 与 B 通道；避开 std::max —— 本层包含链有 min/max 宏）
	uint8_t outer_max(const std::array<uint8_t, 4>& value)
	{
		return value[0] > value[2] ? value[0] : value[2];
	}

	//判断像素是否近似纯绿（绿通道高、外通道低）
	bool pixel_is_green(const std::array<uint8_t, 4>& value)
	{
		//绿通道（下标 1）须为高
		if (value[1] < 200)
			return false;
		//外通道须为低
		if (outer_max(value) > 60)
			return false;
		return true;
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

	//构造纯绿纹理（RGBA8：rgb = 绿，alpha = 255）
	std::vector<uint8_t> solid_green(uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
		for (std::size_t index = 0; index < pixels.size(); index += 4)
		{
			pixels[index] = 0;
			pixels[index + 1] = 255;
			pixels[index + 2] = 0;
			pixels[index + 3] = 255;
		}
		return pixels;
	}

	/*
	构造「铺满全屏」模型矩阵（列主序）
	x / y 各放大 4 倍，使四边形在 x / y 上越过 NDC 边界被裁剪，恰好铺满整个渲染目标；
	放大倍数足够大，故铺满效果不依赖相机视场角的具体取值。
	*/
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

//渲染离屏目标测试夹具
class Renderer_Offscreen_Test : public ::testing::Test
{
};

//未初始化时创建渲染目标失败
TEST_F(Renderer_Offscreen_Test, 未初始化创建目标失败)
{
	engine::Renderer renderer;
	//未 init 直接创建
	uint64_t handle = 7;
	EXPECT_FALSE(renderer.render_target_create(handle, 32, 32));
	//句柄须被复位为 0（无效）
	EXPECT_EQ(handle, 0u);
}

//渲染目标的创建与销毁幂等
TEST_F(Renderer_Offscreen_Test, 创建销毁目标幂等)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//首次创建
	uint64_t first = 0;
	ASSERT_TRUE(renderer.render_target_create(first, 32, 32));
	EXPECT_GE(first, 1u);
	//销毁后再销毁（幂等）不崩溃
	renderer.render_target_destroy(first);
	renderer.render_target_destroy(first);
	//槽可复用：再次创建成功
	uint64_t second = 0;
	ASSERT_TRUE(renderer.render_target_create(second, 32, 32));
	EXPECT_GE(second, 1u);
	renderer.render_target_destroy(second);
}

//把场景渲染到离屏目标并回读目标内容
TEST_F(Renderer_Offscreen_Test, 渲染到目标并回读)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//若本机不支持像素回读则跳过
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建与窗口同尺寸的渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));

	//本帧渲染到目标
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	//回读来源为目标颜色附件
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//目标内容应为纯绿（铺满全屏）——证明离屏渲染确实发生
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "离屏目标中心非纯绿：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2)))
		<< "离屏目标左上角非纯绿：" << pixel_text(pixel_get(pixels, 2, 2));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH - 3, WINDOW_HEIGHT - 3)))
		<< "离屏目标右下角非纯绿：" << pixel_text(pixel_get(pixels, WINDOW_WIDTH - 3, WINDOW_HEIGHT - 3));

	//渲染到目标不改变交换链路径的可用性：切回交换链再渲染一帧应正常
	renderer.render_target_set(0);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	std::vector<uint8_t> swapchain_pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(swapchain_pixels.data(), static_cast<uint32_t>(swapchain_pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(swapchain_pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "切回交换链路径后中心非纯绿";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

//以目标作纹理采样：完成「离屏渲染 → 采样合成」闭环
TEST_F(Renderer_Offscreen_Test, 目标可作纹理采样闭环)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//创建渲染目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();

	//第一帧：把纯绿场景渲染进离屏目标（不呈现、不回读）
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	/*
	第二帧：切回交换链并把目标颜色绑定为当前纹理
	先显式绑定内置默认白纹理，使「采样失败」会呈现为白（而非恰好同为绿），
	故本用例能真正区分「采样到目标」与「回落默认纹理」。
	*/
	renderer.render_target_set(0);
	renderer.texture_bind_set(0);
	renderer.texture_bind_target(target);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	//交换链内容应为目标的绿（若采样失败则回落为默认白，绿通道判定会失败）
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_green(center)) << "采样合成后中心非目标绿（疑似回落默认纹理）：" << pixel_text(center);
	//角落同样为目标的绿（全屏铺满）
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2))) << "采样合成后角落非目标绿";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

//无效渲染目标句柄回落交换链路径
TEST_F(Renderer_Offscreen_Test, 无效目标句柄回落交换链)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	//设定一个从未创建的渲染目标句柄
	renderer.render_target_set(9999u);
	//加载纯绿纹理与四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	renderer.texture_bind_set(texture);
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	//应回落交换链路径：回读到交换链的绿
	std::vector<uint8_t> pixels(pixel_bytes());
	ASSERT_TRUE(renderer.frame_render(pixels.data(), static_cast<uint32_t>(pixels.size())));
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2)))
		<< "无效目标句柄未回落交换链路径";

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
}

/*
离屏目标读回（第二十六轮）：render_target_readback 直接从目标颜色图像取像素
口径：先把绿场景渲染进目标（frame_render 不回读），再读回目标内容；
      应得到与目标尺寸一致的绿；连续两次读回结果一致（读回不破坏内容与布局）。
*/
TEST_F(Renderer_Offscreen_Test, 读回离屏目标内容)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//创建与窗口同尺寸的目标
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	//绿纹理与全屏四边形
	uint64_t texture = 0;
	std::vector<uint8_t> texels = solid_green(2, 2);
	ASSERT_TRUE(renderer.texture_load(texture, texels.data(), 2, 2));
	engine::Mesh_Handle handle = 0;
	ASSERT_TRUE(renderer.mesh_load(handle, quad_data()));
	std::array<float, 16> full = full_screen_matrix();
	renderer.render_target_set(target);
	renderer.texture_bind_set(texture);
	ASSERT_TRUE(renderer.mesh_draw(handle, full.data()));
	ASSERT_TRUE(renderer.frame_render());

	//读回目标内容
	std::vector<uint8_t> pixels(pixel_bytes());
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.render_target_readback(target, pixels.data(),
		static_cast<uint32_t>(pixels.size()), &width, &height));
	EXPECT_EQ(width, WINDOW_WIDTH);
	EXPECT_EQ(height, WINDOW_HEIGHT);
	std::array<uint8_t, 4> center = pixel_get(pixels, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	EXPECT_TRUE(pixel_is_green(center)) << "目标读回中心非纯绿：" << pixel_text(center);
	EXPECT_TRUE(pixel_is_green(pixel_get(pixels, 2, 2))) << "目标读回角落非纯绿";

	//第二次读回：内容不因读回而改变
	std::vector<uint8_t> again(pixel_bytes());
	ASSERT_TRUE(renderer.render_target_readback(target, again.data(), static_cast<uint32_t>(again.size()), nullptr, nullptr));
	EXPECT_EQ(pixels, again);

	renderer.mesh_unload(handle);
	renderer.texture_unload(texture);
	renderer.render_target_destroy(target);
}

/*
离屏目标读回的参数校验（第二十六轮）：未初始化 / 句柄无效 / 容量不足均返回假
*/
TEST_F(Renderer_Offscreen_Test, 读回参数校验)
{
	engine::Renderer renderer;
	std::vector<uint8_t> pixels(64 * 36 * 4);
	//未初始化
	EXPECT_FALSE(renderer.render_target_readback(1, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	ASSERT_TRUE(renderer.init(hidden_config()));
	//句柄无效（从未创建）
	EXPECT_FALSE(renderer.render_target_readback(9999u, pixels.data(), static_cast<uint32_t>(pixels.size()), nullptr, nullptr));
	//容量不足
	uint64_t target = 0;
	ASSERT_TRUE(renderer.render_target_create(target, WINDOW_WIDTH, WINDOW_HEIGHT));
	uint32_t width = 123;
	uint32_t height = 456;
	EXPECT_FALSE(renderer.render_target_readback(target, pixels.data(), 16, &width, &height));
	//失败时尺寸出参保持 0
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
	renderer.render_target_destroy(target);
}
