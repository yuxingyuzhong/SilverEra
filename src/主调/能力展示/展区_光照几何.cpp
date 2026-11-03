//展区③④⑤：光照与阴影、实例化与合批、几何拼接
#include "src/主调/能力展示/展区公共.h"

#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <vector>

namespace engine
{
	//展区③：光照与阴影（八根立柱 ＋ 顶部聚光 ＋ 三盏点光可见光源球）
	void 展区03_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		for (uint32_t 序号 = 0; 序号 < 8; ++序号)
		{
			const float x = 10.5f + 1.0f * static_cast<float>(序号);
			//柱身（圆柱）
			const std::array<float, 16> 柱身 = 展示::非均匀变换(x, 1.0f, -10.0f, 0.4f, 1.2f, 0.4f);
			(void)渲染器.mesh_draw(资源.圆柱, 柱身.data());
			//柱头（立方体）
			const std::array<float, 16> 柱头 = 展示::变换(x, 2.2f, -10.0f, 0.4f);
			(void)渲染器.mesh_draw(资源.立方体, 柱头.data());
		}
		/*
		聚光灯自身在展区地面上形成光池，不再额外绘制自发光圆盘
		（自发光圆盘贴近地面时会变成一块刺眼白斑，干扰「阴影」的观察）。
		*/
		const float 光源位置[3][3] = { { 0.0f, 6.0f, -10.0f }, { -14.0f, 6.0f, -30.0f }, { 14.0f, 6.0f, -30.0f } };
		const float 光源颜色[3][3] = { { 1.0f, 0.42f, 0.30f }, { 0.34f, 0.78f, 1.0f }, { 1.0f, 0.86f, 0.52f } };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			渲染器.material_emissive_set(4.0f);
			渲染器.material_tint_set(光源颜色[序号][0], 光源颜色[序号][1], 光源颜色[序号][2], 1.0f);
			const std::array<float, 16> 光源球 = 展示::变换(光源位置[序号][0], 光源位置[序号][1],
				光源位置[序号][2], 0.22f);
			(void)渲染器.mesh_draw(资源.球, 光源球.data());
		}
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();
	}

	//展区④：实例化与合批（阶梯金字塔 ＋ 环形小球阵，两次实例化提交）
	void 展区04_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		std::vector<float> 阶梯矩阵;
		阶梯矩阵.reserve(55 * 16);
		const uint32_t 层数表[5] = { 5, 4, 3, 2, 1 };
		for (uint32_t 层 = 0; 层 < 5; ++层)
		{
			const uint32_t 边长 = 层数表[层];
			const float 半 = (static_cast<float>(边长) - 1.0f) * 0.5f;
			for (uint32_t 行 = 0; 行 < 边长; ++行)
			{
				for (uint32_t 列 = 0; 列 < 边长; ++列)
				{
					const float x = -14.0f + (static_cast<float>(列) - 半) * 1.15f;
					const float z = -30.0f + (static_cast<float>(行) - 半) * 1.15f;
					const float y = 0.36f + static_cast<float>(层) * 0.72f;
					const std::array<float, 16> 块 = 展示::变换(x, y, z, 0.45f);
					阶梯矩阵.insert(阶梯矩阵.end(), 块.begin(), 块.end());
				}
			}
		}
		//阶梯色调（紫蓝）
		渲染器.material_tint_set(0.58f, 0.52f, 0.92f, 1.0f);
		(void)渲染器.mesh_draw_instanced(资源.立方体, 阶梯矩阵.data(),
			static_cast<uint32_t>(阶梯矩阵.size() / 16));
		渲染器.material_tint_clear();

		//环绕小球阵（第二类实例化）
		std::vector<float> 球阵;
		球阵.reserve(12 * 16);
		for (uint32_t 序号 = 0; 序号 < 12; ++序号)
		{
			const float 角度 = 展示::圆周率 * 2.0f * static_cast<float>(序号) / 12.0f;
			const float x = -14.0f + std::cos(角度) * 4.6f;
			const float z = -30.0f + std::sin(角度) * 4.6f;
			const std::array<float, 16> 球 = 展示::变换(x, 0.35f, z, 0.3f);
			球阵.insert(球阵.end(), 球.begin(), 球.end());
		}
		渲染器.material_tint_set(1.0f, 0.78f, 0.36f, 1.0f);
		(void)渲染器.mesh_draw_instanced(资源.球, 球阵.data(), 12);
		渲染器.material_tint_clear();
	}

	//展区⑤：复杂几何拼接（拱门 ＋ 晶塔 ＋ 环阵）
	void 展区05_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		渲染器.material_tint_set(0.72f, 0.66f, 0.52f, 1.0f);
		//拱门立柱与横梁
		const std::array<float, 16> 左柱 = 展示::非均匀变换(-4.4f, 2.2f, -30.0f, 0.28f, 2.4f, 0.28f);
		const std::array<float, 16> 右柱 = 展示::非均匀变换(-1.6f, 2.2f, -30.0f, 0.28f, 2.4f, 0.28f);
		(void)渲染器.mesh_draw(资源.立方体, 左柱.data());
		(void)渲染器.mesh_draw(资源.立方体, 右柱.data());
		const std::array<float, 16> 横梁 = 展示::非均匀变换(-3.0f, 4.5f, -30.0f, 1.9f, 0.2f, 0.28f);
		(void)渲染器.mesh_draw(资源.立方体, 横梁.data());
		//拱门顶环
		const std::array<float, 16> 顶环 = 展示::绕Y变换(-3.0f, 5.1f, -30.0f, 0.0f, 0.75f);
		(void)渲染器.mesh_draw(资源.圆环, 顶环.data());
		//晶塔基座 / 塔身 / 锥顶
		const std::array<float, 16> 塔基 = 展示::非均匀变换(2.4f, 0.55f, -30.0f, 1.6f, 0.42f, 1.6f);
		const std::array<float, 16> 塔身 = 展示::非均匀变换(2.4f, 1.5f, -30.0f, 1.1f, 0.6f, 1.1f);
		const std::array<float, 16> 塔顶 = 展示::非均匀变换(2.4f, 2.9f, -30.0f, 0.8f, 0.8f, 0.8f);
		(void)渲染器.mesh_draw(资源.圆柱, 塔基.data());
		(void)渲染器.mesh_draw(资源.圆柱, 塔身.data());
		(void)渲染器.mesh_draw(资源.圆锥, 塔顶.data());
		//晶塔能量球（自发光）
		渲染器.material_emissive_set(2.5f);
		渲染器.material_tint_set(0.55f, 0.85f, 1.0f, 1.0f);
		const std::array<float, 16> 能量球 = 展示::变换(2.4f, 3.75f, -30.0f, 0.3f);
		(void)渲染器.mesh_draw(资源.球, 能量球.data());
		//环绕晶塔的两道旋转环
		渲染器.material_tint_set(0.72f, 0.66f, 0.52f, 1.0f);
		const std::array<float, 16> 晶环甲 = 展示::绕Y变换(2.4f, 2.2f, -30.0f, 状态.累计秒 * 0.7f, 1.2f);
		const std::array<float, 16> 晶环乙 = 展示::绕Y变换(2.4f, 1.3f, -30.0f, -状态.累计秒 * 1.1f, 1.0f);
		(void)渲染器.mesh_draw(资源.圆环, 晶环甲.data());
		(void)渲染器.mesh_draw(资源.圆环, 晶环乙.data());
		//环阵（大环 ＋ 中心球 ＋ 环绕小球）
		const std::array<float, 16> 大环 = 展示::绕Y变换(4.6f, 1.8f, -33.0f, 状态.累计秒 * 0.35f, 1.5f);
		(void)渲染器.mesh_draw(资源.圆环, 大环.data());
		const std::array<float, 16> 中心球 = 展示::变换(4.6f, 1.8f, -33.0f, 0.45f);
		(void)渲染器.mesh_draw(资源.球, 中心球.data());
		//绕环小球（实例化）
		std::vector<float> 环珠;
		环珠.reserve(8 * 16);
		for (uint32_t 序号 = 0; 序号 < 8; ++序号)
		{
			const float 角度 = 展示::圆周率 * 2.0f * static_cast<float>(序号) / 8.0f + 状态.累计秒 * 0.4f;
			const float 珠x = 4.6f + std::cos(角度) * 1.9f;
			const float 珠y = 1.8f + std::sin(角度) * 1.9f;
			const std::array<float, 16> 珠 = 展示::变换(珠x, 珠y, -33.0f, 0.18f);
			环珠.insert(环珠.end(), 珠.begin(), 珠.end());
		}
		(void)渲染器.mesh_draw_instanced(资源.球, 环珠.data(), 8);
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();
	}
}
