//展区⑨⑩⑪：粒子喷泉、材质贴图扩展、文字渲染与布局
#include "src/主调/能力展示/展区公共.h"

#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace engine
{
	//展区⑨：粒子喷泉（billboard 绘制由公共设施统一提交）
	void 展区09_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		展示::绘制粒子喷泉(资源, 状态, 渲染器);
	}

	/*
	展区⑩：材质贴图扩展
	三件展品各自演示一种「材质贴图」能力：
	  · 扩展材质球：自发光贴图 ＋ AO 贴图 ＋ 金属度贴图 ＋ UV 缩放（2×2）；
	  · 镂空面板：Alpha 裁剪（棋盘纹理半透明格被丢弃）＋ UV 旋转 0.35 弧度；
	  · 贴图球：法线 / 粗糙度贴图（与展区②同一材质，做并置参照）。
	*/
	void 展区10_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		//扩展材质球（自发光条纹 + AO 对角渐变 + 金属度纵向渐变）
		if (资源.扩展材质 != 0)
		{
			渲染器.material_bind_set(资源.扩展材质);
			const std::array<float, 16> 扩展球 = 展示::变换(-16.0f, 1.0f, -70.0f, 1.0f);
			(void)渲染器.mesh_draw(资源.球, 扩展球.data());
			渲染器.material_bind_set(0);
		}
		//镂空面板（棋盘纹理按 alpha 裁剪 ＋ UV 旋转）
		if (资源.镂空材质 != 0)
		{
			渲染器.material_bind_set(资源.镂空材质);
			const std::array<float, 16> 镂空面 = 展示::非均匀变换(-11.0f, 1.6f, -73.5f, 2.6f, 2.0f, 1.0f);
			(void)渲染器.mesh_draw(资源.平面, 镂空面.data());
			渲染器.material_bind_set(0);
		}
		//贴图球（法线 / 粗糙度，与展区②并置参照）
		if (资源.贴图材质 != 0)
		{
			渲染器.material_bind_set(资源.贴图材质);
			const std::array<float, 16> 贴图球 = 展示::变换(-16.2f, 0.9f, -66.5f, 0.85f);
			(void)渲染器.mesh_draw(资源.球, 贴图球.data());
			渲染器.material_bind_set(0);
		}
		//UV 旋转的棋盘立方（同材质不同角度，直观看出贴图旋转）
		渲染器.texture_bind_set(资源.棋盘);
		const std::array<float, 16> 旋转立方 = 展示::绕Y变换(-11.6f, 0.95f, -67.0f, 状态.累计秒 * 0.4f, 0.9f);
		(void)渲染器.mesh_draw(资源.立方体, 旋转立方.data());
	}

	/*
	展区⑪：文字渲染
	面板本身是深色底板（保证文字对比度），文字按「世界锚点 → 屏幕像素」投影后绘制：
	  · 多行居中换行（text_draw_layout）；
	  · 逐帧变化的计数行（text_draw，验证字形网格缓存复用）；
	  · 字距 / 首行缩进参数一并展示。
	*/
	void 展区11_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
	{
		//深色底板（略微竖立，正对相机方向）
		渲染器.material_tint_set(0.08f, 0.10f, 0.14f, 1.0f);
		const std::array<float, 16> 底板 = 展示::非均匀变换(0.0f, 1.5f, -75.0f, 4.4f, 2.2f, 1.0f);
		(void)渲染器.mesh_draw(资源.平面, 底板.data());
		渲染器.material_tint_clear();
		//无字体则不画字（面板仍有色块）
		if (资源.字体 == 0)
			return;
		/*
		只在该区近处显示说明文字（第四百零六轮）
		本区文字较长，远看会与展区标签叠成一片；距本区中心 18 单位内才绘制。
		*/
		const float 区距x = 状态.位置[0] - 0.0f;
		const float 区距z = 状态.位置[2] - (-70.0f);
		if (区距x * 区距x + 区距z * 区距z > 18.0f * 18.0f)
			return;

		//世界锚点投影到屏幕像素（左上角原点）
		const float 锚点[3] = { -2.0f, 2.4f, -75.0f };
		float u = 0.0f;
		float v = 0.0f;
		float 深度 = 0.0f;
		if (!渲染器.camera_project(锚点, &u, &v, &深度))
			return;
		const float 屏宽 = static_cast<float>(渲染器.extent_width());
		const float 屏高 = static_cast<float>(渲染器.extent_height());
		const float 像素x = u * 屏宽;
		const float 像素y = v * 屏高;

		//文字颜色与描边（深底浅字，保证任何地台色下都可读）
		const float 字色[4] = { 0.94f, 0.97f, 1.0f, 1.0f };
		const float 描边色[3] = { 0.02f, 0.03f, 0.06f };
		渲染器.text_color_set(字色);
		渲染器.text_outline_set(0.0022f, 描边色);

		//多行居中换行（max_width 以像素计）
		Text_Layout 布局;
		布局.max_width = 340.0f;
		布局.line_height = 26.0f;
		布局.align = Text_Align::Center;
		布局.tracking = 0.5f;
		布局.first_line_indent = 12.0f;
		const std::string 正文 =
			"文字渲染：UTF-8 解码、惰性字形补烘、字距与首行缩进、"
			"按像素宽度自动换行、行内居中/右对齐、深色描边。"
			"字形图集在空间不足时自动扩容，中文标签不再缺字。";
		(void)渲染器.text_draw_layout(资源.字体, 正文, 像素x, 像素y, 布局);

		//动态计数行（逐帧变化，验证「文本 → 几何」缓存按内容区分）
		char 计数[96];
		std::snprintf(计数, sizeof(计数), "累计 %.1f 秒 · 帧 %llu · 局势 %.2f",
			状态.累计秒, static_cast<unsigned long long>(渲染器.frame_stats_get().frames), 状态.偏航);
		(void)渲染器.text_draw(资源.字体, 计数, 像素x, 像素y + 96.0f);

		渲染器.text_outline_set(0.0f, 描边色);
	}
}
