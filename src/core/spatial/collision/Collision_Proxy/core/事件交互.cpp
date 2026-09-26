#include "../局部命名空间使用.h"
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
			event("", "", "Collision", "ColliderSet"),
			event("", "", "Collision", "ColliderDisplacement"),
			event("", "", "Collision", "ColliderCollisionResponse")
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

		//位移向量事件
		if (evt->tag == "ColliderDisplacement")
		{
			//保存位移事件（供碰撞空间更新位置时重读）
			displacement_register(evt->config);
			return;
		}

		//碰撞响应事件（回复本地回复信箱，供检测流程回查）
		if (evt->tag == "ColliderCollisionResponse")
		{
			//登记碰撞响应
			collision_response_register(evt->config);
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
}
