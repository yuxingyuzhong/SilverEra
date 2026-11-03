//展区⑱–㉓：系统类展区（采样、雾模式、计算管线、多线程、正交投影、读回统计）
#include "src/主调/能力展示/展区公共.h"

#include "Engine/EngineSystem/src/core/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <vector>

namespace engine
{
	//展区⑱：超采样 / 多重采样（细杆与斜边便于观察锯齿）
	void 展区18_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//斜置细杆阵（观察 MSAA / SSAA 对斜边的平滑）
		渲染器.material_tint_set(0.90f, 0.88f, 0.72f, 1.0f);
		for (uint32_t 序号 = 0; 序号 < 10; ++序号)
		{
			const float x = 10.6f + 0.72f * static_cast<float>(序号);
			const std::array<float, 16> 杆 = 展示::绕X变换(x, 1.5f, -110.0f, 0.55f, 1.0f);
			(void)渲染器.mesh_draw(资源.立方体, 杆.data());
		}
		渲染器.material_tint_clear();
		//高频棋盘面板（锯齿在近处最明显）
		if (资源.棋盘 != 0)
		{
			渲染器.texture_bind_set(资源.棋盘);
			const std::array<float, 16> 面板 = 展示::非均匀变换(14.0f, 1.6f, -114.5f, 3.0f, 2.0f, 1.0f);
			(void)渲染器.mesh_draw(资源.平面, 面板.data());
		}
	}

	//展区⑲：雾三模式 —— 等距立柱，直观看出不同模式的浓度曲线
	void 展区19_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		for (uint32_t 序号 = 0; 序号 < 7; ++序号)
		{
			const float z = -128.0f - 3.0f * static_cast<float>(序号);
			const float 亮度 = 0.82f - 0.09f * static_cast<float>(序号);
			渲染器.material_tint_set(亮度, 亮度 * 0.92f, 亮度 * 0.80f, 1.0f);
			const std::array<float, 16> 柱 = 展示::非均匀变换(-17.2f + 1.2f * static_cast<float>(序号),
				1.4f, z, 0.35f, 1.4f, 0.35f);
			(void)渲染器.mesh_draw(资源.立方体, 柱.data());
		}
		渲染器.material_tint_clear();
	}

	//展区⑳：计算管线（簇光照）—— 小型灯墙 ＋ 受光灯球
	void 展区20_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//灯墙底板
		渲染器.material_tint_set(0.16f, 0.18f, 0.24f, 1.0f);
		const std::array<float, 16> 底板 = 展示::非均匀变换(0.0f, 2.6f, -134.0f, 4.5f, 2.6f, 0.4f);
		(void)渲染器.mesh_draw(资源.立方体, 底板.data());
		渲染器.material_tint_clear();
		//16 个受光电球（彩色）
		for (uint32_t 序号 = 0; 序号 < 16; ++序号)
		{
			const float 角度 = 展示::圆周率 * 2.0f * static_cast<float>(序号) / 16.0f;
			const float x = std::cos(角度) * 2.2f;
			const float z = -130.0f + std::sin(角度) * 2.2f;
			渲染器.material_tint_set(0.55f + 0.45f * std::cos(角度), 0.62f, 0.72f + 0.28f * std::sin(角度), 1.0f);
			const std::array<float, 16> 球 = 展示::变换(x, 0.75f, z, 0.4f);
			(void)渲染器.mesh_draw(资源.球, 球.data());
		}
		渲染器.material_tint_clear();
	}

	//展区㉑：多线程并行录制 —— 大批量实例（进区切 4 线程，区外单线程）
	void 展区21_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		std::vector<float> 批次;
		批次.reserve(216 * 16);
		for (uint32_t 层 = 0; 层 < 6; ++层)
		{
			for (uint32_t 行 = 0; 行 < 6; ++行)
			{
				for (uint32_t 列 = 0; 列 < 6; ++列)
				{
					const float x = 14.0f + 0.9f * (static_cast<float>(列) - 2.5f);
					const float z = -130.0f + 0.9f * (static_cast<float>(行) - 2.5f);
					const float y = 0.28f + 0.5f * static_cast<float>(层);
					const std::array<float, 16> 块 = 展示::变换(x, y, z, 0.34f);
					批次.insert(批次.end(), 块.begin(), 块.end());
				}
			}
		}
		渲染器.material_tint_set(0.52f, 0.70f, 0.86f, 1.0f);
		(void)渲染器.mesh_draw_instanced(资源.立方体, 批次.data(), static_cast<uint32_t>(批次.size() / 16));
		渲染器.material_tint_clear();
	}

	//展区㉒：正交投影 —— 纵深等宽阵列（透视下会逐层缩小，正交下不变）
	void 展区22_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		for (uint32_t 序号 = 0; 序号 < 6; ++序号)
		{
			const float z = -146.0f - 3.0f * static_cast<float>(序号);
			渲染器.material_tint_set(0.85f - 0.08f * static_cast<float>(序号),
				0.62f + 0.04f * static_cast<float>(序号), 0.42f, 1.0f);
			//正面看到完整方块（正交下各层屏幕尺寸一致）
			const std::array<float, 16> 方块 = 展示::变换(-15.4f, 1.2f, z, 0.9f);
			(void)渲染器.mesh_draw(资源.立方体, 方块.data());
			//侧面薄板（观察投影方向）
			const std::array<float, 16> 薄板 = 展示::非均匀变换(-11.4f, 1.2f, z, 0.06f, 1.2f, 1.2f);
			(void)渲染器.mesh_draw(资源.立方体, 薄板.data());
		}
		渲染器.material_tint_clear();
	}

	//展区㉓：读回与统计观测 —— 旋转环阵（数据面板由叠加层显示）
	void 展区23_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		//三层旋转环 ＋ 中心球
		渲染器.material_tint_set(0.60f, 0.84f, 0.92f, 1.0f);
		for (uint32_t 序号 = 0; 序号 < 3; ++序号)
		{
			const float 角度 = 状态.累计秒 * (0.5f + 0.25f * static_cast<float>(序号));
			const float 倾角 = 0.5f * static_cast<float>(序号);
			const std::array<float, 16> 环 = 展示::绕Y变换(0.0f, 1.2f + 0.4f * static_cast<float>(序号), -150.0f,
				角度, 1.6f - 0.35f * static_cast<float>(序号));
			(void)渲染器.mesh_draw(资源.圆环, 环.data());
			(void)倾角;
		}
		渲染器.material_tint_clear();
		渲染器.material_emissive_set(2.0f);
		渲染器.material_tint_set(0.95f, 0.75f, 0.40f, 1.0f);
		const std::array<float, 16> 核心 = 展示::变换(0.0f, 1.2f, -150.0f, 0.42f);
		(void)渲染器.mesh_draw(资源.卡通球, 核心.data());
		渲染器.material_emissive_clear();
		渲染器.material_tint_clear();
	}

	/*
	系统类展区（⑱–㉒）的全局开关
	进入区即改变全局状态，离开即恢复默认；仅在状态变化时调用门面。
	*/
	void 更新系统开关区(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)资源;
		uint32_t 数量 = 0;
		const 展示::展区项* 表 = 展示::展区表(数量);
		//相机到第 N 区的水平距离平方判定（半径 9）
		auto 区内 = [&](uint32_t 编号)
		{
			const float 距x = 状态.位置[0] - 表[编号 - 1].x;
			const float 距z = 状态.位置[2] - 表[编号 - 1].z;
			return (距x * 距x + 距z * 距z) < 81.0f;
		};

		//⑱ 采样：区内升到 2× SSAA，区外回到 1×（MSAA 恒 4×）
		const bool 进入采样区 = 区内(18);
		if (进入采样区 != 状态.采样区激活)
		{
			状态.采样区激活 = 进入采样区;
			渲染器.ssaa_factor_set(进入采样区 ? 2u : 1u);
		}

		/*
		⑲ 雾模式：区内每 3 秒轮换一档（1 = 指数平方 / 2 = 线性 / 3 = 高度衰减）；
		档位 0 表示「不在本区」，离开时恢复默认线性雾。
		*/
		const bool 进入雾区 = 区内(19);
		if (进入雾区)
		{
			const uint32_t 档 = (static_cast<uint32_t>(状态.累计秒 / 3.0f) % 3u) + 1u;
			if (档 != 状态.雾档)
			{
				状态.雾档 = 档;
				const float 雾色[3] = { 0.52f, 0.62f, 0.78f };
				if (档 == 1)
					渲染器.fog_exp2_set(雾色, 0.020f, 4.0f);
				else if (档 == 2)
					渲染器.fog_set(雾色, 0.0f, 40.0f, 200.0f);
				else
					//高度衰减档：先退回线性雾，再叠加更强的贴地浓雾
					{
						渲染器.fog_set(雾色, 0.0f, 20.0f, 190.0f);
						渲染器.fog_height_set(0.060f);
					}
			}
		}
		//离开雾区：恢复默认线性雾与默认高度衰减
		else if (状态.雾档 != 0)
		{
			状态.雾档 = 0;
			const float 雾色[3] = { 0.52f, 0.62f, 0.78f };
			渲染器.fog_set(雾色, 0.0f, 60.0f, 240.0f);
			渲染器.fog_height_set(0.012f);
		}

		//㉑ 多线程：区内 4 工作线程，区外单线程（并行段数面板可观察）
		const bool 进入线程区 = 区内(21);
		if (进入线程区 != 状态.线程区激活)
		{
			状态.线程区激活 = 进入线程区;
			渲染器.render_threads_set(进入线程区 ? 4u : 0u);
		}

		//㉒ 正交投影：区内改正交（垂直可见高度 14 世界单位），区外恢复透视
		const bool 进入正交区 = 区内(22);
		if (进入正交区 != 状态.正交区激活)
		{
			状态.正交区激活 = 进入正交区;
			渲染器.camera_ortho_set(进入正交区, 14.0f);
		}
	}
}
