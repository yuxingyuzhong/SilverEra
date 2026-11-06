#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//关卡数据类型
#include "关卡数据.h"

/*
关卡文本读写（第四百二十九轮；迁入系统层后接口名与语义保持不变）
本模块只负责「关卡文本 ↔ 关卡数据」的文本层职责：
不校验资产文件是否存在、不建 GPU 资源、不做场景应用。
*/
namespace engine
{
	/*
	读取关卡文本
	行式格式：`<段> <键> <值…>`；`#` 起首为注释，空行忽略。
	  level  <名称>
	  version <1>
	  camera position|target|up|clear|fov|near|far|ortho|ortho_height|aspect <值…>
	  light  direction|color|intensity|ambient|specular|attenuation_mode|ambient_color|
	         ambient_ground|ambient_hemisphere|fill_direction|fill_color|fill_intensity|
	         specular_scale|shadow|spot_shadow|spot_shadow_index|spot_shadow_all|spot_shadow_bias_auto|spot_shadow_quality|pbr|toon|outline <值…>；
	         point / spot 逐盏一行
	         spot_shadow 形如 <0|1> [<bias 0..0.05> <softness 0..1> <strength 0..1>]
	         spot_shadow_index 为 0..7 的已登记光源槽位
	         spot_shadow_all 为 <0|1>，非 0 时前 8 盏聚光灯各自投影
	         spot_shadow_bias_auto 为 <0|1>，非 0 时逐瓦片偏置按跨度归一化
	         spot_shadow_quality 形如 <槽位 0..7> <软度缩放 0.25..4> <强度缩放 0..1>，可重复多行（第七百零八轮）
	  fog    color|density|range|mode|height_falloff <值…>
	  mesh / mtl / place <清单原文>（语法同场景清单，装载阶段解析）
	  material <实例名> <模板材质名> [tint r g b a] [emissive v] [toon steps rim specular] [pbr roughness metallic] [clearcoat strength roughness]
	浮点按 `%.9g` 写出（二进制 32 位浮点可唯一还原），故读写往返逐位一致。
	失败（路径不可读 / 超限 / 未知段或键 / 字段数不符 / 非有限值 / 光源超过 8 盏）时返回假并清空输出。
	*/
	bool level_text_read(const std::string& path, Level_Data& out);

	/*
	写出关卡文本
	按固定顺序输出全部受支持字段（未使用的点光 / 聚光槽位不写出）；
	资产段落按 `asset_lines` 原样逐行写出。路径按 UTF-8 解释；失败（路径为空 / 打不开）返回假。
	写出前校验版本 ≥ 1 与全部浮点字段有限，校验失败不落盘。
	*/
	bool level_text_write(const Level_Data& level, const std::string& path);

	/*
	查找关卡内材质实例声明（第五百六十三轮）
	按实例名在 `Level_Data.materials` 中查找首个匹配项并复制到 out；
	名称为空或未找到返回假（out 保持入参原值）。纯查表，不建资源、不修改关卡。
	*/
	bool level_material_find(const Level_Data& level, const std::string& name, Level_Material& out);

	/*
	更新关卡内材质实例声明（第五百六十七轮）
	按实例名定位首个匹配并整体替换为 data（名称须与关卡内一致）；空名 / 未找到返回假且不改关卡。
	本函数不做数值校验，写出前由 `level_text_write` 统一校验。
	*/
	bool level_material_update(Level_Data& level, const std::string& name, const Level_Material& data);

	/*
	新增关卡内材质实例声明（第五百九十三轮）
	把 data 追加到 `Level_Data.materials` 末尾（保留其中的实例名）；实例名为空、已存在同名（按首个匹配）、
	或已达声明上限（256 条）时返回假且不改关卡。本函数不做数值校验，写出前由 `level_text_write` 统一校验。
	*/
	bool level_material_add(Level_Data& level, const Level_Material& data);

	/*
	删除关卡内材质实例声明（第五百九十三轮）
	按实例名删除首个匹配项并保持其余声明原有顺序；空名 / 未找到返回假且不改关卡。
	本函数只改声明表，不回溯清理 `place` 行对已删实例的引用。
	*/
	bool level_material_remove(Level_Data& level, const std::string& name);

	/*
	构造关卡对象的变换矩阵（第四百三十一轮；列主序 4×4）
	按「缩放 → 旋转 → 平移」合成（M = T·R·S）；旋转为欧拉角（**度**），顺序 X → Y → Z（R = Rz·Ry·Rx）。
	结果可直接传给渲染层 `mesh_draw` / `mesh_draw_sections` / `mesh_draw_range`；
	旋转全零时等价于「缩放 ＋ 平移」对角矩阵（与既有手工构造一致）。
	*/
	void level_transform_build(const float position[3], const float rotation_deg[3],
		const float scale[3], float out[16]);
}
