//关卡装配桥实现：把关卡数据落到渲染层（应用层职责）
#include "src/主调/能力展示/关卡装配桥.h"

//日志系统
#include "Engine/EngineCore/src/tools/Logging/日志系统运行包.h"

//浮动与整数
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace engine
{
	namespace
	{
		//材质库项（材质名与句柄按下标对齐，供实例按模板名解析）
		struct 材质库项
		{
			//材质库 ID
			std::string id;
			//材质名
			std::vector<std::string> names;
			//句柄组
			std::vector<Material_Handle> handles;
		};

		//回退本次已建的网格与材质
		void 资产回退(Renderer& renderer, const std::vector<std::pair<std::string, Mesh_Handle>>& 网格表,
			const std::vector<材质库项>& 材质表, Asset_Scene& scene_out)
		{
			//回退材质
			for (const 材质库项& 库 : 材质表)
				for (Material_Handle 句柄 : 库.handles)
					renderer.material_unload(句柄);
			//回退网格
			for (const std::pair<std::string, Mesh_Handle>& item : 网格表)
				renderer.mesh_unload(item.second);
			scene_out.meshes.clear();
			scene_out.materials.clear();
			scene_out.objects.clear();
		}
	}

	//关卡相机组 → 渲染层相机组
	void level_camera_map(const Level_Camera& source, Camera_Data& out)
	{
		for (std::size_t index = 0; index < 3; ++index)
		{
			out.position[index] = source.position[index];
			out.target[index] = source.target[index];
			out.up[index] = source.up[index];
		}
		for (std::size_t index = 0; index < 4; ++index)
			out.clear_color[index] = source.clear_color[index];
		out.fov = source.fov;
		out.near_plane = source.near_plane;
		out.far_plane = source.far_plane;
		out.ortho = source.ortho;
		out.ortho_height = source.ortho_height;
		out.aspect = source.aspect;
	}

	//关卡光照组 → 渲染层光照组
	void level_light_map(const Level_Light& source, Light_Data& out)
	{
		for (std::size_t index = 0; index < 3; ++index)
		{
			out.direction[index] = source.direction[index];
			out.color[index] = source.color[index];
			out.ambient_color[index] = source.ambient_color[index];
			out.ambient_ground[index] = source.ambient_ground[index];
			out.fill_direction[index] = source.fill_direction[index];
			out.fill_color[index] = source.fill_color[index];
			out.toon_shadow_tint[index] = source.toon_shadow_tint[index];
			out.outline_color[index] = source.outline_color[index];
		}
		out.intensity = source.intensity;
		out.ambient = source.ambient;
		out.specular_intensity = source.specular_intensity;
		out.specular_shininess = source.specular_shininess;
		out.attenuation_mode = source.attenuation_mode;
		out.ambient_hemisphere = source.ambient_hemisphere;
		out.fill_intensity = source.fill_intensity;
		out.directional_specular = source.directional_specular;
		out.fill_specular = source.fill_specular;
		out.shadow_enabled = source.shadow_enabled;
		out.shadow_bias = source.shadow_bias;
		out.shadow_softness = source.shadow_softness;
		out.shadow_strength = source.shadow_strength;
		out.shadow_resolution = source.shadow_resolution;
		out.spot_shadow_enabled = source.spot_shadow_enabled;
		out.spot_shadow_bias = source.spot_shadow_bias;
		out.spot_shadow_softness = source.spot_shadow_softness;
		out.spot_shadow_strength = source.spot_shadow_strength;
		out.spot_shadow_index = source.spot_shadow_index;
		out.spot_shadow_all = source.spot_shadow_all;
		out.spot_shadow_bias_auto = source.spot_shadow_bias_auto;
		out.pbr_enabled = source.pbr_enabled;
		out.pbr_roughness = source.pbr_roughness;
		out.pbr_metallic = source.pbr_metallic;
		out.toon_steps = source.toon_steps;
		out.toon_rim = source.toon_rim;
		out.toon_specular = source.toon_specular;
		out.outline_width = source.outline_width;
		//点光源槽位
		out.point_count = source.point_count;
		for (std::size_t index = 0; index < 8; ++index)
		{
			for (std::size_t axis = 0; axis < 3; ++axis)
			{
				out.point_position[index][axis] = source.point_position[index][axis];
				out.point_color[index][axis] = source.point_color[index][axis];
				out.spot_position[index][axis] = source.spot_position[index][axis];
				out.spot_direction[index][axis] = source.spot_direction[index][axis];
				out.spot_color[index][axis] = source.spot_color[index][axis];
			}
			out.point_radius[index] = source.point_radius[index];
			out.point_intensity[index] = source.point_intensity[index];
			out.point_attenuation[index] = source.point_attenuation[index];
			out.point_specular[index] = source.point_specular[index];
			out.point_shininess[index] = source.point_shininess[index];
			out.spot_radius[index] = source.spot_radius[index];
			out.spot_intensity[index] = source.spot_intensity[index];
			out.spot_inner_deg[index] = source.spot_inner_deg[index];
			out.spot_outer_deg[index] = source.spot_outer_deg[index];
			out.spot_attenuation[index] = source.spot_attenuation[index];
			out.spot_specular[index] = source.spot_specular[index];
			out.spot_shininess[index] = source.spot_shininess[index];
		}
		out.spot_count = source.spot_count;
	}

	//关卡雾组 → 渲染层雾组
	void level_fog_map(const Level_Fog& source, Fog_Data& out)
	{
		for (std::size_t index = 0; index < 3; ++index)
			out.color[index] = source.color[index];
		out.density = source.density;
		out.start = source.start;
		out.end = source.end;
		out.mode = source.mode;
		out.height_falloff = source.height_falloff;
	}

	namespace
	{
		/*
		按清单装配资产（与旧 `asset_scene_build` 逐条对应）
		调用方已完成清单解析与目录计算；本函数负责资源创建、对象装配与失败整体回退。
		同一网格 / 材质库只加载一次，place 按顺序复用句柄。
		*/
		bool 资产装配(Renderer& renderer, const Scene_Manifest& 清单, const std::string& 目录,
			const std::vector<Level_Material>& 实例声明, Asset_Scene& scene_out)
		{
			scene_out.objects.clear();
			scene_out.meshes.clear();
			scene_out.materials.clear();

			//网格（ID → 句柄）
			std::vector<std::pair<std::string, Mesh_Handle>> 网格表;
			网格表.reserve(清单.meshes.size());
			for (const Scene_Mesh_Ref& ref : 清单.meshes)
			{
				Mesh_Handle 句柄 = 0;
				if (!renderer.mesh_load_obj(句柄, 目录 + ref.path))
				{
					logger.warn("关卡装配:: 清单网格加载失败（{}），整体回退", ref.path);
					//此时尚未建任何材质库
					const std::vector<材质库项> 空材质表;
					资产回退(renderer, 网格表, 空材质表, scene_out);
					return false;
				}
				网格表.emplace_back(ref.id, 句柄);
				scene_out.meshes.push_back(句柄);
			}

			//材质库（ID → 名称 ＋ 句柄组）
			std::vector<材质库项> 材质表;
			材质表.reserve(清单.materials.size());
			for (const Scene_Mtl_Ref& ref : 清单.materials)
			{
				材质库项 项;
				项.id = ref.id;
				if (!renderer.material_load_mtl(项.handles, 目录 + ref.path, &项.names))
				{
					logger.warn("关卡装配:: 清单材质库加载失败（{}），整体回退", ref.path);
					资产回退(renderer, 网格表, 材质表, scene_out);
					return false;
				}
				for (Material_Handle 句柄 : 项.handles)
					scene_out.materials.push_back(句柄);
				材质表.push_back(std::move(项));
			}

			//材质实例表（实例名 → 覆盖数据）
			std::vector<std::pair<std::string, Material_Instance_Data>> 实例表;
			实例表.reserve(实例声明.size());
			for (const Level_Material& 声明 : 实例声明)
			{
				//按模板材质名在全库中找句柄
				Material_Handle 模板 = 0;
				for (const 材质库项& 库 : 材质表)
				{
					for (std::size_t index = 0; index < 库.names.size() && index < 库.handles.size(); ++index)
					{
						if (库.names[index] == 声明.模板)
						{
							模板 = 库.handles[index];
							break;
						}
					}
					if (模板 != 0)
						break;
				}
				//模板未命中
				if (模板 == 0)
				{
					logger.warn("关卡装配:: 材质实例 {} 的模板 {} 未命中，整体回退", 声明.实例名, 声明.模板);
					资产回退(renderer, 网格表, 材质表, scene_out);
					return false;
				}
				Material_Instance_Data 数据;
				数据.模板 = 模板;
				for (std::size_t index = 0; index < 4; ++index)
					数据.tint[index] = 声明.tint[index];
				数据.use_tint = 声明.use_tint;
				数据.emissive = 声明.emissive;
				数据.use_emissive = 声明.use_emissive;
				数据.use_toon = 声明.use_toon;
				数据.toon_steps = 声明.toon_steps;
				数据.toon_rim = 声明.toon_rim;
				数据.toon_specular = 声明.toon_specular;
				数据.use_pbr = 声明.use_pbr;
				数据.pbr_roughness = 声明.pbr_roughness;
				数据.pbr_metallic = 声明.pbr_metallic;
				数据.use_clearcoat = 声明.use_clearcoat;
				数据.clear_coat = 声明.clear_coat;
				数据.clear_coat_roughness = 声明.clear_coat_roughness;
				//实例数据校验（模板已登记 ＋ 覆盖值合法）
				if (!renderer.material_instance_validate(数据))
				{
					logger.warn("关卡装配:: 材质实例 {} 数据非法，整体回退", 声明.实例名);
					资产回退(renderer, 网格表, 材质表, scene_out);
					return false;
				}
				实例表.emplace_back(声明.实例名, 数据);
			}

			//按 place 顺序装配对象
			for (const Scene_Place& place : 清单.places)
			{
				Asset_Object object;
				//网格句柄（解析阶段已保证 ID 存在）
				for (const std::pair<std::string, Mesh_Handle>& item : 网格表)
				{
					if (item.first == place.mesh_id)
					{
						object.mesh = item.second;
						break;
					}
				}
				//材质列（可选）：先查实例表，未命中再查材质库
				if (!place.material_id.empty())
				{
					for (const std::pair<std::string, Material_Instance_Data>& item : 实例表)
					{
						if (item.first == place.material_id)
						{
							object.materials.push_back(item.second.模板);
							object.material_instances.push_back(item.second);
							break;
						}
					}
					//实例未命中：按材质库 ID 取整组句柄
					if (object.materials.empty())
						for (const 材质库项& 库 : 材质表)
						{
							if (库.id == place.material_id)
							{
								object.materials = 库.handles;
								break;
							}
						}
				}
				//位置 / 缩放 / 旋转
				for (std::size_t index = 0; index < 3; ++index)
				{
					object.position[index] = place.position[index];
					object.scale[index] = place.scale[index];
					object.rotation_deg[index] = place.rotation_deg[index];
				}
				scene_out.objects.push_back(object);
			}
			return true;
		}
	}

	//装载关卡
	bool level_bridge_load(Renderer& renderer, Level_Data& level_out, Asset_Scene& scene_out,
		const std::string& path, bool apply_scene)
	{
		//默认无结果
		level_out = Level_Data();
		scene_out = Asset_Scene();
		//读取关卡文本
		if (!level_text_read(path, level_out))
			return false;

		/*
		相机参数校验（与 `scene_apply` 三入参版同口径）
		`scene_apply` 为 void，非法时静默拒绝；关卡装载要求显式失败，故在此先判一次。
		*/
		const Level_Camera& camera = level_out.camera;
		const bool camera_valid = (camera.fov > 0.0f) && (camera.fov < 3.14159265f)
			&& (camera.near_plane > 0.0f) && (camera.far_plane > camera.near_plane)
			&& (!camera.ortho || camera.ortho_height > 0.0f);
		//若相机参数非法
		if (!camera_valid)
		{
			logger.warn("关卡装配:: 相机非法（视场角 {}、近平面 {}、远平面 {}），装载拒绝",
				camera.fov, camera.near_plane, camera.far_plane);
			level_out = Level_Data();
			return false;
		}

		//关卡所在目录（资产路径相对它解析）
		const std::size_t cut = path.find_last_of("/\\");
		const std::string 目录 = (cut == std::string::npos) ? std::string() : path.substr(0, cut + 1);

		//资产段落（为空表示空场景）
		if (!level_out.asset_lines.empty())
		{
			Scene_Manifest 清单;
			//材质实例名表（place 材质列可引用实例）
			std::vector<std::string> 实例名表;
			实例名表.reserve(level_out.materials.size());
			for (const Level_Material& material : level_out.materials)
				实例名表.push_back(material.实例名);
			if (!scene_manifest_parse_lines(level_out.asset_lines, 清单, 实例名表))
			{
				level_out = Level_Data();
				return false;
			}
			//装配资产（失败整体回退）
			if (!资产装配(renderer, 清单, 目录, level_out.materials, scene_out))
			{
				level_out = Level_Data();
				return false;
			}
		}

		//应用相机 / 光照 / 雾（相机已校验，三组必然生效；仅当调用方要求时）
		if (apply_scene)
		{
			Camera_Data 相机;
			Light_Data 光照;
			Fog_Data 雾;
			level_camera_map(level_out.camera, 相机);
			level_light_map(level_out.light, 光照);
			level_fog_map(level_out.fog, 雾);
			renderer.scene_apply(相机, 光照, 雾);
		}
		return true;
	}

	//释放装配创建的全部资源（幂等）
	void level_bridge_unload(Renderer& renderer, Asset_Scene& scene)
	{
		//网格
		for (Mesh_Handle 句柄 : scene.meshes)
			renderer.mesh_unload(句柄);
		//材质
		for (Material_Handle 句柄 : scene.materials)
			renderer.material_unload(句柄);
		scene.meshes.clear();
		scene.materials.clear();
		scene.objects.clear();
	}

	//装载场景清单文件
	bool scene_bundle_load(Renderer& renderer, Asset_Scene& scene_out, const std::string& path)
	{
		//默认无结果
		scene_out = Asset_Scene();
		//解析清单（清单文件没有材质实例声明）
		Scene_Manifest 清单;
		if (!scene_manifest_parse(path, 清单))
			return false;
		//清单所在目录（保留 UTF-8 编码；相对路径直接前缀拼接）
		const std::size_t cut = path.find_last_of("/\\");
		const std::string 目录 = (cut == std::string::npos) ? std::string() : path.substr(0, cut + 1);
		return 资产装配(renderer, 清单, 目录, {}, scene_out);
	}

	//逐对象构造矩阵并逐段绑材质（返回成功登记的对象数）
	uint32_t level_bridge_draw(Renderer& renderer, const Asset_Scene& scene)
	{
		uint32_t 登记数 = 0;
		for (const Asset_Object& object : scene.objects)
		{
			//对象未持有网格
			if (object.mesh == 0)
				continue;
			//按位置 / 旋转（度）/ 缩放构造变换
			float transform[16] = {};
			level_transform_build(object.position, object.rotation_deg, object.scale, transform);
			/*
			材质实例覆盖
			实例数组与材质数组按段对齐时才套用；否则按旧路径绘制（逐字节不变）。
			*/
			const Material_Instance_Data* 实例指针 = nullptr;
			if (!object.material_instances.empty() && object.material_instances.size() == object.materials.size())
				实例指针 = object.material_instances.data();
			if (renderer.mesh_draw_sections(object.mesh, transform, object.materials.data(),
				static_cast<uint32_t>(object.materials.size()), 实例指针))
				登记数 += 1;
		}
		return 登记数;
	}
}
