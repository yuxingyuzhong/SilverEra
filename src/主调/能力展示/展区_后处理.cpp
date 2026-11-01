//展区⑫–⑰：后处理类展区（泛光链 / LUT 与色调映射 / SSAO 与 FXAA / 描边与体积雾 / 体积光 / SSR）
#include "src/主调/能力展示/展区公共.h"

#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <vector>

namespace engine
{
	//展区⑫：泛光链（亮部提取 → 模糊 → 暗角）—— 高亮球作为泛光光源
	void 展区12_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//三颗自发光球（亮度梯度，进区后可见泛光外溢）
		const float 亮度[3] = { 6.0f, 3.5f, 2.0f };
		const float 位置[3][2] = { { 12.0f, -72.0f }, { 14.2f, -68.6f }, { 16.0f, -72.0f } };
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			渲染器.material_emissive_set(亮度[序号]);
			渲染器.material_tint_set(1.0f, 0.82f, 0.45f, 1.0f);
			const std::array<float, 16> 球 = 展示::变换(位置[序号][0], 1.6f, 位置[序号][1], 0.42f);
			(void)渲染器.mesh_draw(资源.球, 球.data());
		}
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();
		//暗角观察用：四角立柱外再放一圈低矮立方体
		渲染器.material_tint_set(0.30f, 0.30f, 0.36f, 1.0f);
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			const float x = 14.0f + (序号 < 2 ? -3.2f : 3.2f);
			const float z = -70.0f + (序号 % 2 == 0 ? -3.2f : 3.2f);
			const std::array<float, 16> 座 = 展示::变换(x, 0.35f, z, 0.5f);
			(void)渲染器.mesh_draw(资源.立方体, 座.data());
		}
		渲染器.material_tint_clear();
	}

	//展区⑬：3D LUT ＋ 色调映射 —— 色彩标板
	void 展区13_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//六块色标（红 / 绿 / 蓝 / 黄 / 青 / 品红）
		const float 色板[6][3] = {
			{ 0.85f, 0.20f, 0.18f }, { 0.22f, 0.78f, 0.30f }, { 0.20f, 0.35f, 0.86f },
			{ 0.92f, 0.82f, 0.24f }, { 0.24f, 0.82f, 0.84f }, { 0.86f, 0.32f, 0.80f },
		};
		for (uint32_t 序号 = 0; 序号 < 6; ++序号)
		{
			渲染器.material_tint_set(色板[序号][0], 色板[序号][1], 色板[序号][2], 1.0f);
			const float x = -16.6f + 1.05f * static_cast<float>(序号 % 3);
			const float z = -91.5f + (序号 < 3 ? 0.0f : 1.6f);
			const std::array<float, 16> 块 = 展示::非均匀变换(x, 1.35f, z, 1.4f, 1.4f, 0.35f);
			(void)渲染器.mesh_draw(资源.立方体, 块.data());
		}
		渲染器.material_tint_clear();
		//灰度阶梯（检验色调映射曲线对高光的压缩）
		渲染器.material_tint_set(0.75f, 0.75f, 0.75f, 1.0f);
		const std::array<float, 16> 灰条 = 展示::非均匀变换(-14.0f, 2.3f, -92.4f, 3.2f, 0.18f, 0.6f);
		(void)渲染器.mesh_draw(资源.立方体, 灰条.data());
		渲染器.material_tint_clear();
	}

	//展区⑭：SSAO ＋ FXAA —— 密集几何（棱角多，便于看出遮蔽与锯齿）
	void 展区14_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		std::vector<float> 阵列;
		阵列.reserve(54 * 16);
		for (uint32_t 行 = 0; 行 < 6; ++行)
		{
			for (uint32_t 列 = 0; 列 < 9; ++列)
			{
				const float x = -3.6f + 0.9f * static_cast<float>(列);
				const float z = -93.6f + 0.9f * static_cast<float>(行);
				const float y = 0.30f + 0.24f * static_cast<float>((行 + 列) % 3);
				const std::array<float, 16> 块 = 展示::变换(x, y, z, 0.34f);
				阵列.insert(阵列.end(), 块.begin(), 块.end());
			}
		}
		渲染器.material_tint_set(0.62f, 0.62f, 0.70f, 1.0f);
		(void)渲染器.mesh_draw_instanced(资源.立方体, 阵列.data(), static_cast<uint32_t>(阵列.size() / 16));
		渲染器.material_tint_clear();
		//细杆阵（FXAA 观察细长边缘）
		渲染器.material_tint_set(0.88f, 0.80f, 0.56f, 1.0f);
		for (uint32_t 序号 = 0; 序号 < 8; ++序号)
		{
			const float x = 1.4f + 0.55f * static_cast<float>(序号);
			const std::array<float, 16> 杆 = 展示::非均匀变换(x, 1.6f, -87.0f, 0.07f, 1.6f, 0.07f);
			(void)渲染器.mesh_draw(资源.圆柱, 杆.data());
		}
		渲染器.material_tint_clear();
	}

	//展区⑮：深度描边 ＋ 体积雾 —— 远近叠置的门框
	void 展区15_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//三进门框（由近及远，体积雾会把远处门框压淡）
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			const float z = -88.0f - 3.4f * static_cast<float>(序号);
			渲染器.material_tint_set(0.70f - 0.15f * static_cast<float>(序号),
				0.62f - 0.12f * static_cast<float>(序号), 0.50f + 0.10f * static_cast<float>(序号), 1.0f);
			//左右立柱
			const std::array<float, 16> 左 = 展示::非均匀变换(11.4f, 2.0f, z, 0.3f, 2.2f, 0.3f);
			const std::array<float, 16> 右 = 展示::非均匀变换(16.6f, 2.0f, z, 0.3f, 2.2f, 0.3f);
			//顶梁
			const std::array<float, 16> 顶 = 展示::非均匀变换(14.0f, 4.0f, z, 2.9f, 0.28f, 0.3f);
			(void)渲染器.mesh_draw(资源.立方体, 左.data());
			(void)渲染器.mesh_draw(资源.立方体, 右.data());
			(void)渲染器.mesh_draw(资源.立方体, 顶.data());
		}
		渲染器.material_tint_clear();
	}

	//展区⑯：体积光（God ray）—— 高位光源 ＋ 立柱，光束穿过柱间
	void 展区16_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//光源球（高处，屏幕外一点也能看见光束方向）
		渲染器.material_emissive_set(5.0f);
		渲染器.material_tint_set(1.0f, 0.92f, 0.66f, 1.0f);
		const std::array<float, 16> 光源 = 展示::变换(-14.0f, 8.2f, -112.0f, 0.5f);
		(void)渲染器.mesh_draw(资源.球, 光源.data());
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();
		//柱阵（遮挡光束形成条纹）
		渲染器.material_tint_set(0.26f, 0.26f, 0.32f, 1.0f);
		for (uint32_t 序号 = 0; 序号 < 5; ++序号)
		{
			const float x = -18.0f + 2.0f * static_cast<float>(序号);
			const std::array<float, 16> 柱 = 展示::非均匀变换(x, 2.2f, -110.0f, 0.22f, 2.4f, 0.22f);
			(void)渲染器.mesh_draw(资源.立方体, 柱.data());
		}
		渲染器.material_tint_clear();
	}

	//展区⑰：屏幕空间反射 —— 镜面地板 ＋ 铬球 ＋ 立柱，反射互相映照
	void 展区17_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//镜面地板（金属度 1、粗糙度 0.05）
		(void)渲染器.material_pbr_set(0.06f, 1.0f);
		渲染器.material_tint_set(0.72f, 0.78f, 0.86f, 1.0f);
		const std::array<float, 16> 镜面 = 展示::非均匀变换(0.0f, 0.04f, -110.0f, 4.2f, 0.05f, 4.2f);
		(void)渲染器.mesh_draw(资源.立方体, 镜面.data());
		渲染器.material_tint_clear();
		(void)渲染器.material_pbr_clear();
		//三颗高金属度球（反射主体）
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			(void)渲染器.material_pbr_set(0.08f + 0.06f * static_cast<float>(序号), 0.95f);
			const float x = -2.6f + 2.6f * static_cast<float>(序号);
			const std::array<float, 16> 球 = 展示::变换(x, 1.1f, -108.0f, 0.8f);
			(void)渲染器.mesh_draw(资源.球, 球.data());
		}
		(void)渲染器.material_pbr_clear();
		//背板立柱（给反射一个明确参照物）
		渲染器.material_tint_set(0.88f, 0.55f, 0.35f, 1.0f);
		for (uint32_t 序号 = 0; 序号 < 4; ++序号)
		{
			const float x = -3.9f + 2.6f * static_cast<float>(序号);
			const std::array<float, 16> 柱 = 展示::非均匀变换(x, 1.9f, -113.5f, 0.24f, 2.0f, 0.24f);
			(void)渲染器.mesh_draw(资源.立方体, 柱.data());
		}
		渲染器.material_tint_clear();
	}

	/*
	后处理类展区（⑫–⑰）的链切换
	渲染层的后处理链是全局的，故沿用「进入即启用、离开即恢复」的空间开关：
	进区时按展区装一条 2 级以内的链并下发该展区的参数，离开时清空。
	*/
	void 更新后处理区(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		uint32_t 数量 = 0;
		const 展示::展区项* 表 = 展示::展区表(数量);
		//当前所在的后处理区（12–17；同时只可能在一个区内）
		uint32_t 当前区 = 0;
		for (uint32_t 编号 = 12; 编号 <= 17; ++编号)
		{
			const float 距x = 状态.位置[0] - 表[编号 - 1].x;
			const float 距z = 状态.位置[2] - 表[编号 - 1].z;
			if ((距x * 距x + 距z * 距z) < 81.0f)
			{
				当前区 = 编号;
				break;
			}
		}
		//区未变化则直接返回（避免逐帧重建链）
		if (当前区 == 状态.后处理区)
			return;
		状态.后处理区 = 当前区;
		//先清空旧链（清链会同时清除全部逐级专设值）
		(void)渲染器.post_chain_set(nullptr, 0);
		(void)渲染器.render_target_present_chain_set(nullptr, 0);

		switch (当前区)
		{
		//⑫ 泛光链：亮部提取 → 模糊 → 暗角
		case 12:
		{
			const uint64_t 链[3] = { 资源.亮部提取, 资源.模糊, 资源.暗角 };
			if (!渲染器.post_chain_set(链, 3))
				return;
			//亮部提取（阈值 / 膝 / 强度）与暗角（强度 / 半径）
			渲染器.post_stage_param_set(0, 0.72f, 0.25f, 0.85f);
			渲染器.post_stage_vec4_set(2, 0.45f, 0.85f, 1.0f, 0.0f);
			break;
		}
		//⑬ LUT ＋ 色调映射
		case 13:
		{
			const uint64_t 链[2] = { 资源.色彩查找, 资源.色调映射 };
			if (!渲染器.post_chain_set(链, 2))
				return;
			//色彩查找：混合强度取第四分量
			渲染器.post_stage_vec4_set(0, 1.0f, 1.0f, 1.0f, 0.85f);
			break;
		}
		//⑭ SSAO ＋ FXAA
		case 14:
		{
			const uint64_t 链[2] = { 资源.遮蔽, 资源.平滑 };
			if (!渲染器.post_chain_set(链, 2))
				return;
			//遮蔽：强度 / 半径 / 偏置 / 幂次；FXAA 强度取第三分量
			渲染器.post_stage_vec4_set(0, 1.0f, 0.55f, 0.02f, 1.4f);
			渲染器.post_stage_vec4_set(1, 0.0f, 0.0f, 0.85f, 0.0f);
			break;
		}
		//⑮ 深度描边 ＋ 体积雾
		case 15:
		{
			const uint64_t 链[2] = { 资源.深度描边, 资源.体积雾 };
			if (!渲染器.post_chain_set(链, 2))
				return;
			//深度描边：半径取第三分量（1..4）
			渲染器.post_stage_vec4_set(0, 0.0f, 0.0f, 2.0f, 0.0f);
			//体积雾：密度 / 高度衰减 / 最大距离 / 步数缩放 ＋ 雾色 / 噪声强度
			渲染器.post_stage_vec4_set(1, 0.22f, 0.02f, 60.0f, 0.6f);
			渲染器.post_stage_color4_set(1, 0.56f, 0.66f, 0.80f, 0.25f);
			break;
		}
		//⑯ 体积光
		case 16:
		{
			const uint64_t 链[1] = { 资源.体积光 };
			if (!渲染器.post_chain_set(链, 1))
				return;
			//强度 / 每步衰减 / 采样缩放 / 光源屏幕 uv y（0.28：略高于画面中心）
			渲染器.post_stage_vec4_set(0, 0.85f, 0.96f, 1.0f, 0.28f);
			//光色 ＋ 光源屏幕 uv x（0.5 居中）
			渲染器.post_stage_color4_set(0, 1.0f, 0.92f, 0.70f, 0.5f);
			break;
		}
		//⑰ SSR
		case 17:
		{
			const uint64_t 链[1] = { 资源.反射 };
			if (!渲染器.post_chain_set(链, 1))
				return;
			//强度 / 命中厚度 / 最大距离 / 步数缩放
			渲染器.post_stage_vec4_set(0, 0.65f, 0.28f, 6.0f, 1.0f);
			//边缘淡出宽度
			渲染器.post_stage_color4_set(0, 0.0f, 0.0f, 0.0f, 0.18f);
			break;
		}
		//不在任何后处理区：链已清空
		default:
			break;
		}
	}
}
