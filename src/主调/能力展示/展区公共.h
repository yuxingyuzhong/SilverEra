#pragma once
/*
展区公共设施（能力展示拆分版）
本文件只做声明：变换与像素工具、程序化网格数据、字体加载、展区表、公共绘制与展区绘制入口。
实现见 `展区公共.cpp`（工具与公共绘制）与 `展区_*.cpp`（逐展区绘制）。
*/
#include <array>
#include <cstdint>
#include <vector>
#include <string>

//网格数据结构（文字 / 场景网格共用；Vulkan-free）
#include "Application/Renderer/src/render/Mesh/网格数据.h"
//资源台账与展示状态（本目录内的统一上下文）
#include "src/主调/能力展示/能力展示场景.h"

namespace engine
{
	//引擎命名空间
	namespace 展示
	{
		//圆周率（环形排布与自转动画）
		inline constexpr float 圆周率 = 3.14159265f;

		//平移 ＋ 等比缩放（列主序）
		std::array<float, 16> 变换(float x, float y, float z, float 缩放);
		//平移 ＋ 三轴非等比缩放
		std::array<float, 16> 非均匀变换(float x, float y, float z, float sx, float sy, float sz);
		//绕 Y 轴旋转 ＋ 平移
		std::array<float, 16> 绕Y变换(float x, float y, float z, float 弧度, float 缩放);
		//绕 X 轴旋转 ＋ 平移（把 XY 平面网格放平）
		std::array<float, 16> 绕X变换(float x, float y, float z, float 弧度, float 缩放);

		//漫反射法线贴图（全朝 +Z）
		std::vector<uint8_t> 法线像素(uint32_t 边长);
		//粗糙度贴图（左粗右滑的横向渐变）
		std::vector<uint8_t> 粗糙像素(uint32_t 边长);
		//地板网格像素（深色底 ＋ 浅色细网格线）
		std::vector<uint8_t> 地板像素(uint32_t 边长, uint32_t 格距);
		//天空盒六面像素（自上而下的深蓝天光渐变；data 为 6 面紧排）
		std::vector<uint8_t> 天空像素(uint32_t 边长);
		//暖调查找表像素（3D LUT 的 2D 条带存放，17³）
		std::vector<uint8_t> 暖调查找像素(uint32_t 边长);

		//XY 平面四边形（法线朝 +Z；手写绕序避免内置平面被背面剔除）
		Mesh_Data 四边形数据(float 半宽, float 半高);
		//XZ 平面四边形（法线朝 +Y；反向绕序参数用于确认正面绕序）
		Mesh_Data 地面数据(float 半宽, bool 反向绕序);

		//加载可用字体（依次试候选中文字体；全失败返回 0，字幕退化为无）
		uint64_t 加载字体(Renderer& 渲染器);

		/*
		展区描述（行列式排布：前 9 区沿用原三列布局，第 10 区起落在更远的行上）
		编号用于绘制分派与图例；标题同时作为三维投影标签文本。
		*/
		struct 展区项
		{
			uint32_t 编号 = 0;
			float x = 0.0f;
			float z = 0.0f;
			float 地色[3] = { 0.5f, 0.5f, 0.5f };
			const char* 标题 = "";
		};

		//取全部展区（数量写出）；展区表是全局常量，指针生命周期与进程一致
		const 展区项* 展区表(uint32_t& 数量);

		//基础场地：地面 ＋ 各展区彩色地台 ＋ 展区四角立柱（一次实例化提交）
		void 绘制基础场地(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
		//粒子喷泉 billboard（常驻路径优先 GPU 间接 / 直接，回落 CPU 参考）
		void 绘制粒子喷泉(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
		//全展区三维投影标签（世界锚点 → 屏幕像素 → text_draw）
		void 绘制展区标签(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
		//进区开关（卡通 / 灯墙 / 粒子 / 各后处理区 / 采样 / 雾档 / 线程 / 正交）
		void 更新进区开关(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
		//关卡展区初始化（写关卡文本并装载第一套）／逐帧切换／释放（展区_关卡.cpp）
		bool 关卡区初始化(展示资源& 资源, Renderer& 渲染器);
		void 关卡区更新(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
		void 关卡区释放(Renderer& 渲染器);
	}

	//后处理类展区（⑫–⑰）的链切换（展区_后处理.cpp 实现）
	void 更新后处理区(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
	//系统类展区（⑱–㉒）的全局开关（展区_系统.cpp 实现）
	void 更新系统开关区(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);

	//—— 展区绘制入口（各展区一个绘制函数，按编号分派） ——
	//①② 程序化网格 / PBR 材质（展区_基础网格材质.cpp）
	void 展区01_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区02_绘制(展示资源&, 展示状态&, Renderer&);
	//③④⑤ 光照与阴影 / 实例化与合批 / 几何拼接（展区_光照几何.cpp）
	void 展区03_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区04_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区05_绘制(展示资源&, 展示状态&, Renderer&);
	//⑥⑦⑧ 纹理与动态 / 卡通渲染 / Cluster 灯墙（展区_纹理风格.cpp）
	void 展区06_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区07_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区08_绘制(展示资源&, 展示状态&, Renderer&);
	//⑨⑩⑪ 粒子喷泉 / 材质贴图扩展 / 文字与布局（展区_粒子文字.cpp）
	void 展区09_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区10_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区11_绘制(展示资源&, 展示状态&, Renderer&);
	//⑫–⑰ 后处理五区（展区_后处理.cpp）
	void 展区12_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区13_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区14_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区15_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区16_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区17_绘制(展示资源&, 展示状态&, Renderer&);
	//⑱–㉓ 系统类六区（展区_系统.cpp）
	void 展区18_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区19_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区20_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区21_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区22_绘制(展示资源&, 展示状态&, Renderer&);
	void 展区23_绘制(展示资源&, 展示状态&, Renderer&);
	//㉔ 关卡装载（展区_关卡.cpp）
	void 展区24_绘制(展示资源&, 展示状态&, Renderer&);

	//—— 资源与场景配置（分文件实现） ——
	//创建全部网格 / 纹理 / 管线 / 材质 / 目标 / 字体；失败返回 false
	bool 资源构建(展示资源& 资源, Renderer& 渲染器);
	//释放全部资源（幂等）
	void 资源释放(展示资源& 资源, Renderer& 渲染器);
	//初始相机、光照、雾、天空、阴影、簇灯墙、粒子预配置
	void 场景配置(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器);
}
