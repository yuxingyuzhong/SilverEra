//展区①②：程序化网格（①）与 PBR 材质（②）
#include "src/主调/能力展示/展区公共.h"

#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <vector>

namespace engine
{
	//展区①：程序化网格（十种图元环形排布 ＋ 细分地形底板）
	void 展区01_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//细分地形底板（9×9 单位、8×8 段，略高于地台避免 z-fighting）
		if (资源.地形 != 0)
		{
			渲染器.material_tint_set(0.34f, 0.40f, 0.52f, 1.0f);
			const std::array<float, 16> 地形矩阵 = 展示::变换(-14.0f, 0.06f, -10.0f, 1.0f);
			(void)渲染器.mesh_draw(资源.地形, 地形矩阵.data());
			渲染器.material_tint_clear();
		}
		//圆台底座
		渲染器.material_tint_set(0.30f, 0.34f, 0.44f, 1.0f);
		const std::array<float, 16> 圆台 = 展示::非均匀变换(-14.0f, 0.25f, -10.0f, 2.4f, 0.25f, 2.4f);
		(void)渲染器.mesh_draw(资源.圆柱, 圆台.data());
		渲染器.material_tint_clear();

		//十种图元环形排布（含三角形与四棱锥，补全图元清单）
		const uint64_t 网格[] = { 资源.球, 资源.圆柱, 资源.圆锥, 资源.胶囊, 资源.圆环,
			资源.平面, 资源.圆盘, 资源.立方体, 资源.四棱锥, 资源.三角 };
		const float 色调表[10][3] = {
			{ 1.0f, 0.45f, 0.40f }, { 1.0f, 0.65f, 0.30f }, { 1.0f, 0.92f, 0.45f }, { 0.45f, 0.95f, 0.55f },
			{ 0.40f, 0.90f, 0.95f }, { 0.45f, 0.62f, 1.0f }, { 0.72f, 0.50f, 1.0f }, { 1.0f, 0.55f, 0.85f },
			{ 0.95f, 0.78f, 0.42f }, { 0.62f, 0.85f, 0.55f },
		};
		const uint32_t 个数 = static_cast<uint32_t>(sizeof(网格) / sizeof(网格[0]));
		for (uint32_t 序号 = 0; 序号 < 个数; ++序号)
		{
			const float 角度 = 展示::圆周率 * 2.0f * static_cast<float>(序号) / static_cast<float>(个数);
			const float x = -14.0f + std::cos(角度) * 1.85f;
			const float z = -10.0f + std::sin(角度) * 1.85f;
			/*
			二维图元（平面 5 / 圆盘 6 / 三角 9）单独处理
			三者天生没有厚度：沿用统一朝向时侧看只剩一条薄片，背面还会被剔除。
			故改用「法线朝环外」的绕 Y 朝向 ＋ 双面材质，任意绕行角度都能看到完整面。
			*/
			const bool 二维图元 = (序号 == 5u || 序号 == 6u || 序号 == 9u);
			if (二维图元 && 资源.双面材质 != 0)
				渲染器.material_bind_set(资源.双面材质);
			渲染器.material_tint_set(色调表[序号][0], 色调表[序号][1], 色调表[序号][2], 1.0f);
			//法线朝外所需偏航角：+Z 法线经 R_y(θ) 后为 (sinθ, 0, cosθ)，令其等于 (cos角度, 0, sin角度)
			const float 偏航 = 展示::圆周率 * 0.5f - 角度;
			const std::array<float, 16> 矩阵 = 二维图元
				? 展示::绕Y变换(x, 0.95f, z, 偏航, 0.5f)
				: 展示::变换(x, 0.95f, z, 0.5f);
			(void)渲染器.mesh_draw(网格[序号], 矩阵.data());
			if (二维图元 && 资源.双面材质 != 0)
				渲染器.material_bind_set(0);
		}
		渲染器.material_tint_clear();
	}

	//展区②：PBR 材质（粗糙度 / 金属度递增 ＋ 法线与粗糙度贴图 ＋ 自发光 ＋ 清漆对比）
	void 展区02_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		const float 粗糙度[4] = { 0.10f, 0.35f, 0.65f, 0.92f };
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			(void)渲染器.material_pbr_set(粗糙度[序号], 0.0f);
			const std::array<float, 16> 矩阵 = 展示::变换(-4.5f + 2.2f * static_cast<float>(序号), 0.75f, -8.0f, 0.7f);
			(void)渲染器.mesh_draw(资源.球, 矩阵.data());
		}
		//金属度递增（粗糙度固定 0.3）
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			(void)渲染器.material_pbr_set(0.3f, 0.15f + 0.25f * static_cast<float>(序号));
			const std::array<float, 16> 矩阵 = 展示::变换(-4.5f + 2.2f * static_cast<float>(序号), 0.75f, -11.5f, 0.7f);
			(void)渲染器.mesh_draw(资源.球, 矩阵.data());
		}
		(void)渲染器.material_pbr_clear();

		//法线 / 粗糙度贴图球（一次登记的材质）
		if (资源.贴图材质 != 0)
		{
			渲染器.material_bind_set(资源.贴图材质);
			const std::array<float, 16> 贴图球 = 展示::变换(4.4f, 0.9f, -9.8f, 0.85f);
			(void)渲染器.mesh_draw(资源.球, 贴图球.data());
			渲染器.material_bind_set(0);
		}
		//自发光标记（示意材质的 emissive 通道）
		渲染器.material_emissive_set(3.0f);
		渲染器.material_tint_set(0.45f, 0.85f, 1.0f, 1.0f);
		const std::array<float, 16> 自发光 = 展示::变换(4.6f, 2.0f, -11.8f, 0.5f);
		(void)渲染器.mesh_draw(资源.圆环, 自发光.data());
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();

		/*
		清漆层对比球
		左侧走全局 PBR（无清漆），右侧绑定清漆材质；两者粗糙度 / 金属度 / 尺寸完全相同，
		只有清漆层有无不同，便于近距离比较第二层锐利反射。
		*/
		(void)渲染器.material_pbr_set(0.5f, 0.2f);
		const std::array<float, 16> 素坯球 = 展示::变换(2.1f, 0.85f, -13.2f, 0.55f);
		(void)渲染器.mesh_draw(资源.球, 素坯球.data());
		(void)渲染器.material_pbr_clear();
		if (资源.清漆材质 != 0)
		{
			渲染器.material_bind_set(资源.清漆材质);
			const std::array<float, 16> 清漆球 = 展示::变换(3.8f, 0.85f, -13.2f, 0.55f);
			(void)渲染器.mesh_draw(资源.球, 清漆球.data());
			渲染器.material_bind_set(0);
		}
	}
}
