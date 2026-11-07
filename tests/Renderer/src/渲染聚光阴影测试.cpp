/*
渲染聚光阴影测试（第五百六十二轮 · 由 渲染关卡测试.cpp 迁出）
覆盖：八盏聚光灯阴影（前向 / Cluster 路径）、质量参数组合、偏置自校准（关卡行 / 门面）、按需重绘、
分辨率切换、软度开销、与方向光共存、MSAA 组合、逐瓦片互异性（前向 ＋ 簇路径）、点光上限边界。
口径：只移动代码、不改断言；套件名保持 Renderer_Level_Test；临时文件写在 out/关卡测试/。
*/
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Engine/EngineSystem/src/scene/Level/关卡文本.h"
#include "Engine/EngineRenderer/src/render/Scene/场景装配.h"
#include "Engine/EngineRenderer/src/render/Renderer/渲染器.h"

namespace
{

	//端到端窗口尺寸
	constexpr uint32_t LEVEL_WIDTH = 128;
	constexpr uint32_t LEVEL_HEIGHT = 72;

	//临时文件目录（宽字符；相对可执行文件目录）
	std::filesystem::path level_dir(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"关卡测试");
	}

	//关卡文件的 UTF-8 窄路径（供读写入参）
	std::string level_utf8(const char* name)
	{
		return std::string("out/关卡测试/") + name;
	}

	//写一个 UTF-8 文本文件（目录自动创建；宽路径写盘）
	void write_text(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//两帧之间逐通道差 ≥ 6 的像素数
	uint32_t changed_pixels(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
	{
		uint32_t count = 0;
		for (std::size_t index = 0; index + 3 < left.size(); index += 4)
		{
			int maximum = 0;
			for (std::size_t channel = 0; channel < 3; ++channel)
			{
				int delta = static_cast<int>(left[index + channel]) - static_cast<int>(right[index + channel]);
				if (delta < 0)
					delta = -delta;
				if (maximum < delta)
					maximum = delta;
			}
			if (maximum >= 6)
				++count;
		}
		return count;
	}

	/*
	写立方体几何 ＋ 单材质（第四百七十二轮；卡通用例需要多变法线）
	六个面各自独立法线，供逐物体卡通色阶产生可见差异；正面 / 右侧 / 顶面按外法线逆时针绕序。
	*/
	void write_cube_assets(void)
	{
		write_text(level_dir() / L"cube.obj",
			"v -1 -1 -1\n"
			"v 1 -1 -1\n"
			"v 1 1 -1\n"
			"v -1 1 -1\n"
			"v -1 -1 1\n"
			"v 1 -1 1\n"
			"v 1 1 1\n"
			"v -1 1 1\n"
			"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
			"vn 0 0 -1\nvn 0 0 1\nvn -1 0 0\nvn 1 0 0\nvn 0 -1 0\nvn 0 1 0\n"
			"f 5/1/2 6/2/2 7/3/2\n"
			"f 5/1/2 7/3/2 8/4/2\n"
			"f 6/1/4 7/2/4 3/3/4\n"
			"f 6/1/4 3/3/4 2/4/4\n"
			"f 8/1/6 7/2/6 3/3/6\n"
			"f 8/1/6 3/3/6 4/4/6\n"
			"f 2/1/1 1/2/1 4/3/1\n"
			"f 2/1/1 4/3/1 3/4/1\n"
			"f 5/1/3 1/2/3 4/3/3\n"
			"f 5/1/3 4/3/3 8/4/3\n"
			"f 1/1/5 2/2/5 6/3/5\n"
			"f 1/1/5 6/3/5 5/4/5\n");
		write_text(level_dir() / L"cube.mtl", "newmtl 甲\nKd 0.7 0.5 0.4\nNs 64\n");
	}

	/*
	八盏聚光关卡（第五百一十五轮抽出）
	八盏聚光各照一块地面与遮挡体；无其它前向灯，确保八盏全部登记（前向光源上限 8 盏）。
	*/
	void write_eight_spot_level(void)
	{
		write_text(level_dir() / L"八盏聚光.level",
			"level 八盏聚光\n"
			"camera position 0 8 10\n"
			"camera target 0 0 0\n"
			"light direction 0 -1 0\n"
			"light color 0.1 0.1 0.1\n"
			"light intensity 0.1\n"
			"light spot -7 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -5 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot -1 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 1 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 3 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 5 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"light spot 7 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
			"mesh 方块 cube.obj\n"
			"mtl 方块材质 cube.mtl\n"
			"place 方块 方块材质 0 -1.5 0 24 0.2 10\n"
			"place 方块 方块材质 -7 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -5 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -3 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 -1 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 1 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 3 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 5 0.6 0 1.2 1.2 1.2\n"
			"place 方块 方块材质 7 0.6 0 1.2 1.2 1.2\n");
	}
}

/*
Cluster 路径八盏聚光阴影（第五百二十五轮）
口径：八盏关卡在 Cluster 开启下，关闭 / 开启八盏阴影差异 > 100 像素且瓦片数为 8；
随后关闭 Cluster 再出图，应与此前 Cluster 开启的有影图逐字节一致（两条路径同结果）。
*/
TEST(Renderer_Level_Test, 关卡Cluster八盏聚光阴影)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	if (!renderer.cluster_supported())
		GTEST_SKIP() << "本机不支持 Cluster 计算路径";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);

	//Cluster 开启：关闭聚光阴影出参照图
	ASSERT_TRUE(renderer.cluster_enable_set(true));
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

	//Cluster 开启：八盏全投影
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 簇有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(簇有影.data(), static_cast<uint32_t>(簇有影.size())));
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);
	const uint32_t 簇差异 = changed_pixels(无影, 簇有影);
	std::cout << "[关卡] Cluster 八盏聚光阴影差异 " << 簇差异 << std::endl;
	EXPECT_GT(簇差异, 100u) << "Cluster 路径八盏聚光阴影未生效";

	//关闭 Cluster：前向路径应得到同一结果
	ASSERT_TRUE(renderer.cluster_enable_set(false));
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 前向有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(前向有影.data(), static_cast<uint32_t>(前向有影.size())));
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);
	const uint32_t 路径差异 = changed_pixels(簇有影, 前向有影);
	std::cout << "[关卡] 八盏阴影前后向路径差异 " << 路径差异 << std::endl;
	EXPECT_EQ(路径差异, 0u) << "Cluster 与前向路径的八盏阴影结果不一致";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光阴影与 MSAA 组合（第五百二十六轮）
口径：1× 与 4× 采样下八盏阴影均生效（关闭 / 开启差异 > 100 且瓦片 8）；
4× 档追加 Cluster 开 / 关两态，画面逐字节一致（与 278 轮方向光组合矩阵同口径）。
*/
TEST(Renderer_Level_Test, 八盏聚光阴影与MSAA组合)
{
	write_cube_assets();
	write_eight_spot_level();

	const uint32_t msaa_list[2] = { 1u, 4u };
	for (uint32_t msaa_index = 0; msaa_index < 2; ++msaa_index)
	{
		engine::Renderer_Config config;
		config.width = LEVEL_WIDTH;
		config.height = LEVEL_HEIGHT;
		config.hidden = true;
		config.msaa_samples = msaa_list[msaa_index];
		engine::Renderer renderer;
		ASSERT_TRUE(renderer.init(config));
		if (!renderer.readback_supported())
			GTEST_SKIP() << "本机交换链图像不具备传输源用法";

		engine::Level_Data 关卡;
		engine::Asset_Scene 场景;
		ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

		//关闭聚光阴影：参照图
		engine::Light_Data 灯光 = 关卡.light;
			灯光.spot_shadow_enabled = 0;
		renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 无影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(无影.data(), static_cast<uint32_t>(无影.size())));

		//八盏全投影
			灯光.spot_shadow_enabled = 1;
			灯光.spot_shadow_all = 1;
		renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(有影.data(), static_cast<uint32_t>(有影.size())));
		EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);
		const uint32_t 差异 = changed_pixels(无影, 有影);
		std::cout << "[关卡] 八盏阴影 " << msaa_list[msaa_index] << "x 差异 " << 差异 << std::endl;
		EXPECT_GT(差异, 100u) << "MSAA " << msaa_list[msaa_index] << "x 下八盏聚光阴影未生效";

		//4× 档：Cluster 开 / 关两态逐字节一致
		if (msaa_list[msaa_index] == 4u && renderer.cluster_supported())
		{
			ASSERT_TRUE(renderer.cluster_enable_set(true));
			EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
			std::vector<uint8_t> 簇有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
			ASSERT_TRUE(renderer.frame_render(簇有影.data(), static_cast<uint32_t>(簇有影.size())));
			ASSERT_TRUE(renderer.cluster_enable_set(false));
			EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
			std::vector<uint8_t> 前向有影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
			ASSERT_TRUE(renderer.frame_render(前向有影.data(), static_cast<uint32_t>(前向有影.size())));
			const uint32_t 开关差异 = changed_pixels(簇有影, 前向有影);
			std::cout << "[关卡] 4x 下 Cluster 两态差异 " << 开关差异 << std::endl;
			EXPECT_EQ(开关差异, 0u) << "4x 下 Cluster 开关改变了八盏阴影画面";
		}
		level_bridge_unload(renderer, 场景);
	}
}

/*
八盏聚光阴影软度开销（第五百二十七轮）
口径：1920×1080 ＋ 八盏聚光全投影；三态（关闭阴影 / 软度 0 单点 / 软度 1 的 7×7 邻域）
每态预热 2 帧后取样 5 帧，记录场景 GPU 帧耗时均值（设备不支持时间戳时三态均为 0）。
只记录数据与弱断言（软度 1 不低于软度 0 减 0.1 ms），不做跨机器阈值断言。
*/
TEST(Renderer_Level_Test, 八盏聚光软度开销)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = 1920;
	config.height = 1080;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//三态软度（负值表示关闭阴影）
	const float 软度值[3] = { -1.0f, 0.0f, 1.0f };
	const char* 记录键[3] = { "spot_softness_gpu_ms_off", "spot_softness_gpu_ms_hard",
		"spot_softness_gpu_ms_soft" };
	double 帧耗时[3] = { 0.0, 0.0, 0.0 };
	for (std::size_t index = 0; index < 3; ++index)
	{
		//关闭态：直接应用无阴影
		engine::Light_Data 灯光 = 关卡.light;
		if (软度值[index] < 0.0f)
			灯光.spot_shadow_enabled = 0;
		else
		{
			灯光.spot_shadow_enabled = 1;
			灯光.spot_shadow_all = 1;
		}
		renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
		//scene_apply 会按关卡字段重置质量参数，故在其后显式设置软度
		if (软度值[index] >= 0.0f)
			renderer.spot_shadow_params_set(0.010f, 软度值[index], 1.0f);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		//预热 2 帧
		for (uint32_t frame = 0; frame < 2; ++frame)
			ASSERT_TRUE(renderer.frame_render());
		//取样 5 帧取均值
		double 合计 = 0.0;
		for (uint32_t frame = 0; frame < 5; ++frame)
		{
			ASSERT_TRUE(renderer.frame_render());
			合计 += renderer.frame_stats_get().gpu_frame_ms;
		}
		帧耗时[index] = 合计 / 5.0;
		RecordProperty(记录键[index], 帧耗时[index]);
		std::cout << "[关卡] 软度开销 " << 记录键[index] << " " << 帧耗时[index] << " ms" << std::endl;
	}

	if (renderer.gpu_timestamp_supported())
	{
		//三态均应产生非零帧耗时
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_GT(帧耗时[index], 0.0) << "软度开销态 " << index << " 未产生帧耗时";
		//软度 1 不应显著低于软度 0（容差 0.1 ms）
		EXPECT_GE(帧耗时[2], 帧耗时[1] - 0.1) << "软度 1 帧耗时异常低于软度 0";
	}
	else
	{
		for (std::size_t index = 0; index < 3; ++index)
			EXPECT_EQ(帧耗时[index], 0.0);
	}
	level_bridge_unload(renderer, 场景);
}

/*
关卡偏置自校准开关往返（第五百二十九轮）
口径：`light spot_shadow_bias_auto 1` 读回为 1、写出再读回一致；缺省为 0；
非法值（2 / 非数字 / 缺值）整份拒绝。
*/
TEST(Renderer_Level_Test, 关卡偏置自校准开关往返)
{
	write_text(level_dir() / L"偏置自校准开关.level",
		"level 偏置自校准开关\n"
		"light spot_shadow_bias_auto 1\n");

	engine::Level_Data 读取;
	ASSERT_TRUE(engine::level_text_read(level_utf8("偏置自校准开关.level"), 读取));
	EXPECT_EQ(读取.light.spot_shadow_bias_auto, 1);

	//写出再读回一致
	ASSERT_TRUE(engine::level_text_write(读取, level_utf8("偏置自校准开关_回写.level")));
	engine::Level_Data 回读;
	ASSERT_TRUE(engine::level_text_read(level_utf8("偏置自校准开关_回写.level"), 回读));
	EXPECT_EQ(回读.light.spot_shadow_bias_auto, 1);

	//缺省为 0
	write_text(level_dir() / L"偏置自校准缺省.level", "level 偏置自校准缺省\n");
	engine::Level_Data 缺省;
	ASSERT_TRUE(engine::level_text_read(level_utf8("偏置自校准缺省.level"), 缺省));
	EXPECT_EQ(缺省.light.spot_shadow_bias_auto, 0);

	//非法值整份拒绝
	const char* 非法值[3] = { "2", "x", "" };
	for (const char* 值 : 非法值)
	{
		std::string 文本 = std::string("level 偏置自校准非法\nlight spot_shadow_bias_auto ") + 值 + "\n";
		write_text(level_dir() / L"偏置自校准非法.level", 文本);
		engine::Level_Data 拒绝;
		EXPECT_FALSE(engine::level_text_read(level_utf8("偏置自校准非法.level"), 拒绝))
			<< "非法值未拒绝：" << 值;
	}
}

/*
关卡偏置自校准生效（第五百二十九轮）
口径：八盏关卡追加 `light spot_shadow_bias_auto 1` 后装载并应用，门面为真、
逐瓦片偏置按 9 / 8.9 ≈ 0.010112（八盏半径均为 9）；关闭后恢复全局 0.010。
*/
TEST(Renderer_Level_Test, 关卡偏置自校准生效)
{
	write_cube_assets();
	write_eight_spot_level();
	//读取原关卡文本并追加自校准行（独立文件，避免影响其它八盏用例）
	std::string 原文;
	{
		std::ifstream 输入(level_dir() / L"八盏聚光.level", std::ios::binary);
		std::string 行;
		while (std::getline(输入, 行))
			原文 += 行 + "\n";
	}
	write_text(level_dir() / L"八盏聚光校准.level", 原文 + "light spot_shadow_bias_auto 1\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光校准.level"), false));
	EXPECT_EQ(关卡.light.spot_shadow_bias_auto, 1);

	//应用场景后门面应为开启
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_TRUE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	float 偏置[8] = {};
	renderer.spot_shadow_bias_tiles_get(偏置);
	for (std::size_t index = 0; index < 8; ++index)
		EXPECT_NEAR(偏置[index], 0.010f * 9.0f / 8.9f, 1e-4f) << "瓦片 " << index;

	//关闭自校准：恢复全局 0.010
	renderer.spot_shadow_bias_auto_set(false);
	EXPECT_FALSE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	renderer.spot_shadow_bias_tiles_get(偏置);
	for (std::size_t index = 0; index < 8; ++index)
		EXPECT_NEAR(偏置[index], 0.010f, 1e-5f) << "瓦片 " << index;
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光阴影按需重绘（第五百三十三轮）
口径：八盏全投影时静态连续 4 帧阴影 pass 计数不增长；
把第 7 盏聚光灯位置移动后签名变化 → 重绘一次（与 486 轮单盏口径同规则，扩到 8 瓦片）。
*/
TEST(Renderer_Level_Test, 八盏聚光阴影按需重绘)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);

	//静态连续渲染：阴影 pass 不应再增长
	const uint64_t 静态后 = renderer.shadow_pass_count();
	for (int frame = 0; frame < 4; ++frame)
	{
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		ASSERT_TRUE(renderer.frame_render());
	}
	EXPECT_EQ(renderer.shadow_pass_count(), 静态后) << "八盏静态场景仍重绘阴影";

	//移动第 7 盏聚光灯（瓦片 6）：签名变化 → 重绘一次
	灯光.spot_position[6][0] += 1.0f;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	ASSERT_TRUE(renderer.frame_render());
	EXPECT_GT(renderer.shadow_pass_count(), 静态后) << "八盏中单盏移动后未重绘阴影";
	level_bridge_unload(renderer, 场景);
}

/*
八盏偏置自校准画面差异（第五百三十四轮）
口径：八盏全投影时，开启偏置自校准后逐瓦片偏置均为 0.010 × 9 / 8.9 ≈ 0.010112（统计口径）；
八盏同半径时该差异仅 1.1%，小画面不可见（差异只记录且上限 20 像素）；
关闭后与默认图逐字节一致（默认关闭 → 画面与引入前一致）。
*/
TEST(Renderer_Level_Test, 八盏偏置自校准画面差异)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 默认图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(默认图.data(), static_cast<uint32_t>(默认图.size())));

	//开启自校准：逐瓦片偏置 ≈ 0.010112，画面应有边界偏移差异
	renderer.spot_shadow_bias_auto_set(true);
	EXPECT_TRUE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 校准图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(校准图.data(), static_cast<uint32_t>(校准图.size())));
	float 偏置[8] = {};
	renderer.spot_shadow_bias_tiles_get(偏置);
	for (std::size_t index = 0; index < 8; ++index)
		EXPECT_NEAR(偏置[index], 0.010f * 9.0f / 8.9f, 1e-4f) << "瓦片 " << index;
	const uint32_t 校准差异 = changed_pixels(默认图, 校准图);
	std::cout << "[关卡] 八盏偏置自校准画面差异 " << 校准差异 << std::endl;
	/*
	八盏半径均为 9 → 归一化偏置只变化 1.1%（0.010 → 0.010112），128×72 画面下不可见，
	故此差异只记录、不作下限断言；统计口径由上面的逐瓦片偏置断言覆盖。
	*/
	EXPECT_LE(校准差异, 20u) << "开启偏置自校准改变了过多像素（应仅为边界抖动）";

	//关闭自校准：与默认图逐字节一致
	renderer.spot_shadow_bias_auto_set(false);
	EXPECT_FALSE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 复原图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(复原图.data(), static_cast<uint32_t>(复原图.size())));
	EXPECT_EQ(changed_pixels(默认图, 复原图), 0u) << "关闭偏置自校准后画面未复原";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光与方向光阴影共存（第五百三十九轮）
口径：八盏全投影 ＋ 方向光阴影同时开启；三态对照（共存 / 仅方向光 / 仅八盏聚光），
共存图与「仅聚光」差异 > 100（聚光生效）、与「仅方向光」差异 > 0（方向光生效），两套阴影互不干扰。
*/
TEST(Renderer_Level_Test, 八盏聚光与方向光阴影共存)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//方向光调高并斜射（原关卡 0.1 且正下，方向光阴影与聚光阴影完全重叠而不可见）
	engine::Light_Data 灯光 = 关卡.light;
		灯光.intensity = 0.6f;
		灯光.direction[0] = 0.6f;
		灯光.direction[1] = -0.62f;
		灯光.direction[2] = -0.24f;
		for (std::size_t 通道 = 0; 通道 < 3; ++通道)
			灯光.color[通道] = 0.4f;
		灯光.shadow_enabled = 1;
		灯光.shadow_bias = 0.004f;
		灯光.shadow_softness = 0.35f;
		灯光.shadow_strength = 0.8f;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;

	//共存：方向光 ＋ 八盏聚光
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 共存(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(共存.data(), static_cast<uint32_t>(共存.size())));

	//仅方向光（关闭聚光阴影）
	engine::Light_Data 仅方向 = 灯光;
		仅方向.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 仅方向, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 仅方向图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(仅方向图.data(), static_cast<uint32_t>(仅方向图.size())));

	//仅八盏聚光（关闭方向光阴影）
	engine::Light_Data 仅聚光 = 灯光;
		仅聚光.shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 仅聚光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 仅聚光图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(仅聚光图.data(), static_cast<uint32_t>(仅聚光图.size())));

	//聚光生效
	const uint32_t 聚光差异 = changed_pixels(共存, 仅方向图);
	std::cout << "[关卡] 八盏共存 聚光贡献 " << 聚光差异 << std::endl;
	EXPECT_GT(聚光差异, 100u) << "共存态中八盏聚光阴影未生效";
	//方向光生效（八盏场景方向光阴影覆盖较弱，只要求非零）
	const uint32_t 方向差异 = changed_pixels(共存, 仅聚光图);
	std::cout << "[关卡] 八盏共存 方向光贡献 " << 方向差异 << std::endl;
	EXPECT_GT(方向差异, 0u) << "共存态中方向光阴影未生效";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光阴影 512 档细节（第五百四十二轮）
口径：517 轮在 128×72 画幅下 512 档差异为 0（阴影边界过粗）；本轮改 512×288 画幅，
验证 512 档相对 1024 档有可见细节变化，切回 1024 后逐字节恢复。
*/
TEST(Renderer_Level_Test, 八盏聚光阴影512档细节)
{
	write_cube_assets();
	write_eight_spot_level();

	//较大画幅（512×288）以显现分辨率差异
	const uint32_t 宽 = 512;
	const uint32_t 高 = 288;
	engine::Renderer_Config config;
	config.width = 宽;
	config.height = 高;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);

	//1024 档基准
	EXPECT_EQ(renderer.shadow_resolution(), 1024u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 原图(宽 * 高 * 4);
	ASSERT_TRUE(renderer.frame_render(原图.data(), static_cast<uint32_t>(原图.size())));

	//512 档：应有可见细节变化
	renderer.shadow_resolution_set(512);
	EXPECT_EQ(renderer.shadow_resolution(), 512u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 低分辨率(宽 * 高 * 4);
	ASSERT_TRUE(renderer.frame_render(低分辨率.data(), static_cast<uint32_t>(低分辨率.size())));
	const uint32_t 低差异 = changed_pixels(原图, 低分辨率);
	std::cout << "[关卡] 八盏 512x288 画幅 512 档差异 " << 低差异 << std::endl;
	EXPECT_GT(低差异, 0u) << "512 档在较大画幅下仍未改变阴影细节";

	//切回 1024：逐字节恢复
	renderer.shadow_resolution_set(1024);
	EXPECT_EQ(renderer.shadow_resolution(), 1024u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 回图(宽 * 高 * 4);
	ASSERT_TRUE(renderer.frame_render(回图.data(), static_cast<uint32_t>(回图.size())));
	EXPECT_EQ(changed_pixels(原图, 回图), 0u) << "分辨率往返后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光灯阴影（第五百一十四轮）
口径：八盏聚光各照一块地面与遮挡体；单盏（序号 0）只遮蔽最左，多盏开启后八盏各自遮蔽，两图差异 > 100 像素。
*/
TEST(Renderer_Level_Test, 关卡八盏聚光阴影)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);

	//单盏（序号 0）：只遮蔽最左
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 0;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));
	//本帧参与投影的瓦片数（第五百二十轮）
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 1u);

	//多盏：八盏各自投影（瓦片 4..7 按需创建）
	灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 八盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(八盏.data(), static_cast<uint32_t>(八盏.size())));
	EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 8u);

	const uint32_t 差异 = changed_pixels(单盏, 八盏);
	std::cout << "[关卡] 八盏聚光阴影差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "八盏聚光灯阴影未生效";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光逐瓦片互异（第五百五十一轮）
口径：序号 0..7 逐盏单盏投影，每盏使用独立视图矩阵与独立阴影图；
各序号单盏画面均应偏离关闭聚光阴影基准，且相邻序号单盏画面互不相同（每盏遮蔽不同遮挡体）。
*/
TEST(Renderer_Level_Test, 八盏聚光逐瓦片互异)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);

	//关闭聚光阴影基准
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 0;
	灯光.spot_shadow_all = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 基准(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(基准.data(), static_cast<uint32_t>(基准.size())));

	//逐序号单盏投影（瓦片 1）
	灯光.spot_shadow_enabled = 1;
	std::vector<uint8_t> 上帧;
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		灯光.spot_shadow_index = static_cast<int>(序号);
		renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 当前(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(当前.data(), static_cast<uint32_t>(当前.size())));
		EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 1u);

		//与关闭基准的差异（每盏都应产生遮蔽）
		const uint32_t 基准差异 = changed_pixels(基准, 当前);
		std::cout << "[关卡] 第 " << 序号 << " 盏基准差异 " << 基准差异 << std::endl;
		EXPECT_GT(基准差异, 0u) << "第 " << 序号 << " 盏单盏投影未生效";

		//与上一序号单盏的差异（相邻瓦片画面应不同）
		if (序号 > 0)
		{
			const uint32_t 相邻差异 = changed_pixels(上帧, 当前);
			std::cout << "[关卡] 第 " << 序号 - 1 << " / " << 序号 << " 盏瓦片差异 " << 相邻差异 << std::endl;
			EXPECT_GT(相邻差异, 0u) << "第 " << 序号 << " 盏与上一盏投影画面相同（瓦片未独立）";
		}
		上帧 = std::move(当前);
	}
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光逐瓦片互异（簇路径）（第五百五十九轮）
口径：同上例，但灯光经 Cluster 计算路径登记；序号 0..7 逐盏单盏投影，
各序号与关闭基准有差异、相邻序号画面互不相同，确认簇路径亦按盏绑定独立瓦片。
*/
TEST(Renderer_Level_Test, 八盏聚光逐瓦片互异_簇路径)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";
	if (!renderer.cluster_supported())
		GTEST_SKIP() << "本机不支持 Cluster 计算路径";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));
	ASSERT_EQ(关卡.light.spot_count, 8u);
	ASSERT_TRUE(renderer.cluster_enable_set(true));

	//簇路径关闭聚光阴影基准
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 0;
	灯光.spot_shadow_all = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 基准(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(基准.data(), static_cast<uint32_t>(基准.size())));

	//簇路径逐序号单盏投影（瓦片 1）
	灯光.spot_shadow_enabled = 1;
	std::vector<uint8_t> 上帧;
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		灯光.spot_shadow_index = static_cast<int>(序号);
		renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		std::vector<uint8_t> 当前(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
		ASSERT_TRUE(renderer.frame_render(当前.data(), static_cast<uint32_t>(当前.size())));
		EXPECT_EQ(renderer.frame_stats_get().spot_shadow_tiles, 1u);

		//与关闭基准的差异
		const uint32_t 基准差异 = changed_pixels(基准, 当前);
		std::cout << "[关卡] 簇路径第 " << 序号 << " 盏基准差异 " << 基准差异 << std::endl;
		EXPECT_GT(基准差异, 0u) << "簇路径第 " << 序号 << " 盏单盏投影未生效";

		//与上一序号单盏的差异
		if (序号 > 0)
		{
			const uint32_t 相邻差异 = changed_pixels(上帧, 当前);
			std::cout << "[关卡] 簇路径第 " << 序号 - 1 << " / " << 序号 << " 盏瓦片差异 " << 相邻差异 << std::endl;
			EXPECT_GT(相邻差异, 0u) << "簇路径第 " << 序号 << " 盏瓦片未独立";
		}
		上帧 = std::move(当前);
	}
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光质量参数组合（第五百二十三轮）
口径：八盏聚光全部投影时，强度 0 应与「关闭聚光阴影」画面逐字节一致（着色器 mix 结果恒为 1）；
强度 0.5 保留半遮蔽（与全强度差异 > 100 像素）；软度满档（1.0，7×7 邻域）只改变阴影边缘；
恢复默认参数后与全强度基准逐字节一致。
*/
TEST(Renderer_Level_Test, 八盏聚光质量参数组合)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//关闭聚光阴影：无阴影参照图
	engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 无阴影(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(无阴影.data(), static_cast<uint32_t>(无阴影.size())));

	//八盏全投影 ＋ 默认参数（强度 1）
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 全强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(全强度.data(), static_cast<uint32_t>(全强度.size())));
	//门面回读默认三值
	float 偏置 = 0.0f;
	float 软度 = 0.0f;
	float 强度 = 0.0f;
	renderer.spot_shadow_params_get(&偏置, &软度, &强度);
	EXPECT_NEAR(偏置, 0.010f, 1e-6f);
	EXPECT_NEAR(软度, 0.0f, 1e-6f);
	EXPECT_NEAR(强度, 1.0f, 1e-6f);
	const uint32_t 全强度差异 = changed_pixels(无阴影, 全强度);
	std::cout << "[关卡] 八盏全强度差异 " << 全强度差异 << std::endl;
	EXPECT_GT(全强度差异, 100u) << "八盏全强度阴影未生效";

	//强度 0：应与无阴影画面逐字节一致
	renderer.spot_shadow_params_set(0.010f, 0.0f, 0.0f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 零强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(零强度.data(), static_cast<uint32_t>(零强度.size())));
	EXPECT_EQ(changed_pixels(无阴影, 零强度), 0u) << "强度 0 未与关闭聚光阴影逐字节一致";

	//强度 0.5：半遮蔽仍在
	renderer.spot_shadow_params_set(0.010f, 0.0f, 0.5f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 半强度(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(半强度.data(), static_cast<uint32_t>(半强度.size())));
	const uint32_t 半强度差异 = changed_pixels(全强度, 半强度);
	std::cout << "[关卡] 八盏半强度差异 " << 半强度差异 << std::endl;
	EXPECT_GT(半强度差异, 100u) << "强度 0.5 未产生半遮蔽差异";
	EXPECT_GT(changed_pixels(无阴影, 半强度), 100u) << "强度 0.5 未保留可见阴影";

	//软度满档（7×7 邻域）：仅阴影边缘变化
	renderer.spot_shadow_params_set(0.010f, 1.0f, 1.0f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 软边(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(软边.data(), static_cast<uint32_t>(软边.size())));
	const uint32_t 软边差异 = changed_pixels(全强度, 软边);
	std::cout << "[关卡] 八盏软度满档差异 " << 软边差异 << std::endl;
	EXPECT_GT(软边差异, 0u) << "软度满档未改变阴影边缘";

	//恢复默认参数：与全强度基准逐字节一致
	renderer.spot_shadow_params_set(0.010f, 0.0f, 1.0f);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 恢复(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(恢复.data(), static_cast<uint32_t>(恢复.size())));
	EXPECT_EQ(changed_pixels(全强度, 恢复), 0u) << "恢复默认参数后画面未复原";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光分辨率切换（第五百一十七轮）
口径：八盏全部投影时，阴影图 1024 → 512 → 1024 往返；切到 512 画面应有细节变化，切回后逐字节一致。
覆盖分辨率切换路径对瓦片 4..7 的重建与重绑。
*/
TEST(Renderer_Level_Test, 关卡八盏聚光分辨率切换)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//八盏全部投影，1024 基准图
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 原图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(原图.data(), static_cast<uint32_t>(原图.size())));

	//切 512：记录画面差异（随后切回 1024 校验画面恢复）
	renderer.shadow_resolution_set(512);
	EXPECT_EQ(renderer.shadow_resolution(), 512u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 低分辨率(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(低分辨率.data(), static_cast<uint32_t>(低分辨率.size())));
	//512 档画面差异（本场景阴影边界较粗，可能为 0；分辨率效果由单盏用例覆盖）
	const uint32_t 低差异 = changed_pixels(原图, 低分辨率);
	std::cout << "[关卡] 八盏 512 档差异 " << 低差异 << std::endl;

	//切回 1024：应与基准图一致
	renderer.shadow_resolution_set(1024);
	EXPECT_EQ(renderer.shadow_resolution(), 1024u);
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 回图(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(回图.data(), static_cast<uint32_t>(回图.size())));
	EXPECT_EQ(changed_pixels(原图, 回图), 0u) << "分辨率往返后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
聚光灯阴影偏置自校准（第五百二十一轮）
口径：两盏 radius 26 / 9 的聚光；关闭自校准（默认）时逐瓦片偏置都等于全局值；
开启后按参考跨度 9 归一化，小半径瓦片略增、大半径瓦片显著减小，比值 ≈ 两瓦片跨度比。
*/
TEST(Renderer_Level_Test, 聚光灯偏置自校准)
{
	write_cube_assets();
	write_text(level_dir() / L"偏置自校准.level",
		"level 偏置自校准\n"
		"camera position 0 6 8\n"
		"camera target 0 0 0\n"
		"light direction 0 -1 0\n"
		"light color 0.2 0.2 0.2\n"
		"light intensity 0.2\n"
		"light spot -4 4 0 0 -1 0 1 1 1 26 2 20 40 0 0 16\n"
		"light spot 4 4 0 0 -1 0 1 1 1 9 2 20 40 0 0 16\n"
		"mesh 方块 cube.obj\n"
		"mtl 方块材质 cube.mtl\n"
		"place 方块 方块材质 0 -1.5 0 16 0.2 8\n"
		"place 方块 方块材质 -4 0.6 0 1.2 1.2 1.2\n"
		"place 方块 方块材质 4 0.6 0 1.2 1.2 1.2\n");

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("偏置自校准.level"), false));

	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 1;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);

	//默认关闭：两瓦片都等于全局偏置 0.010
	EXPECT_FALSE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 3u);
	ASSERT_TRUE(renderer.frame_render());
	float 偏置[8] = {};
	renderer.spot_shadow_bias_tiles_get(偏置);
	EXPECT_NEAR(偏置[0], 0.010f, 1e-5f);
	EXPECT_NEAR(偏置[1], 0.010f, 1e-5f);

	//开启自校准：瓦片 0（radius 26）≈ 0.010 × 9 / 25.9，瓦片 1（radius 9）≈ 0.010 × 9 / 8.9
	renderer.spot_shadow_bias_auto_set(true);
	EXPECT_TRUE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 3u);
	ASSERT_TRUE(renderer.frame_render());
	renderer.spot_shadow_bias_tiles_get(偏置);
	EXPECT_NEAR(偏置[0], 0.010f * 9.0f / 25.9f, 1e-4f);
	EXPECT_NEAR(偏置[1], 0.010f * 9.0f / 8.9f, 1e-4f);
	//比值应接近跨度比 25.9 / 8.9
	EXPECT_NEAR(偏置[0] * (25.9f / 8.9f), 偏置[1], 5e-4f);

	//关闭后恢复全局偏置
	renderer.spot_shadow_bias_auto_set(false);
	EXPECT_FALSE(renderer.spot_shadow_bias_auto());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 3u);
	ASSERT_TRUE(renderer.frame_render());
	renderer.spot_shadow_bias_tiles_get(偏置);
	EXPECT_NEAR(偏置[0], 0.010f, 1e-5f);
	EXPECT_NEAR(偏置[1], 0.010f, 1e-5f);
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光运行期开关（第五百一十七轮）
口径：八盏场景按单盏应用后用 spot_shadow_all_set 运行期切到八盏、再切回；
八盏与单盏差异 > 100 像素，切回后与初始单盏差异 < 50 像素。
*/
TEST(Renderer_Level_Test, 八盏聚光运行期开关)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));
	if (!renderer.readback_supported())
		GTEST_SKIP() << "本机交换链图像不具备传输源用法";

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//单盏（序号 0）
	engine::Light_Data 灯光 = 关卡.light;
	灯光.spot_shadow_enabled = 1;
	灯光.spot_shadow_all = 0;
	灯光.spot_shadow_index = 0;
	renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
	EXPECT_FALSE(renderer.spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(单盏.data(), static_cast<uint32_t>(单盏.size())));

	//运行期切八盏（按已登记聚光灯数量创建瓦片 1..7）
	renderer.spot_shadow_all_set(true);
	EXPECT_TRUE(renderer.spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 八盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(八盏.data(), static_cast<uint32_t>(八盏.size())));
	const uint32_t 差异 = changed_pixels(单盏, 八盏);
	std::cout << "[关卡] 八盏运行期开关差异 " << 差异 << std::endl;
	EXPECT_GT(差异, 100u) << "运行期切八盏未生效";

	//切回单盏：画面应恢复
	renderer.spot_shadow_all_set(false);
	EXPECT_FALSE(renderer.spot_shadow_all());
	EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
	std::vector<uint8_t> 回单盏(LEVEL_WIDTH * LEVEL_HEIGHT * 4);
	ASSERT_TRUE(renderer.frame_render(回单盏.data(), static_cast<uint32_t>(回单盏.size())));
	const uint32_t 回差异 = changed_pixels(单盏, 回单盏);
	std::cout << "[关卡] 八盏运行期切回差异 " << 回差异 << std::endl;
	EXPECT_LT(回差异, 50u) << "运行期切回单盏后画面未恢复";
	level_bridge_unload(renderer, 场景);
}

/*
八盏聚光灯开销基线（第五百一十五轮）
口径：同一八盏聚光场景，分别登记 1 / 8 盏并全部投影；记录阴影 pass 单遍 GPU 耗时，
断言 8 盏的单遍耗时不显著低于 1 盏（容差 0.1 ms），即总成本随遍数线性增长。
*/
TEST(Renderer_Level_Test, 关卡八盏聚光开销)
{
	write_cube_assets();
	write_eight_spot_level();

	engine::Renderer_Config config;
	config.width = LEVEL_WIDTH;
	config.height = LEVEL_HEIGHT;
	config.hidden = true;
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(config));

	engine::Level_Data 关卡;
	engine::Asset_Scene 场景;
	ASSERT_TRUE(level_bridge_load(renderer, 关卡, 场景, level_utf8("八盏聚光.level"), false));

	//分别按 1 / 8 盏登记，全部投影并预热 6 帧
	const uint32_t counts[2] = { 1u, 8u };
	double shadow_ms[2] = { 0.0, 0.0 };
	std::string keys[2] = { "spot_shadow_gpu_ms_1of8", "spot_shadow_gpu_ms_8of8" };
	for (std::size_t index = 0; index < 2; ++index)
	{
		engine::Light_Data 灯光 = 关卡.light;
		灯光.spot_shadow_enabled = 1;
		灯光.spot_shadow_all = 1;
		灯光.spot_count = counts[index];
		renderer.scene_apply(关卡.camera, 灯光, 关卡.fog);
		EXPECT_EQ(level_bridge_draw(renderer, 场景), 9u);
		for (uint32_t frame = 0; frame < 6; ++frame)
			ASSERT_TRUE(renderer.frame_render());
		shadow_ms[index] = renderer.frame_stats_get().gpu_shadow_ms;
		RecordProperty(keys[index], shadow_ms[index]);
		std::cout << "[关卡] 八盏开销 " << counts[index] << " 盏阴影耗时 " << shadow_ms[index] << " ms" << std::endl;
	}

	if (renderer.gpu_timestamp_supported())
	{
		EXPECT_GT(shadow_ms[1], 0.0) << "8 盏聚光未产生阴影耗时";
		EXPECT_GE(shadow_ms[1], shadow_ms[0] - 0.1) << "8 盏阴影耗时异常低于 1 盏";
	}
	else
	{
		for (std::size_t index = 0; index < 2; ++index)
			EXPECT_EQ(shadow_ms[index], 0.0);
	}
	level_bridge_unload(renderer, 场景);
}

/*
八盏点光边界（第四百三十六轮）
口径：恰好 8 盏通过；第 9 盏拒绝（上限 8）。
*/
TEST(Renderer_Level_Test, 八盏点光边界)
{
	std::string text = "level 八盏\n";
	for (uint32_t 序号 = 0; 序号 < 8; ++序号)
	{
		char 行[128] = {};
		std::snprintf(行, sizeof(行), "light point %d 0 0 1 1 1 5 1 0 1 0\n", static_cast<int>(序号));
		text += 行;
	}
	write_text(level_dir() / L"八盏.level", text);

	engine::Level_Data loaded;
	ASSERT_TRUE(engine::level_text_read(level_utf8("八盏.level"), loaded));
	EXPECT_EQ(loaded.light.point_count, 8u);

	text += "light point 8 0 0 1 1 1 5 1 0 1 0\n";
	write_text(level_dir() / L"九盏.level", text);
	engine::Level_Data too_many;
	EXPECT_FALSE(engine::level_text_read(level_utf8("九盏.level"), too_many));
	EXPECT_EQ(too_many.light.point_count, 0u);
}
