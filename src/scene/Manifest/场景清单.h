#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"

/*
场景清单（第四百二十四轮迁入系统层）
纯文本行格式（UTF-8，一行一条，`#` 起首为注释）：
  mesh  <网格ID> <OBJ 路径>
  mtl   <材质ID> <MTL 路径>
  place <网格ID> [材质ID] <x> <y> <z> <sx> <sy> <sz> [<rx> <ry> <rz>]
路径相对清单文件所在目录；`place` 的材质列可省略（表示无材质）；
旋转三轴可省略（度，缺省 0 = 不旋转）。
本模块只做文本解析，不建任何 GPU 资源；资源创建与摆放由应用层装配桥完成。
*/
namespace engine
{
	//清单里的网格引用
	struct Scene_Mesh_Ref
	{
		//网格 ID（清单内唯一）
		std::string id;
		//OBJ 路径（相对清单目录）
		std::string path;
	};

	//清单里的材质库引用
	struct Scene_Mtl_Ref
	{
		//材质 ID（清单内唯一）
		std::string id;
		//MTL 路径（相对清单目录）
		std::string path;
	};

	//清单里的一个摆放记录
	struct Scene_Place
	{
		//网格 ID
		std::string mesh_id;
		//材质 ID（空串表示无材质）
		std::string material_id;
		//位置（世界坐标）
		float position[3] = { 0.0f, 0.0f, 0.0f };
		//缩放（三轴）
		float scale[3] = { 1.0f, 1.0f, 1.0f };
		//欧拉角旋转（度；顺序 X → Y → Z，缺省全零 = 不旋转）
		float rotation_deg[3] = { 0.0f, 0.0f, 0.0f };
	};

	//整份清单
	struct Scene_Manifest
	{
		//网格引用
		std::vector<Scene_Mesh_Ref> meshes;
		//材质库引用
		std::vector<Scene_Mtl_Ref> materials;
		//摆放记录
		std::vector<Scene_Place> places;
	};

	/*
	解析场景清单文件
	失败（路径不可读 / 超限 / 未知指令 / 字段缺失 / ID 重复 / 未含 place）返回假并清空输出；
	仅做语法与「ID 是否被引用」的静态检查，不校验 OBJ / MTL 文件是否存在。
	`实例名表` 为关卡材质实例声明；`place` 的材质列命中实例名即视为合法，未命中再查材质库。
	*/
	bool scene_manifest_parse(const std::string& path, Scene_Manifest& out,
		const std::vector<std::string>& 实例名表 = {});

	/*
	按内存行解析场景清单
	行内容与 `scene_manifest_parse` 完全同语法；供关卡装载复用（关卡文件里的资产段落不落成独立清单文件）。
	失败口径同文件版（含「至少一条 place」），失败时输出清空。
	*/
	bool scene_manifest_parse_lines(const std::vector<std::string>& lines, Scene_Manifest& out,
		const std::vector<std::string>& 实例名表 = {});
}
