/*
渲染纹理分级测试（第一百五十八 / 一百六十一轮拆分）
覆盖：mip 级子区域更新（仅重建下游）、分级读回参数校验、分级尺寸查询。
从「渲染网格纹理测试.cpp」拆出，保持断言不变（单文件 ≤ 800 行约束）。
*/
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

//获取渲染器门面与配置
#include "Application/Renderer/src/render/Renderer/渲染器.h"

namespace
{
	//构造隐藏窗口配置
	engine::Renderer_Config hidden_config(void)
	{
		engine::Renderer_Config config;
		config.width = 64;
		config.height = 36;
		config.hidden = true;
		return config;
	}
}

//纹理分级测试夹具（与网格纹理套件分离，便于独立定位）
class Renderer_Texture_Level_Test : public ::testing::Test
{
};

/*
纹理分级区域更新（第一百五十八轮）
口径：4×4 全白纹理（自带 3 级 mip：4×4 / 2×2 / 1×1），把第 1 级（2×2）整幅改为纯红：
      ① 第 1 级读回四像素为红；② 下游第 2 级（1×1）由新内容重建，同样为红；
      ③ 上游第 0 级保持全白（分级更新不波及上游）；④ 级号越界 / 按级尺寸越界 / 空指针均被拒绝。
*/
TEST_F(Renderer_Texture_Level_Test, 纹理分级区域更新)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	//4×4 全白（RGBA8 紧排）
	std::vector<uint8_t> white(4 * 4 * 4, 255);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load(texture, white.data(), 4, 4));
	//第 1 级整幅纯红（2×2）
	const uint8_t red[16] = {
		255, 0, 0, 255,
		255, 0, 0, 255,
		255, 0, 0, 255,
		255, 0, 0, 255,
	};
	ASSERT_TRUE(renderer.texture_update_region_level(texture, 1, 0, 0, 2, 2, red));

	//① 第 1 级四像素为红
	std::vector<uint8_t> level1(2 * 2 * 4, 0);
	uint32_t level_width = 0;
	uint32_t level_height = 0;
	ASSERT_TRUE(renderer.texture_readback_level(texture, 1, level1.data(),
		static_cast<uint32_t>(level1.size()), &level_width, &level_height));
	EXPECT_EQ(level_width, 2u);
	EXPECT_EQ(level_height, 2u);
	for (uint32_t index = 0; index < 4; ++index)
	{
		EXPECT_EQ(level1[index * 4], 255) << "第 1 级红通道未更新";
		EXPECT_EQ(level1[index * 4 + 1], 0) << "第 1 级绿通道未更新";
		EXPECT_EQ(level1[index * 4 + 2], 0) << "第 1 级蓝通道未更新";
	}

	//② 下游第 2 级（1×1）由红色重建
	std::vector<uint8_t> level2(4, 0);
	ASSERT_TRUE(renderer.texture_readback_level(texture, 2, level2.data(), 4, nullptr, nullptr));
	EXPECT_GE(level2[0], 200) << "第 2 级红通道过暗（下游未重建）";
	EXPECT_LE(level2[1], 40) << "第 2 级绿通道未随之下降";
	EXPECT_LE(level2[2], 40) << "第 2 级蓝通道未随之下降";

	//③ 上游第 0 级仍全白
	std::vector<uint8_t> level0(4 * 4 * 4, 0);
	ASSERT_TRUE(renderer.texture_readback_level(texture, 0, level0.data(),
		static_cast<uint32_t>(level0.size()), nullptr, nullptr));
	for (uint32_t index = 0; index < 16; ++index)
		EXPECT_EQ(level0[index * 4 + 1], 255) << "第 0 级被分级更新波及";

	//④ 级号越界 / 按级尺寸越界 / 空指针均拒绝
	EXPECT_FALSE(renderer.texture_update_region_level(texture, 3, 0, 0, 1, 1, red));
	EXPECT_FALSE(renderer.texture_update_region_level(texture, 1, 1, 1, 2, 2, red));
	EXPECT_FALSE(renderer.texture_update_region_level(texture, 1, 0, 0, 2, 2, nullptr));
	renderer.texture_unload(texture);
}

/*
分级读回参数校验（第一百五十八轮）
口径：未初始化 / 级号越界 / 容量不足 / 无效句柄 / 卸载后 均返回假且尺寸出参归零；
      正常读回第 1 级时写出该级尺寸 2×2。
*/
TEST_F(Renderer_Texture_Level_Test, 分级读回参数校验)
{
	//未初始化
	engine::Renderer cold;
	uint8_t buffer[16] = {};
	uint32_t width = 7;
	uint32_t height = 7;
	EXPECT_FALSE(cold.texture_readback_level(1, 0, buffer, 16, &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);

	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	std::vector<uint8_t> white(4 * 4 * 4, 255);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load(texture, white.data(), 4, 4));

	//级号越界（4×4 只有 3 级：0 / 1 / 2）
	width = 9;
	height = 9;
	EXPECT_FALSE(renderer.texture_readback_level(texture, 3, buffer, 16, &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
	//容量不足（第 0 级需 64 字节，仅给 16）
	EXPECT_FALSE(renderer.texture_readback_level(texture, 0, buffer, 16, nullptr, nullptr));
	//无效句柄
	EXPECT_FALSE(renderer.texture_readback_level(9999u, 0, buffer, 16, nullptr, nullptr));

	//正常读回第 1 级：写出该级尺寸 2×2
	width = 0;
	height = 0;
	EXPECT_TRUE(renderer.texture_readback_level(texture, 1, buffer, 16, &width, &height));
	EXPECT_EQ(width, 2u);
	EXPECT_EQ(height, 2u);
	renderer.texture_unload(texture);
	//卸载后句柄失效
	EXPECT_FALSE(renderer.texture_readback_level(texture, 1, buffer, 16, nullptr, nullptr));
}

/*
分级尺寸查询（第一百六十一轮）
口径：4×4 纹理自带 3 级（4×4 / 2×2 / 1×1），逐级查询尺寸一致；级号越界 / 未初始化 返回假且出参归零。
*/
TEST_F(Renderer_Texture_Level_Test, 分级尺寸查询)
{
	engine::Renderer renderer;
	ASSERT_TRUE(renderer.init(hidden_config()));
	std::vector<uint8_t> white(4 * 4 * 4, 255);
	uint64_t texture = 0;
	ASSERT_TRUE(renderer.texture_load(texture, white.data(), 4, 4));

	//逐级尺寸
	uint32_t width = 0;
	uint32_t height = 0;
	ASSERT_TRUE(renderer.texture_size_level(texture, 0, &width, &height));
	EXPECT_EQ(width, 4u);
	EXPECT_EQ(height, 4u);
	ASSERT_TRUE(renderer.texture_size_level(texture, 1, &width, &height));
	EXPECT_EQ(width, 2u);
	EXPECT_EQ(height, 2u);
	ASSERT_TRUE(renderer.texture_size_level(texture, 2, &width, &height));
	EXPECT_EQ(width, 1u);
	EXPECT_EQ(height, 1u);
	//出参可为空
	EXPECT_TRUE(renderer.texture_size_level(texture, 1, nullptr, nullptr));

	//级号越界：返回假且出参归零
	width = 9;
	height = 9;
	EXPECT_FALSE(renderer.texture_size_level(texture, 3, &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);

	//未初始化 / 无效句柄
	engine::Renderer cold;
	width = 9;
	height = 9;
	EXPECT_FALSE(cold.texture_size_level(texture, 0, &width, &height));
	EXPECT_EQ(width, 0u);
	EXPECT_EQ(height, 0u);
	EXPECT_FALSE(renderer.texture_size_level(9999u, 0, nullptr, nullptr));

	renderer.texture_unload(texture);
	EXPECT_FALSE(renderer.texture_size_level(texture, 0, nullptr, nullptr));
}
