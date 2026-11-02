//展区⑥⑦⑧：纹理与动态、卡通渲染、Cluster 灯墙
#include "src/主调/能力展示/展区公共.h"

#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <vector>

namespace engine
{
	//展区⑥：纹理（棋盘面板 / 动态贴图面板 / UV 立方 / 贴图球）
	void 展区06_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		//大棋盘面板（竖直）
		渲染器.texture_bind_set(资源.棋盘);
		const std::array<float, 16> 棋盘面 = 展示::非均匀变换(10.4f, 2.0f, -34.4f, 3.0f, 2.0f, 1.0f);
		(void)渲染器.mesh_draw(资源.平面, 棋盘面.data());

		//逐帧动态更新面板（每 0.1 秒写入一个移动色块）
		if (资源.动态贴图 != 0 && 状态.累计秒 - 状态.上次更新时间 > 0.1f)
		{
			状态.上次更新时间 = 状态.累计秒;
			//色块左上角在 0..12 之间循环
			const uint32_t 色块x = static_cast<uint32_t>(状态.累计秒 * 8.0f) % 13u;
			const uint32_t 色块y = static_cast<uint32_t>(状态.累计秒 * 5.0f) % 13u;
			//4×4 纯红（RGBA8 紧排）
			std::vector<uint8_t> 色块(4 * 4 * 4, 0);
			for (std::size_t 偏移 = 0; 偏移 < 色块.size(); 偏移 += 4)
			{
				色块[偏移] = 255;
				色块[偏移 + 3] = 255;
			}
			(void)渲染器.texture_update_region(资源.动态贴图, 色块x, 色块y, 4, 4, 色块.data());
		}
		if (资源.动态贴图 != 0)
		{
			渲染器.texture_bind_set(资源.动态贴图);
			const std::array<float, 16> 动态面 = 展示::非均匀变换(14.0f, 1.8f, -34.4f, 1.8f, 1.8f, 1.0f);
			(void)渲染器.mesh_draw(资源.平面, 动态面.data());
		}
		//棋盘立方（展示立方体六面 UV）
		渲染器.texture_bind_set(资源.棋盘);
		const std::array<float, 16> 纹理立方 = 展示::绕Y变换(10.8f, 0.95f, -29.4f, 状态.累计秒 * 0.3f, 1.1f);
		(void)渲染器.mesh_draw(资源.立方体, 纹理立方.data());
		//法线 / 粗糙度贴图球
		if (资源.贴图材质 != 0)
		{
			渲染器.material_bind_set(资源.贴图材质);
			const std::array<float, 16> 贴图球 = 展示::变换(17.2f, 0.9f, -30.6f, 0.9f);
			(void)渲染器.mesh_draw(资源.球, 贴图球.data());
			渲染器.material_bind_set(0);
		}
	}

	/*
	展区⑦：卡通渲染（逐物体档位）
	三个正二十面体球环绕排布；第四百一十一轮起，三个球各自绑定一个带 `toon_override` 的材质：
	甲 = 二值化（色阶 1）、乙 = 三档 ＋ 边缘光、丙 = 不量化 ＋ 强边缘光，
	同屏三种档位并存（此前是全局切换，整屏统一档位）。
	区内的全局风格化参数（`toon_set` / 暗部色调 / 卡通高光 / 外扩描边）仍由「相机进入本区」驱动。
	*/
	void 展区07_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		if (资源.卡通球 != 0)
		{
			//逐物体卡通材质（顺序与球位一致；0 句柄回落默认材质）
			const uint64_t 材质[3] = { 资源.卡通材质甲, 资源.卡通材质乙, 资源.卡通材质丙 };
			for (uint32_t 序号 = 0; 序号 < 3; ++序号)
			{
				const float 角度 = 展示::圆周率 * 2.0f * static_cast<float>(序号) / 3.0f;
				const float x = std::cos(角度) * 2.2f;
				const float z = -50.0f + std::sin(角度) * 2.2f;
				//绑定该球的卡通档位（色调随材质下发，故不再单独设全局色调）
				if (材质[序号] != 0)
					渲染器.material_bind_set(材质[序号]);
				//球心抬高到地台上方，避免穿地
				const std::array<float, 16> 矩阵 = 展示::变换(x, 1.35f, z, 1.3f);
				(void)渲染器.mesh_draw(资源.卡通球, 矩阵.data());
			}
			//恢复默认绑定
			渲染器.material_bind_set(0);
		}

		/*
		加法混合光晕
		三层同心暖色球按「小 → 大」顺序以加法档绘制：各层前表面依次更近，深度测试逐层通过，
		亮度按层累加（体现 Additive 档「不取代背景、只叠加亮度」的语义）。
		绘制前后临时关闭外扩描边，避免光晕外层出现深色轮廓。
		*/
		if (状态.卡通区激活 && 资源.卡通球 != 0)
		{
			渲染器.outline_clear();
			渲染器.material_bind_set(资源.光晕材质);
			for (uint32_t 层 = 0; 层 < 3; ++层)
			{
				//逐层放大的球体（半径 0.46 / 0.68 / 0.90）
				const float 层缩 = 0.46f + 0.22f * static_cast<float>(层);
				渲染器.material_tint_set(0.55f, 0.35f, 0.10f, 1.0f);
				const std::array<float, 16> 矩阵 = 展示::变换(0.0f, 2.95f, -50.0f, 层缩);
				(void)渲染器.mesh_draw(资源.卡通球, 矩阵.data());
			}
			渲染器.material_tint_clear();
			渲染器.material_bind_set(0);
			//恢复本区外扩描边（与 更新进区开关 内的启用值一致）
			const float 描边色[3] = { 0.06f, 0.05f, 0.10f };
			渲染器.outline_set(0.05f, 描边色);
		}
	}

	/*
	展区⑧：Cluster 灯墙
	墙面用立方体压扁成板；128 个灯泡按 16 × 8 排布，一次实例化提交；
	灯泡对应的点光已在 场景配置 中登记，进入本区时由 更新进区开关 开启簇光照。
	*/
	void 展区08_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		(void)状态;
		//墙板（10 × 6.5 × 0.5，中心抬高到墙高一半）
		渲染器.material_tint_set(0.22f, 0.24f, 0.30f, 1.0f);
		const std::array<float, 16> 墙板 = 展示::非均匀变换(-14.0f, 3.4f, -50.0f, 10.0f, 6.5f, 0.5f);
		(void)渲染器.mesh_draw(资源.立方体, 墙板.data());

		//灯泡矩阵（与灯光位置一一对应，略靠墙前）
		std::vector<std::array<float, 16>> 灯泡矩阵;
		灯泡矩阵.reserve(128);
		for (uint32_t 行 = 0; 行 < 8; ++行)
			for (uint32_t 列 = 0; 列 < 16; ++列)
				灯泡矩阵.push_back(展示::变换(-19.0f + 0.64f * static_cast<float>(列),
					1.0f + 0.7f * static_cast<float>(行), -49.6f, 0.22f));
		渲染器.material_tint_set(1.0f, 0.86f, 0.62f, 1.0f);
		(void)渲染器.mesh_draw_instanced(资源.球, reinterpret_cast<const float*>(灯泡矩阵.data()),
			static_cast<uint32_t>(灯泡矩阵.size()));
		渲染器.material_tint_clear();
	}
}
