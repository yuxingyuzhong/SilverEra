//展示资源构建与释放：网格 / 纹理 / 后处理管线 / 材质 / 中间目标 / 字体
#include "src/主调/能力展示/展区公共.h"

//渲染器门面（Vulkan-free 公共面；材质描述亦在其中）
#include "Engine/EngineSystem/src/core/Renderer/src/render/Renderer/渲染器.h"

#include <cstdint>
#include <vector>

namespace engine
{
	//引擎命名空间
	namespace
	{
		//自发光贴图（横向暖色条纹，乘到反照率上）
		std::vector<uint8_t> 自发光像素(uint32_t 边长)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
			for (uint32_t y = 0; y < 边长; ++y)
			{
				for (uint32_t x = 0; x < 边长; ++x)
				{
					//条纹周期 8 像素：亮带 255、暗带 40
					const bool 亮带 = (x / 8) % 2 == 0;
					std::size_t 偏移 = (static_cast<std::size_t>(y) * 边长 + x) * 4;
					像素[偏移] = 亮带 ? 255 : 60;
					像素[偏移 + 1] = 亮带 ? 190 : 40;
					像素[偏移 + 2] = 亮带 ? 90 : 20;
					像素[偏移 + 3] = 255;
				}
			}
			return 像素;
		}

		//环境光遮蔽贴图（左上亮 → 右下暗的对角渐变）
		std::vector<uint8_t> 遮蔽像素(uint32_t 边长)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
			const float 归一 = 边长 > 1 ? 1.0f / static_cast<float>(边长 - 1) : 0.0f;
			for (uint32_t y = 0; y < 边长; ++y)
			{
				for (uint32_t x = 0; x < 边长; ++x)
				{
					float t = 0.5f * (static_cast<float>(x) * 归一 + static_cast<float>(y) * 归一);
					uint8_t 值 = static_cast<uint8_t>(255.0f * (1.0f - t));
					std::size_t 偏移 = (static_cast<std::size_t>(y) * 边长 + x) * 4;
					像素[偏移] = 值;
					像素[偏移 + 1] = 值;
					像素[偏移 + 2] = 值;
					像素[偏移 + 3] = 255;
				}
			}
			return 像素;
		}

		//金属度贴图（下金属 → 上非金属的纵向渐变）
		std::vector<uint8_t> 金属像素(uint32_t 边长)
		{
			std::vector<uint8_t> 像素(static_cast<std::size_t>(边长) * 边长 * 4);
			const float 归一 = 边长 > 1 ? 1.0f / static_cast<float>(边长 - 1) : 0.0f;
			for (uint32_t y = 0; y < 边长; ++y)
			{
				uint8_t 值 = static_cast<uint8_t>(255.0f * (1.0f - static_cast<float>(y) * 归一));
				for (uint32_t x = 0; x < 边长; ++x)
				{
					std::size_t 偏移 = (static_cast<std::size_t>(y) * 边长 + x) * 4;
					像素[偏移] = 值;
					像素[偏移 + 1] = 值;
					像素[偏移 + 2] = 值;
					像素[偏移 + 3] = 255;
				}
			}
			return 像素;
		}
	}

	//创建全部资源（任一失败即返回假，已建部分由调用方 shutdown 释放）
	bool 资源构建(展示资源& 资源, Renderer& 渲染器)
	{
		//—— 网格（程序化图元） ——
		if (!渲染器.mesh_sphere_load(资源.球, 1.0f, 32, 16))
			return false;
		if (!渲染器.mesh_cylinder_load(资源.圆柱, 0.8f, 1.8f, 24))
			return false;
		if (!渲染器.mesh_cone_load(资源.圆锥, 0.9f, 1.8f, 24))
			return false;
		if (!渲染器.mesh_capsule_load(资源.胶囊, 0.6f, 1.0f, 24, 8))
			return false;
		if (!渲染器.mesh_torus_load(资源.圆环, 0.8f, 0.3f, 32, 12))
			return false;
		//手写四边形（避免内置平面在部分视角被背面剔除）
		if (!渲染器.mesh_load(资源.平面, 展示::四边形数据(0.9f, 0.9f)))
			return false;
		if (!渲染器.mesh_load(资源.地面, 展示::地面数据(20.0f, false)))
			return false;
		if (!渲染器.mesh_load(资源.地面反向, 展示::地面数据(20.0f, true)))
			return false;
		if (!渲染器.mesh_disc_load(资源.圆盘, 0.9f, 32))
			return false;
		if (!渲染器.mesh_cube_load(资源.立方体, 0.8f))
			return false;
		//四棱锥（硬边法线 ＋ 背面剔除绕序的观察素材）
		if (!渲染器.mesh_pyramid_load(资源.四棱锥, 0.7f, 1.6f))
			return false;
		//正二十面体球（卡通渲染区；纯三角面拓扑）
		if (!渲染器.mesh_icosphere_load(资源.卡通球, 1.0f, 2))
			return false;
		//水平细分网格（展区①的细分地形底板）
		if (!渲染器.mesh_ground_grid_load(资源.地形, 9.0f, 9.0f, 8, 8))
			return false;
		//三角形（图元清单补全；无参：内置默认尺寸）
		if (!渲染器.mesh_triangle_load(资源.三角))
			return false;
		/*
		带子网格分段的四边形（第四百一十六轮；展区⑩ 多材质面板）
		两个三角形各一段（段 0 → 材质槽 0、段 1 → 材质槽 1），经 mesh_load_sections 单独传入段描述
		（分段不放进 Mesh_Data，避免与 EngineCore 同名类型的 ODR 冲突）。
		*/
		Mesh_Data 分段数据 = 展示::四边形数据(0.9f, 0.9f);
		const Mesh_Section 分段描述[2] = { { 0u, 3u, 0u }, { 3u, 3u, 1u } };
		if (!渲染器.mesh_load_sections(资源.分段平面, 分段数据, 分段描述, 2))
			return false;

		//—— 纹理 ——
		if (!渲染器.texture_load_checkerboard(资源.棋盘, 256, 256, 32))
			return false;
		const std::vector<uint8_t> 地板 = 展示::地板像素(256, 64);
		if (!渲染器.texture_load(资源.地板, 地板.data(), 256, 256))
			return false;
		const std::vector<uint8_t> 法线 = 展示::法线像素(64);
		if (!渲染器.texture_load(资源.法线贴图, 法线.data(), 64, 64))
			return false;
		const std::vector<uint8_t> 粗糙 = 展示::粗糙像素(64);
		if (!渲染器.texture_load(资源.粗糙贴图, 粗糙.data(), 64, 64))
			return false;
		const std::vector<uint8_t> 天空 = 展示::天空像素(64);
		if (!渲染器.texture_cube_load(资源.天空, 天空.data(), 64, 64))
			return false;
		//动态贴图（16×16 棋盘，每 0.1 秒更新一个移动色块）
		if (!渲染器.texture_load_checkerboard(资源.动态贴图, 16, 16, 4))
			return false;
		const std::vector<uint8_t> 自发光 = 自发光像素(64);
		if (!渲染器.texture_load(资源.自发光贴图, 自发光.data(), 64, 64))
			return false;
		const std::vector<uint8_t> 遮蔽 = 遮蔽像素(64);
		if (!渲染器.texture_load(资源.遮蔽贴图, 遮蔽.data(), 64, 64))
			return false;
		const std::vector<uint8_t> 金属 = 金属像素(64);
		if (!渲染器.texture_load(资源.金属贴图, 金属.data(), 64, 64))
			return false;

		//—— 后处理管线（统一顶点着色器 后处理.vert.spv） ——
		if (!渲染器.pipeline_load(资源.亮部提取, "后处理.vert.spv", "亮部提取.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.模糊, "后处理.vert.spv", "模糊.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.暗角, "后处理.vert.spv", "暗角.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.平滑, "后处理.vert.spv", "FXAA.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.色彩查找, "后处理.vert.spv", "色彩查找.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.遮蔽, "后处理.vert.spv", "屏幕空间遮蔽.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.色调映射, "后处理.vert.spv", "色调映射.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.深度描边, "后处理.vert.spv", "深度描边.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.体积雾, "后处理.vert.spv", "体积雾.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.体积光, "后处理.vert.spv", "体积光.frag.spv"))
			return false;
		if (!渲染器.pipeline_load(资源.反射, "后处理.vert.spv", "屏幕空间反射.frag.spv"))
			return false;
		//3D LUT 查找表（17³ 暖调）＋ 绑定到后处理采样集
		const std::vector<uint8_t> 查找像素 = 展示::暖调查找像素(17);
		if (!渲染器.texture_load(资源.查找表, 查找像素.data(), 17 * 17, 17))
			return false;
		渲染器.post_lut_bind(资源.查找表);

		//—— 材质 ——
		Material_Data 地面描述;
		地面描述.texture = 资源.地板;
		地面描述.double_sided = true;
		地面描述.pbr_override = true;
		地面描述.pbr_roughness = 1.0f;
		地面描述.pbr_metallic = 0.0f;
		地面描述.uv_scale[0] = 6.0f;
		地面描述.uv_scale[1] = 6.0f;
		if (!渲染器.material_load(资源.地面材质, 地面描述))
			return false;

		Material_Data 贴图描述;
		贴图描述.pbr_override = true;
		贴图描述.pbr_roughness = 0.7f;
		贴图描述.pbr_metallic = 0.35f;
		贴图描述.normal_texture = 资源.法线贴图;
		贴图描述.roughness_texture = 资源.粗糙贴图;
		贴图描述.normal_strength = 1.5f;
		if (!渲染器.material_load(资源.贴图材质, 贴图描述))
			return false;

		//加法混合材质（卡通区光晕专用；共享管线按混合档缓存）
		Material_Data 光晕描述;
		光晕描述.blend = Blend_Mode::Additive;
		if (!渲染器.material_load(资源.光晕材质, 光晕描述))
			return false;

		//清漆层材质（与展区②素坯球同粗糙度 / 同金属度，唯一差异是清漆）
		Material_Data 清漆描述;
		清漆描述.pbr_override = true;
		清漆描述.pbr_roughness = 0.5f;
		清漆描述.pbr_metallic = 0.2f;
		清漆描述.clear_coat = 1.0f;
		清漆描述.clear_coat_roughness = 1.0f;
		if (!渲染器.material_load(资源.清漆材质, 清漆描述))
			return false;

		//镂空材质（棋盘纹理 ＋ Alpha 裁剪 ＋ UV 旋转；展区⑩）
		Material_Data 镂空描述;
		镂空描述.texture = 资源.棋盘;
		镂空描述.alpha_cutoff = 0.5f;
		镂空描述.uv_rotation = 0.35f;
		镂空描述.double_sided = true;
		if (!渲染器.material_load(资源.镂空材质, 镂空描述))
			return false;

		//贴图扩展材质（自发光 / AO / 金属度贴图 ＋ UV 缩放；展区⑩）
		Material_Data 扩展描述;
		扩展描述.pbr_override = true;
		扩展描述.pbr_roughness = 0.45f;
		扩展描述.pbr_metallic = 0.6f;
		扩展描述.emissive_texture = 资源.自发光贴图;
		扩展描述.ao_texture = 资源.遮蔽贴图;
		扩展描述.metallic_texture = 资源.金属贴图;
		扩展描述.uv_scale[0] = 2.0f;
		扩展描述.uv_scale[1] = 2.0f;
		if (!渲染器.material_load(资源.扩展材质, 扩展描述))
			return false;

		/*
		逐物体卡通材质（第四百一十一轮；展区⑦ 三个档位）
		同一场景三种档位并存：甲 = 二值化、乙 = 三档 ＋ 边缘光、丙 = 不量化 ＋ 强边缘光；
		tint 保留原有三色（暖橙 / 冷蓝 / 米黄），否则球会变白。
		*/
		Material_Data 卡通甲描述;
		卡通甲描述.toon_override = true;
		/*
		二值化改为三档
		单档二值化把球面直接切成「深蓝暗面 ＋ 浅色亮面」两块，边界像被削掉一角（观感上是缺面）；
		改三档后仍有明显色阶，但过渡不再是单一切口。
		*/
		卡通甲描述.toon_steps = 3.0f;
		卡通甲描述.tint_override = true;
		卡通甲描述.tint[0] = 1.0f;
		卡通甲描述.tint[1] = 0.62f;
		卡通甲描述.tint[2] = 0.45f;
		卡通甲描述.tint[3] = 1.0f;
		//暗部色调提亮（旧口径下球面暗部只剩深蓝大块，亮部一块硬边暖色，观感像缺面）
		卡通甲描述.toon_shadow_tint[0] = 0.66f;
		卡通甲描述.toon_shadow_tint[1] = 0.70f;
		卡通甲描述.toon_shadow_tint[2] = 0.88f;
		if (!渲染器.material_load(资源.卡通材质甲, 卡通甲描述))
			return false;
		Material_Data 卡通乙描述;
		卡通乙描述.toon_override = true;
		卡通乙描述.toon_steps = 3.0f;
		卡通乙描述.toon_rim = 0.6f;
		卡通乙描述.tint_override = true;
		卡通乙描述.tint[0] = 0.55f;
		卡通乙描述.tint[1] = 0.85f;
		卡通乙描述.tint[2] = 1.0f;
		卡通乙描述.tint[3] = 1.0f;
		卡通乙描述.toon_shadow_tint[0] = 0.66f;
		卡通乙描述.toon_shadow_tint[1] = 0.70f;
		卡通乙描述.toon_shadow_tint[2] = 0.88f;
		if (!渲染器.material_load(资源.卡通材质乙, 卡通乙描述))
			return false;
		Material_Data 卡通丙描述;
		卡通丙描述.toon_override = true;
		卡通丙描述.toon_steps = 0.0f;
		卡通丙描述.toon_rim = 1.0f;
		卡通丙描述.tint_override = true;
		卡通丙描述.tint[0] = 0.95f;
		卡通丙描述.tint[1] = 0.85f;
		卡通丙描述.tint[2] = 0.45f;
		卡通丙描述.tint[3] = 1.0f;
		卡通丙描述.toon_shadow_tint[0] = 0.66f;
		卡通丙描述.toon_shadow_tint[1] = 0.70f;
		卡通丙描述.toon_shadow_tint[2] = 0.88f;
		if (!渲染器.material_load(资源.卡通材质丙, 卡通丙描述))
			return false;

		//多材质分段材质（第四百一十一轮；展区⑩ 红 / 蓝两块，仅反照率色调不同）
		Material_Data 分段甲描述;
		分段甲描述.tint_override = true;
		分段甲描述.tint[0] = 1.0f;
		分段甲描述.tint[1] = 0.25f;
		分段甲描述.tint[2] = 0.25f;
		分段甲描述.tint[3] = 1.0f;
		if (!渲染器.material_load(资源.分段材质甲, 分段甲描述))
			return false;
		Material_Data 分段乙描述;
		分段乙描述.tint_override = true;
		分段乙描述.tint[0] = 0.25f;
		分段乙描述.tint[1] = 0.35f;
		分段乙描述.tint[2] = 1.0f;
		分段乙描述.tint[3] = 1.0f;
		if (!渲染器.material_load(资源.分段材质乙, 分段乙描述))
			return false;

		/*
		双面材质（展区① 的平面 / 圆盘 / 三角）
		关闭背面剔除后，二维图元从任意方向都能看到完整面；色调仍由逐项 material_tint_set 下发。
		*/
		Material_Data 双面描述;
		双面描述.double_sided = true;
		if (!渲染器.material_load(资源.双面材质, 双面描述))
			return false;

		//—— 中间目标（与交换链同尺寸，后处理 1:1 取源） ——
		资源.目标宽 = 渲染器.extent_width();
		资源.目标高 = 渲染器.extent_height();
		if (!渲染器.render_target_create(资源.目标甲, 资源.目标宽, 资源.目标高))
			return false;
		if (!渲染器.render_target_create(资源.目标乙, 资源.目标宽, 资源.目标高))
			return false;

		//—— 字体（失败不致命：仅三维标签缺失，叠加层仍有图例） ——
		资源.字体 = 展示::加载字体(渲染器);
		return true;
	}

	//释放全部资源（幂等；先退后处理链，避免目标被采样时销毁）
	void 资源释放(展示资源& 资源, Renderer& 渲染器)
	{
		(void)渲染器.post_chain_set(nullptr, 0);
		(void)渲染器.render_target_present_chain_set(nullptr, 0);
		if (资源.目标乙 != 0)
			渲染器.render_target_destroy(资源.目标乙);
		if (资源.目标甲 != 0)
			渲染器.render_target_destroy(资源.目标甲);
		资源.目标乙 = 0;
		资源.目标甲 = 0;

		//后处理管线
		const uint64_t 管线[] = { 资源.亮部提取, 资源.模糊, 资源.暗角, 资源.平滑, 资源.色彩查找,
			资源.遮蔽, 资源.色调映射, 资源.深度描边, 资源.体积雾, 资源.体积光, 资源.反射 };
		for (uint64_t 句柄 : 管线)
		{
		 if (句柄 != 0)
			渲染器.pipeline_unload(句柄);
		}
		资源.亮部提取 = 0;
		资源.模糊 = 0;
		资源.暗角 = 0;
		资源.平滑 = 0;
		资源.色彩查找 = 0;
		资源.遮蔽 = 0;
		资源.色调映射 = 0;
		资源.深度描边 = 0;
		资源.体积雾 = 0;
		资源.体积光 = 0;
		资源.反射 = 0;

		//材质
		if (资源.贴图材质 != 0)
			渲染器.material_unload(资源.贴图材质);
		if (资源.光晕材质 != 0)
			渲染器.material_unload(资源.光晕材质);
		if (资源.清漆材质 != 0)
			渲染器.material_unload(资源.清漆材质);
		if (资源.镂空材质 != 0)
			渲染器.material_unload(资源.镂空材质);
		if (资源.扩展材质 != 0)
			渲染器.material_unload(资源.扩展材质);
		if (资源.地面材质 != 0)
			渲染器.material_unload(资源.地面材质);
		//逐物体卡通与分段材质（第四百一十一轮）
		if (资源.卡通材质甲 != 0)
			渲染器.material_unload(资源.卡通材质甲);
		if (资源.卡通材质乙 != 0)
			渲染器.material_unload(资源.卡通材质乙);
		if (资源.卡通材质丙 != 0)
			渲染器.material_unload(资源.卡通材质丙);
		if (资源.分段材质甲 != 0)
			渲染器.material_unload(资源.分段材质甲);
		if (资源.分段材质乙 != 0)
			渲染器.material_unload(资源.分段材质乙);
		if (资源.双面材质 != 0)
			渲染器.material_unload(资源.双面材质);
		资源.贴图材质 = 0;
		资源.光晕材质 = 0;
		资源.清漆材质 = 0;
		资源.镂空材质 = 0;
		资源.扩展材质 = 0;
		资源.地面材质 = 0;
		资源.卡通材质甲 = 0;
		资源.卡通材质乙 = 0;
		资源.卡通材质丙 = 0;
		资源.分段材质甲 = 0;
		资源.分段材质乙 = 0;
		资源.双面材质 = 0;

		//字体与纹理
		if (资源.字体 != 0)
			渲染器.font_unload(资源.字体);
		const uint64_t 纹理[] = { 资源.棋盘, 资源.地板, 资源.法线贴图, 资源.粗糙贴图, 资源.天空,
			资源.动态贴图, 资源.自发光贴图, 资源.遮蔽贴图, 资源.金属贴图, 资源.查找表 };
		for (uint64_t 句柄 : 纹理)
		{
		 if (句柄 != 0)
			渲染器.texture_unload(句柄);
		}
		资源.字体 = 0;
		资源.棋盘 = 0;
		资源.地板 = 0;
		资源.法线贴图 = 0;
		资源.粗糙贴图 = 0;
		资源.天空 = 0;
		资源.动态贴图 = 0;
		资源.自发光贴图 = 0;
		资源.遮蔽贴图 = 0;
		资源.金属贴图 = 0;
		资源.查找表 = 0;

		//网格
		const uint64_t 网格[] = { 资源.球, 资源.圆柱, 资源.圆锥, 资源.胶囊, 资源.圆环, 资源.平面,
			资源.地面, 资源.地面反向, 资源.圆盘, 资源.立方体, 资源.四棱锥, 资源.卡通球, 资源.地形, 资源.三角,
			资源.分段平面 };
		for (uint64_t 句柄 : 网格)
		{
		 if (句柄 != 0)
			渲染器.mesh_unload(句柄);
		}
		资源.球 = 0;
		资源.圆柱 = 0;
		资源.圆锥 = 0;
		资源.胶囊 = 0;
		资源.圆环 = 0;
		资源.平面 = 0;
		资源.地面 = 0;
		资源.地面反向 = 0;
		资源.圆盘 = 0;
		资源.立方体 = 0;
		资源.四棱锥 = 0;
		资源.卡通球 = 0;
		资源.地形 = 0;
		资源.三角 = 0;
		资源.分段平面 = 0;
	}
}
