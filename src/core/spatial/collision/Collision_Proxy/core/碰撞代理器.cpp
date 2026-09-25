#include "../局部命名空间使用.h"
#include "src/tools/Data_Validator/数据校验器.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
		//字符串字段读取
		bool text_read(const json& config, const string& field, string& receiver)
		{
			//字段存在性与类型检查
			if (!config.is_object() || !config.contains(field) || !config[field].is_string())
				return false;
			//读取字段内容
			receiver = config[field].get<string>();
			return !receiver.empty();
		}

		//无符号整数字段读取
		bool number_read(const json& config, const string& field, uint64_t& receiver)
		{
			//字段存在性与类型检查
			if (!config.is_object() || !config.contains(field) ||
				!config[field].is_number_integer())
				return false;
			//读取字段内容
			receiver = config[field].get<uint64_t>();
			return true;
		}

		//三元数组字段读取
		bool vector_read(const json& config, const string& field, Vector3& receiver)
		{
			//字段存在性检查
			if (!config.is_object() || !config.contains(field))
				return false;

			try
			{
				//读取数组内容
				vector<double> values = config[field].get<vector<double>>();
				//数组长度检查
				if (values.size() != 3)
					return false;
				//写入矢量
				receiver.setValue(values[0], values[1], values[2]);
			}
			catch (const std::exception&)
			{
				//数组内容非法
				return false;
			}

			return true;
		}
	}

	//构造函数
	Collision_Proxy::Collision_Proxy()
	{
		//生成事件发送权限密钥
		acl_key = event_terminal.acl_key_gen();
	}

	//碰撞空间构建
	bool Collision_Proxy::region_build(const string& region)
	{
		//若目标碰撞空间已存在
		if(regions.count(region))
		{
			Log::warn("Collision_Proxy::待构建碰撞空间({})已存在",region);
			return false;
		}
		else
		{
			//分配碰撞空间内存
			regions[region].reset(new(nothrow)Collision_Region(region));
			//若内存分配失败
			if (!regions[region])
			{
				Log::error("Collision_Proxy::内存分配失败\n目标碰撞空间({})无法创建", region);
				return false;
			}
			else
				return true;
		}
	}

	//碰撞空间卸载
	bool Collision_Proxy::region_unload(const string& region)
	{
		//获取目标碰撞空间迭代器
		auto it = regions.find(region);
		//若目标碰撞空间不存在
		if (it == regions.end())
		{
			Log::warn("Collision_Proxy::待卸载碰撞空间({})不存在", region);
			return false;
		}
		else
		{
			//注销该空间持有的全部碰撞体编号
			for (uint64_t collider_ID : it->second->colliders())
				collider_mapping_unlink(collider_ID, region);
			//卸载目标碰撞空间
			regions.erase(region);
			return true;
		}
	}

	//碰撞空间检测执行
	bool Collision_Proxy::region_detect(const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待检测碰撞空间({})不存在", region);
			return false;
		}

		//执行碰撞检测
		optional<vector<Collision_Result>> detected = target->detect();
		//若空间未激活或后端不可用
		if (!detected)
		{
			Log::warn("Collision_Proxy::碰撞空间({})未激活，检测未执行", region);
			return false;
		}

		//检测结果载荷
		json payload;
		//写入空间名称
		payload["region"] = region;
		//写入碰撞对集合
		payload["results"] = json::array();
		//逐个写入碰撞对
		for (const Collision_Result& result : *detected)
		{
			//单个碰撞对
			json item;
			//写入碰撞体编号
			item["collider_A"] = result.collider_A;
			item["collider_B"] = result.collider_B;
			//追加碰撞对
			payload["results"].push_back(item);
		}

		//发布检测结果事件
		return event_publish("DetectResult", payload);
	}

	//碰撞空间边界设置
	bool Collision_Proxy::region_boundary_set(const string& path)
	{
		//空间边界配置内容
		json boundary_config;
		//读取空间边界配置
		if (!boundary_config_read(path, boundary_config))
			return false;

		//是否全部空间设置成功
		bool all_set = true;
		//逐个空间设置空间边界
		for (const auto& boundary_data : boundary_config)
		{
			//空间名称
			string region;
			//空间边界网格路径
			string mesh_path;
			//读取空间名称
			if (!text_read(boundary_data, "region", region))
			{
				Log::warn("Collision_Proxy::边界配置缺少有效字段(region)");
				all_set = false;
				continue;
			}
			//读取空间边界网格路径
			if (!text_read(boundary_data, "mesh_path", mesh_path))
			{
				Log::warn("Collision_Proxy::边界配置缺少有效字段(mesh_path)");
				all_set = false;
				continue;
			}

			//查找目标碰撞空间
			Collision_Region* target = region_seek(region);
			//若目标碰撞空间不存在
			if (!target)
			{
				Log::warn("Collision_Proxy::待设置边界的碰撞空间({})不存在", region);
				all_set = false;
				continue;
			}

			//构建空间边界
			if (!target->boundary_build(mesh_path))
			{
				Log::warn("Collision_Proxy::碰撞空间({})边界构建失败({})", region, mesh_path);
				all_set = false;
			}
		}

		return all_set;
	}

	//碰撞空间活跃性设置
	bool Collision_Proxy::region_state_set(const string& region, bool active)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待设置状态的碰撞空间({})不存在", region);
			return false;
		}

		//设置空间活跃性
		target->state_set(active);
		return true;
	}

	//碰撞体构建
	optional<uint64_t> Collision_Proxy::collider_build(const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待构建碰撞体的碰撞空间({})不存在", region);
			return nullopt;
		}

		//在目标空间内构建碰撞体
		uint64_t collider_ID = target->collider_build();
		//登记编号归属
		collider_mapping_link(collider_ID, region);
		return collider_ID;
	}

	//碰撞体卸载
	bool Collision_Proxy::collider_unload(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待卸载碰撞体的碰撞空间({})不存在", region);
			return false;
		}

		//在目标空间内卸载碰撞体
		if (!target->collider_unload(collider_ID))
			return false;
		//注销编号归属
		collider_mapping_unlink(collider_ID, region);
		return true;
	}

	//碰撞体转移
	bool Collision_Proxy::collider_transfer(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待转移至的碰撞空间({})不存在", region);
			return false;
		}

		//查找编号归属空间
		string owner;
		//若该编号未被任何空间持有
		if (!collider_owner_seek(collider_ID, owner))
		{
			Log::warn("Collision_Proxy::待转移碰撞体({})不存在", collider_ID);
			return false;
		}
		//归属空间即目标空间时无需转移
		if (owner == region)
		{
			Log::warn("Collision_Proxy::待转移碰撞体({})已在目标空间({})内", collider_ID, region);
			return false;
		}

		//查找源碰撞空间
		Collision_Region* source = region_seek(owner);
		//若源碰撞空间不存在
		if (!source)
		{
			Log::warn("Collision_Proxy::碰撞体({})的归属空间({})不存在", collider_ID, owner);
			return false;
		}

		//取源碰撞体的几何配置
		json geometry = source->collider_geometry(collider_ID);

		//在目标空间内按原编号接管（接管失败时源空间尚未改动）
		if (!target->collider_adopt(collider_ID))
		{
			Log::warn("Collision_Proxy::碰撞体({})编号在目标空间({})内冲突，转移中止", collider_ID, region);
			return false;
		}
		//卸载源碰撞体
		if (!source->collider_unload(collider_ID))
		{
			//回滚目标空间内的接管
			target->collider_unload(collider_ID);
			return false;
		}

		//更新编号归属
		collider_mapping_unlink(collider_ID, owner);
		collider_mapping_link(collider_ID, region);

		//无几何配置时仅转移编号登记（尚未设置几何体的占位碰撞体）
		if (geometry.is_null() || geometry.empty())
			return true;

		//在目标空间内按原编号重建几何体
		geometry["collider_ID"] = collider_ID;
		if (!target->collider_set(geometry))
		{
			Log::warn("Collision_Proxy::碰撞体({})几何体在目标空间({})重建失败", collider_ID, region);
			return false;
		}

		return true;
	}

	//碰撞体镜像
	bool Collision_Proxy::collider_mirror(const uint64_t collider_ID, const string& region)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待镜像至的碰撞空间({})不存在", region);
			return false;
		}

		//查找编号归属空间
		string owner;
		//若该编号未被任何空间持有
		if (!collider_owner_seek(collider_ID, owner))
		{
			Log::warn("Collision_Proxy::待镜像碰撞体({})不存在", collider_ID);
			return false;
		}

		//查找源碰撞空间
		Collision_Region* source = region_seek(owner);
		//若源碰撞空间不存在
		if (!source)
		{
			Log::warn("Collision_Proxy::碰撞体({})的归属空间({})不存在", collider_ID, owner);
			return false;
		}

		//取源碰撞体的几何配置
		json geometry = source->collider_geometry(collider_ID);

		//在目标空间内构建全新编号的碰撞体
		optional<uint64_t> mirror_ID = collider_build(region);
		//若碰撞体构建失败
		if (!mirror_ID)
			return false;

		//无几何配置时镜像得到的是占位碰撞体
		if (geometry.is_null() || geometry.empty())
			return true;

		//按新编号写入几何配置
		geometry["collider_ID"] = *mirror_ID;
		if (!target->collider_set(geometry))
		{
			Log::warn("Collision_Proxy::镜像碰撞体({})几何体构建失败", *mirror_ID);
			return false;
		}

		return true;
	}

	//碰撞体设置 —— 位移向量重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const Vector3& vector)
	{
		//对持有该编号的空间施加位移向量设置
		return collider_set_dispatch(collider_ID, [collider_ID, &vector](Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}

	//碰撞体设置 —— 检测方式重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const Detection_Mode& vector)
	{
		//对持有该编号的空间施加检测方式设置
		return collider_set_dispatch(collider_ID, [collider_ID, &vector](Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}

	//碰撞体设置 —— 豁免标记重载
	bool Collision_Proxy::collider_set(const uint64_t collider_ID, const uint64_t& vector)
	{
		//对持有该编号的空间施加豁免标记设置
		return collider_set_dispatch(collider_ID, [collider_ID, &vector](Collision_Region& region)
			{
				return region.collider_set(collider_ID, vector);
			});
	}

	//事件中转站接入
	void Collision_Proxy::attach(void)
	{
		//若事件中转站接入入口尚未注册（接入入口由事件中转站的持有者注册）
		if (!event_terminal->interface_check(interface_ID::ATTACH_HANDLER))
		{
			Log::error("Collision_Proxy::事件中转站接入入口未注册，接入中止");
			return;
		}

		//注册本模块事件接收入口（事件送达后交由事件处理分派）
		if (!event_terminal->event_receiver_register(
			[this](shared_ptr<event> evt) { this->event_process(evt); }))
		{
			Log::error("Collision_Proxy::事件接收入口注册失败，接入中止");
			return;
		}

		//待订阅事件清单
		vector<event> needed_events{
			event("", "", "Config", "Load"),
			event("", "", "Collision", "RegionBuild"),
			event("", "", "Collision", "RegionUnload"),
			event("", "", "Collision", "RegionState"),
			event("", "", "Collision", "RegionBoundary"),
			event("", "", "Collision", "RegionDetect"),
			event("", "", "Collision", "ColliderBuild"),
			event("", "", "Collision", "ColliderUnload"),
			event("", "", "Collision", "ColliderTransfer"),
			event("", "", "Collision", "ColliderMirror"),
			event("", "", "Collision", "ColliderSet")
		};

		//接入事件中转站
		if (!event_terminal.attach(module_name, needed_events, acl_key))
			Log::error("Collision_Proxy::事件中转站接入失败");
	}

	//事件处理
	void Collision_Proxy::event_process(std::shared_ptr<event> evt)
	{
		//空事件检查
		if (!evt)
			return;

		//---------- 配置路由指令 ----------
		if (evt->category == "Config" && evt->tag == "Load")
		{
			//非本模块的配置指令一律忽略
			if (evt->target_object != module_name)
				return;
			//按配置内容构建碰撞空间与碰撞体
			config_apply(evt->config);
			return;
		}

		//---------- 碰撞模块指令 ----------
		if (evt->category != "Collision")
		{
			Log::debug("Collision_Proxy::未识别事件({}/{})", evt->category, evt->tag);
			return;
		}

		//碰撞空间构建
		if (evt->tag == "RegionBuild")
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, "region", region))
			{
				Log::warn("Collision_Proxy::空间构建事件缺少有效字段(region)");
				return;
			}
			//构建碰撞空间
			region_build(region);
			return;
		}

		//碰撞空间卸载
		if (evt->tag == "RegionUnload")
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, "region", region))
			{
				Log::warn("Collision_Proxy::空间卸载事件缺少有效字段(region)");
				return;
			}
			//卸载碰撞空间
			region_unload(region);
			return;
		}

		//碰撞空间活跃性设置
		if (evt->tag == "RegionState")
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, "region", region))
			{
				Log::warn("Collision_Proxy::空间状态事件缺少有效字段(region)");
				return;
			}
			//若缺少活跃性字段
			if (!evt->config.contains("active") || !evt->config["active"].is_boolean())
			{
				Log::warn("Collision_Proxy::空间状态事件缺少有效字段(active)");
				return;
			}
			//设置空间活跃性
			region_state_set(region, evt->config["active"].get<bool>());
			return;
		}

		//碰撞空间边界设置
		if (evt->tag == "RegionBoundary")
		{
			//空间边界配置路径
			string path;
			//读取空间边界配置路径
			if (!text_read(evt->config, "path", path))
			{
				Log::warn("Collision_Proxy::空间边界事件缺少有效字段(path)");
				return;
			}
			//设置空间边界
			region_boundary_set(path);
			return;
		}

		//碰撞空间检测执行
		if (evt->tag == "RegionDetect")
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, "region", region))
			{
				Log::warn("Collision_Proxy::空间检测事件缺少有效字段(region)");
				return;
			}
			//执行碰撞检测
			region_detect(region);
			return;
		}

		//碰撞体构建
		if (evt->tag == "ColliderBuild")
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, "region", region))
			{
				Log::warn("Collision_Proxy::碰撞体构建事件缺少有效字段(region)");
				return;
			}
			//构建碰撞体
			optional<uint64_t> collider_ID = collider_build(region);
			//若碰撞体构建失败
			if (!collider_ID)
				return;

			//碰撞体构建结果载荷（事件调用无法取得返回值，故以结果事件回告编号）
			json payload;
			//写入空间名称与碰撞体编号
			payload["region"] = region;
			payload["collider_ID"] = *collider_ID;
			//发布碰撞体构建结果事件
			event_publish("ColliderBuildResult", payload);
			return;
		}

		//碰撞体卸载、转移、镜像与设置
		if (evt->tag == "ColliderUnload" || evt->tag == "ColliderTransfer" ||
			evt->tag == "ColliderMirror" || evt->tag == "ColliderSet")
		{
			//目标空间名称
			string region;
			//目标碰撞体编号
			uint64_t collider_ID = 0;
			//读取目标空间名称
			if (!text_read(evt->config, "region", region))
			{
				Log::warn("Collision_Proxy::碰撞体事件缺少有效字段(region)");
				return;
			}
			//读取目标碰撞体编号
			if (!number_read(evt->config, "collider_ID", collider_ID))
			{
				Log::warn("Collision_Proxy::碰撞体事件缺少有效字段(collider_ID)");
				return;
			}

			//碰撞体卸载
			if (evt->tag == "ColliderUnload")
				collider_unload(collider_ID, region);
			//碰撞体转移
			else if (evt->tag == "ColliderTransfer")
				collider_transfer(collider_ID, region);
			//碰撞体镜像
			else if (evt->tag == "ColliderMirror")
				collider_mirror(collider_ID, region);
			//碰撞体设置
			else
				collider_config_apply(region, collider_ID, evt->config);
			return;
		}

		//未识别的碰撞模块指令
		Log::debug("Collision_Proxy::未识别事件({}/{})", evt->category, evt->tag);
	}

	//碰撞空间查找
	Collision_Region* Collision_Proxy::region_seek(const string& region)
	{
		//查找目标碰撞空间
		auto it = regions.find(region);
		//若目标碰撞空间不存在
		if (it == regions.end())
			return nullptr;

		return it->second.get();
	}

	//碰撞体归属查找
	bool Collision_Proxy::collider_owner_seek(const uint64_t collider_ID, string& receiver) const
	{
		//查找该编号的全部登记项
		auto it = collider_mapping.find(collider_ID);
		//若该编号未被任何空间持有
		if (it == collider_mapping.end())
			return false;

		//写入该编号的持有空间名称
		receiver = it->second;
		return true;
	}

	//碰撞体编号登记
	void Collision_Proxy::collider_mapping_link(const uint64_t collider_ID, const string& region)
	{
		collider_mapping.emplace(collider_ID, region);
	}

	//碰撞体编号注销
	void Collision_Proxy::collider_mapping_unlink(const uint64_t collider_ID, const string& region)
	{
		//取该编号的全部登记项
		auto range = collider_mapping.equal_range(collider_ID);
		//逐个注销属于目标空间的登记项
		for (auto it = range.first; it != range.second;)
		{
			//若登记项属于目标空间则删除
			if (it->second == region)
				it = collider_mapping.erase(it);
			else
				++it;
		}
	}

	//碰撞体设置分发
	bool Collision_Proxy::collider_set_dispatch(const uint64_t collider_ID,
		const std::function<bool(Collision_Region&)>& setter)
	{
		//取该编号的全部登记项
		auto range = collider_mapping.equal_range(collider_ID);
		//若该编号未被任何空间持有
		if (range.first == range.second)
		{
			Log::warn("Collision_Proxy::待设置碰撞体({})不存在", collider_ID);
			return false;
		}

		//是否存在设置成功的空间
		bool any_set = false;
		//逐个对持有该编号的空间施加设置
		for (auto it = range.first; it != range.second; ++it)
		{
			//查找目标碰撞空间
			Collision_Region* target = region_seek(it->second);
			//若目标碰撞空间不存在则跳过
			if (!target)
				continue;
			//施加设置
			if (setter(*target))
				any_set = true;
		}

		return any_set;
	}

	//碰撞体配置应用
	void Collision_Proxy::collider_config_apply(const string& region, uint64_t collider_ID,
		const json& config)
	{
		//查找目标碰撞空间
		Collision_Region* target = region_seek(region);
		//若目标碰撞空间不存在
		if (!target)
		{
			Log::warn("Collision_Proxy::待设置碰撞体的碰撞空间({})不存在", region);
			return;
		}

		//若配置中带有几何体
		if (config.contains("geometry") && config["geometry"].is_object())
		{
			//几何配置（补齐碰撞体编号）
			json geometry = config["geometry"];
			//写入目标碰撞体编号
			geometry["collider_ID"] = collider_ID;
			//设置几何体
			if (!target->collider_set(geometry))
				Log::warn("Collision_Proxy::碰撞体({})几何体设置失败", collider_ID);
		}

		//若配置中带有位移向量
		if (config.contains("displacement"))
		{
			//位移向量
			Vector3 displacement;
			//读取位移向量
			if (vector_read(config, "displacement", displacement))
				collider_set(collider_ID, displacement);
			else
				Log::warn("Collision_Proxy::碰撞体({})字段(displacement)非法", collider_ID);
		}

		//若配置中带有检测方式
		if (config.contains("detection_mode"))
		{
			//检测方式
			Detection_Mode detection_mode;
			//检测方式字段检查
			if (config["detection_mode"].is_object() &&
				config["detection_mode"].contains("is_swept_volume") &&
				config["detection_mode"]["is_swept_volume"].is_boolean())
			{
				//写入扫掠检测标记
				detection_mode.is_swept_volume =
					config["detection_mode"]["is_swept_volume"].get<bool>();
				//写入离散检测步长
				if (config["detection_mode"].contains("step_length") &&
					config["detection_mode"]["step_length"].is_number_integer())
					detection_mode.step_length =
						config["detection_mode"]["step_length"].get<uint32_t>();
				//设置检测方式
				collider_set(collider_ID, detection_mode);
			}
			else
				Log::warn("Collision_Proxy::碰撞体({})字段(detection_mode)非法", collider_ID);
		}

		//若配置中带有豁免标记
		if (config.contains("exemption_flag"))
		{
			//豁免标记
			uint64_t exemption_flag = 0;
			//读取豁免标记
			if (number_read(config, "exemption_flag", exemption_flag))
				collider_set(collider_ID, exemption_flag);
			else
				Log::warn("Collision_Proxy::碰撞体({})字段(exemption_flag)非法", collider_ID);
		}
	}

	//事件发布
	bool Collision_Proxy::event_publish(const string& tag, const json& payload)
	{
		//构造发布事件
		shared_ptr<event> evt = event_terminal.build(module_name, "", "Collision", tag);
		//若事件内存分配失败
		if (!evt)
		{
			Log::error("Collision_Proxy::事件内存分配失败，发布中止");
			return false;
		}
		//写入事件载荷
		evt->config = payload;

		//发送事件（事件终端按单事件通道发送）
		if (!event_terminal.send(evt, acl_key))
		{
			Log::warn("Collision_Proxy::事件({})发送失败，事件发送通道不可用", tag);
			return false;
		}

		return true;
	}

	//配置内容应用
	void Collision_Proxy::config_apply(const json& config)
	{
		//配置内容格式检查
		if (!config.is_object() || !config.contains("regions") || !config["regions"].is_array())
		{
			Log::warn("Collision_Proxy::配置缺少有效字段(regions)");
			return;
		}

		//逐个应用空间配置
		for (const auto& region_config : config["regions"])
		{
			//空间名称
			string region;
			//读取空间名称
			if (!text_read(region_config, "region", region))
			{
				Log::warn("Collision_Proxy::空间配置缺少有效字段(region)");
				continue;
			}

			//构建碰撞空间
			region_build(region);
			//查找目标碰撞空间
			Collision_Region* target = region_seek(region);
			//若目标碰撞空间不存在则跳过
			if (!target)
				continue;

			//设置空间活跃性
			if (region_config.contains("active") && region_config["active"].is_boolean())
				region_state_set(region, region_config["active"].get<bool>());

			//设置空间边界
			if (region_config.contains("boundary") && region_config["boundary"].is_string())
			{
				//空间边界网格路径
				string mesh_path = region_config["boundary"].get<string>();
				//构建空间边界
				if (!target->boundary_build(mesh_path))
					Log::warn("Collision_Proxy::碰撞空间({})边界构建失败({})", region, mesh_path);
			}

			//若空间内无碰撞体清单则结束本空间
			if (!region_config.contains("colliders") || !region_config["colliders"].is_array())
				continue;

			//逐个构建并配置碰撞体
			for (const auto& collider_config : region_config["colliders"])
			{
				//在目标空间内构建碰撞体
				optional<uint64_t> collider_ID = collider_build(region);
				//若碰撞体构建失败则跳过
				if (!collider_ID)
					continue;
				//应用碰撞体配置
				collider_config_apply(region, *collider_ID, collider_config);
			}
		}
	}

	//边界配置文件读取
	bool Collision_Proxy::boundary_config_read(const string& path, json& receiver) const
	{
		//边界配置文件路径检查
		if (!Data_Validator::path_check(path))
		{
			Log::warn("Collision_Proxy::边界配置路径不可读取({})", path);
			return false;
		}

		//打开边界配置文件
		std::ifstream file(string_to_path(path));
		//若文件打开失败
		if (!file.is_open())
		{
			Log::warn("Collision_Proxy::边界配置文件打开失败({})", path);
			return false;
		}

		try
		{
			//解析边界配置内容
			file >> receiver;
		}
		catch (const std::exception&)
		{
			Log::warn("Collision_Proxy::边界配置内容非法({})", path);
			return false;
		}

		//边界配置须为空间数组
		if (!receiver.is_array())
		{
			Log::warn("Collision_Proxy::边界配置须为空间数组({})", path);
			return false;
		}

		return true;
	}

}
