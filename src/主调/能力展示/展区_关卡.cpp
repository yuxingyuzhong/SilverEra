/*
关卡展区（第四百三十二轮 · 方向六：场景序列化与关卡装载 · 阶段 D）
用途：展区㉔ 演示「关卡即数据」——两套关卡文本交替装载，资产与摆放全部来自文件。
切换周期 5 秒：卸载当前关卡资产 → 装载另一关卡（只装配资产，不动展示层全局相机 / 光照 / 雾）。
*/
#include "src/主调/能力展示/展区公共.h"

#include "Application/Renderer/src/render/Renderer/渲染器.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	//关卡切换周期（秒）
	constexpr float 切换周期 = 5.0f;
	//两套关卡（相对可执行文件目录；UTF-8 入参）
	const char* const 关卡路径[2] = { "out/关卡展示/关卡甲.level", "out/关卡展示/关卡乙.level" };

	//当前装配结果与关卡数据
	engine::Asset_Scene 关卡场景;
	engine::Level_Data 关卡数据;
	//当前关卡序号与上次切换时刻
	uint32_t 当前关卡 = 0;
	float 上次切换 = 0.0f;
	//是否已成功装载过（未装载时绘制 / 切换均为空操作）
	bool 已装载 = false;

	//关卡展示目录（宽字符写盘路径）
	std::filesystem::path 关卡目录(void)
	{
		return std::filesystem::path(L"out") / std::filesystem::path(L"关卡展示");
	}

	//写 UTF-8 文本（目录自动创建）
	void 写文本(const std::filesystem::path& path, const std::string& text)
	{
		std::error_code code;
		std::filesystem::create_directories(path.parent_path(), code);
		std::ofstream file(path, std::ios::binary);
		file << text;
	}

	//准备资产与两套关卡文本（面板网格 ＋ 单材质 ＋ 摆放）
	bool 准备文件(void)
	{
		//XY 平面面板（法线朝 +Z）
		写文本(关卡目录() / L"面板.obj",
			"v -1 -1 0\n"
			"v 1 -1 0\n"
			"v 1 1 0\n"
			"v -1 1 0\n"
			"vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
			"vn 0 0 1\n"
			"usemtl 面板材质\n"
			"f 1/1/1 2/2/1 3/3/1\n"
			"f 1/1/1 3/3/1 4/4/1\n");
		//单材质（暖橙色）
		写文本(关卡目录() / L"面板.mtl", "newmtl 面板材质\nKd 0.85 0.35 0.30\nNs 64\n");
		//关卡甲：三块正立面板
		写文本(关卡目录() / L"关卡甲.level",
			"level 关卡甲\n"
			"mesh 面板 面板.obj\n"
			"mtl 面板材质 面板.mtl\n"
			"place 面板 面板材质 11 1.4 -148 1 1 1 0 0 0\n"
			"place 面板 面板材质 14 2.0 -150 1 1 1 0 20 0\n"
			"place 面板 面板材质 17 1.4 -152 1 1 1 0 -20 0\n");
		//关卡乙：四块倾斜面板（明显不同的布局与旋转）
		写文本(关卡目录() / L"关卡乙.level",
			"level 关卡乙\n"
			"mesh 面板 面板.obj\n"
			"mtl 面板材质 面板.mtl\n"
			"place 面板 面板材质 12 1.2 -149 1.2 1.2 1.2 0 45 0\n"
			"place 面板 面板材质 16 1.2 -149 1.2 1.2 1.2 0 -45 0\n"
			"place 面板 面板材质 12 2.4 -151 1 1 1 30 0 0\n"
			"place 面板 面板材质 16 2.4 -151 1 1 1 -30 0 0\n");
		return true;
	}

	//装载指定序号关卡（先卸旧；只装配资产，不应用相机 / 光照 / 雾）
	bool 装载(engine::Renderer& 渲染器, uint32_t 序号)
	{
		//卸载当前资产
		if (已装载)
			渲染器.level_unload(关卡场景);
		已装载 = false;
		//装载新关卡
		if (!渲染器.level_load(关卡数据, 关卡场景, 关卡路径[序号], false))
		{
			std::cout << "[关卡展示] 装载失败：" << 关卡路径[序号] << std::endl;
			return false;
		}
		当前关卡 = 序号;
		已装载 = true;
		return true;
	}
}

//初始化关卡展区（写文件并装载第一套）
bool engine::展示::关卡区初始化(展示资源& 资源, Renderer& 渲染器)
{
	(void)资源;
	if (!准备文件())
		return false;
	上次切换 = 0.0f;
	return 装载(渲染器, 0);
}

//逐帧更新：到周期即切换另一套关卡
void engine::展示::关卡区更新(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
{
	(void)资源;
	if (!已装载)
		return;
	if (状态.累计秒 - 上次切换 < 切换周期)
		return;
	上次切换 = 状态.累计秒;
	装载(渲染器, (当前关卡 + 1u) % 2u);
}

//绘制当前关卡的全部对象（位置 / 旋转 / 缩放来自关卡文件）
void engine::展区24_绘制(展示资源& 资源, 展示状态& 状态, Renderer& 渲染器)
{
	(void)资源;
	(void)状态;
	if (!已装载)
		return;
	//逐对象登记绘制（门面 scene_draw：位置 / 旋转 / 缩放 ＋ 逐段绑材质）
	(void)渲染器.scene_draw(关卡场景);
}

//释放关卡资产（幂等）
void engine::展示::关卡区释放(Renderer& 渲染器)
{
	if (!已装载)
		return;
	渲染器.level_unload(关卡场景);
	已装载 = false;
}
