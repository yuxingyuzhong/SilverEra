#pragma once
/*
渲染能力展示场景（测试层入口专用，非 gtest 用例）
用途：把渲染层当前可提供的全部能力铺进**一个静态场景**，供人自由漫游观察；
      入口由图形操作界面上的按钮或命令行 --selector=showcase 提供。

维护约定：渲染层新增能力时，在「场景搭建」中补上对应资源与摆放，
          并在 Overlay 能力清单中加一行说明（见 图形操作界面.cpp）。
*/
#include <cstdint>

//引擎命名空间
namespace engine
{
	//渲染器门面（仅前置声明，本头不引入任何 Vulkan / SDL 类型）
	class Renderer;

	//一帧的相机输入（由界面层从 ImGui IO 采集后传入）
	struct 展示输入
	{
		//前后 / 左右 / 上下移动量（−1..1）
		float 前后 = 0.0f;
		float 左右 = 0.0f;
		float 上下 = 0.0f;
		//鼠标拖拽造成的偏航 / 俯仰增量（弧度）
		float 偏航增量 = 0.0f;
		float 俯仰增量 = 0.0f;
		//速度倍率（滚轮调节；默认 1）
		float 速度倍率 = 1.0f;
	};

	/*
	能力展示场景
	生命周期：init(渲染器) → 每帧 update(渲染器, 输入, 秒) ＋ frame_draw(渲染器) → shutdown(渲染器)
	init 失败返回 false（资源创建失败），调用方应退出展示模式。
	*/
	class 能力展示场景
	{
	public:
		//搭建场景与全部资源，并应用初始相机与光照
		bool init(Renderer& 渲染器);
		//按输入推进相机（秒为帧间隔）并在窗口尺寸变化时重建后处理目标
		void update(Renderer& 渲染器, const 展示输入& 输入, float 秒);
		//登记本帧全部绘制项与字幕（真正绘制在 frame_render 内发生）
		void frame_draw(Renderer& 渲染器);
		//释放全部资源（幂等）
		void shutdown(Renderer& 渲染器);

		//相机世界位置（Overlay 显示用）
		void 相机位置(float 出[3]) const;
		//当前偏航 / 俯仰（弧度；Overlay 显示用）
		float 偏航(void) const;
		float 俯仰(void) const;

	private:
		//程序化网格
		uint64_t 球_ = 0;
		uint64_t 圆柱_ = 0;
		uint64_t 圆锥_ = 0;
		uint64_t 胶囊_ = 0;
		uint64_t 圆环_ = 0;
		uint64_t 平面_ = 0;
		uint64_t 地面_ = 0;
		uint64_t 圆盘_ = 0;
		//程序化立方体（第一百四十二轮新增入口）
		uint64_t 立方体_ = 0;
		//纹理
		uint64_t 棋盘_ = 0;
		uint64_t 法线贴图_ = 0;
		uint64_t 粗糙贴图_ = 0;
		uint64_t 天空_ = 0;
		//字体
		uint64_t 字体_ = 0;
		//后处理管线
		uint64_t 亮部提取_ = 0;
		uint64_t 模糊_ = 0;
		uint64_t 暗角_ = 0;
		//后处理管线（FXAA 平滑；第一百四十一轮）
		uint64_t 平滑_ = 0;
		//3D LUT 色彩查找管线与查找表（第一百四十四轮）
		uint64_t 色彩查找_ = 0;
		uint64_t 查找表_ = 0;
		//屏幕空间遮蔽管线（第一百四十七轮）
		uint64_t 遮蔽_ = 0;
		//中间目标（同帧后处理链轮转用）
		uint64_t 目标甲_ = 0;
		uint64_t 目标乙_ = 0;
		//目标对应的交换链尺寸（尺寸变化时重建目标）
		uint32_t 目标宽_ = 0;
		uint32_t 目标高_ = 0;
		//相机状态
		float 位置_[3] = { 0.0f, 4.0f, 16.0f };
		float 偏航_ = 0.0f;
		float 俯仰_ = -0.12f;
		//累计时间（驱动动画，如圆环自转）
		float 累计秒_ = 0.0f;
	};
}
