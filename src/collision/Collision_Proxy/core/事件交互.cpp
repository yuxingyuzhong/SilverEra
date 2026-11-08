#include "../局部命名空间使用.h"
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"
//获取碰撞配置字段读取工具
#include "src/collision/Common/字段读取.h"

namespace engine
{
	//引入字段读取工具
	using namespace collision_detail;

	//事件中转站接入
	void Collision_Proxy::attach(void)
	{
		//若事件中转站接入入口尚未注册（接入入口由事件中转站的持有者注册）
		if (!event_terminal->check(Port_ID::Broker_Attacher))
		{
			logger.error("Collision_Proxy::事件中转站接入入口未注册，接入中止");
			return;
		}

		//注册本模块事件接收入口（事件送达后交由事件处理分派）
		event_terminal->bind(Port_ID::Evt_Receiver,
			[this](shared_ptr<Event> evt) { this->event_process(evt); });
		//若事件接收入口注册失败
		if (!event_terminal->check(Port_ID::Evt_Receiver))
		{
			logger.error("Collision_Proxy::事件接收入口注册失败，接入中止");
			return;
		}

		//待订阅事件清单
		vector<Event> needed_events{
			Event("", "", event_protocol::config, event_protocol::load),
			Event("", "", collision_event::category, collision_event::region_build),
			Event("", "", collision_event::category, collision_event::region_unload),
			Event("", "", collision_event::category, collision_event::region_state),
			Event("", "", collision_event::category, collision_event::region_boundary),
			Event("", "", collision_event::category, collision_event::region_event_output),
			Event("", "", collision_event::category, collision_event::collider_build),
			Event("", "", collision_event::category, collision_event::collider_unload),
			Event("", "", collision_event::category, collision_event::collider_transfer),
			Event("", "", collision_event::category, collision_event::collider_mirror),
			Event("", "", collision_event::category, collision_event::collider_set),
			Event("", "", collision_event::category, collision_event::collider_motion),
			Event("", "", collision_event::category, collision_event::region_cross_state_query)
		};

		//接入事件中转站（仅登记订阅者身份与接收入口）
		if (!event_terminal.attach(module_name, acl_key))
		{
			logger.error("Collision_Proxy::事件中转站接入失败");
			return;
		}

		//构造订阅事件清单
		json subscribed_events = json::array();
		//逐条写入订阅事件标识
		for (const auto& needed_event : needed_events)
		{
			//单条订阅事件标识
			json subscribed_event;
			subscribed_event[event_protocol::field_category] = needed_event.category;
			subscribed_event[event_protocol::field_tag] = needed_event.tag;
			subscribed_events.push_back(subscribed_event);
		}

		//构造订阅事件（目标对象名与权限密钥经载荷传递）
		auto attach_evt = event_terminal.build("", "", event_protocol::subscribed_events, event_protocol::add);
		//若事件构造失败
		if (attach_evt == nullptr)
		{
			logger.error("Collision_Proxy::订阅事件构造失败");
			return;
		}
		//写入目标对象名、权限密钥与订阅清单
		attach_evt->config[event_protocol::field_object] = module_name;
		attach_evt->config[event_protocol::field_acl_key] = static_cast<uint64_t>(acl_key);
		attach_evt->config[event_protocol::field_subscribed_events] = subscribed_events;
		//发送订阅事件（登记模块订阅清单）
		if (!event_terminal.send(attach_evt, acl_key))
			logger.error("Collision_Proxy::订阅事件发送失败");
	}

	//事件处理
	void Collision_Proxy::event_process(std::shared_ptr<Event> evt)
	{
		//空事件检查
		if (!evt)
			return;

		//---------- 配置路由指令 ----------
		if (evt->category == event_protocol::config && evt->tag == event_protocol::load)
		{
			//非本模块的配置指令一律忽略
			if (evt->receiver != module_name)
				return;
			//按配置内容构建碰撞空间与碰撞体
			config_apply(evt->config);
			return;
		}

		//---------- 碰撞模块指令 ----------
		if (evt->category != collision_event::category)
		{
			logger.debug("Collision_Proxy::未识别事件({}/{})", evt->category, evt->tag);
			return;
		}

		//碰撞空间构建
		if (evt->tag == collision_event::region_build)
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::空间构建事件缺少有效字段(region)");
				return;
			}
			//构建碰撞空间
			region_build(region);
			return;
		}

		//碰撞空间卸载
		if (evt->tag == collision_event::region_unload)
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::空间卸载事件缺少有效字段(region)");
				return;
			}
			//卸载碰撞空间
			region_unload(region);
			return;
		}

		//碰撞空间活跃性设置
		if (evt->tag == collision_event::region_state)
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::空间状态事件缺少有效字段(region)");
				return;
			}
			//若缺少活跃性字段
			if (!evt->config.contains(collision_event::field_active) || !evt->config[collision_event::field_active].is_boolean())
			{
				logger.warn("Collision_Proxy::空间状态事件缺少有效字段(active)");
				return;
			}
			//设置空间活跃性
			region_state_set(region, evt->config[collision_event::field_active].get<bool>());
			return;
		}

		//碰撞空间边界设置
		if (evt->tag == collision_event::region_boundary)
		{
			//空间边界配置路径
			string path;
			//读取空间边界配置路径
			if (!text_read(evt->config, collision_event::field_path, path))
			{
				logger.warn("Collision_Proxy::空间边界事件缺少有效字段(path)");
				return;
			}
			//设置空间边界
			region_boundary_set(path);
			return;
		}

		//碰撞空间检测执行（已改为公共 API，不再接受事件触发）

		//简略碰撞结果事件开关设置
		if (evt->tag == collision_event::region_event_output)
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::结果事件开关事件缺少有效字段(region)");
				return;
			}
			//若缺少开关字段
			if (!evt->config.contains(collision_event::field_enabled) || !evt->config[collision_event::field_enabled].is_boolean())
			{
				logger.warn("Collision_Proxy::结果事件开关事件缺少有效字段(enabled)");
				return;
			}
			//设置开关
			region_event_output_set(region, evt->config[collision_event::field_enabled].get<bool>());
			return;
		}

		//跨越状态查询
		if (evt->tag == collision_event::region_cross_state_query)
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::跨越状态查询缺少有效字段(region)");
				return;
			}

			//目标碰撞体编号（可选）
			uint64_t collider_ID = 0;
			//是否只查询单个碰撞体
			bool single = false;
			//若指定了碰撞体编号
			if (evt->config.contains(collision_event::field_collider_ID))
			{
				//读取碰撞体编号
				if (!number_read(evt->config, collision_event::field_collider_ID, collider_ID))
				{
					logger.warn("Collision_Proxy::跨越状态查询字段(collider_ID)非法");
					return;
				}
				single = true;
			}

			//请求编号（可选）
			uint64_t request_ID = 0;
			//若指定了请求编号则读取
			if (evt->config.contains(collision_event::field_request_ID))
				number_read(evt->config, collision_event::field_request_ID, request_ID);

			//查询结果载荷
			json payload;
			//写入空间名称、请求编号与默认结果
			payload[collision_event::field_region] = region;
			payload[collision_event::field_request_ID] = request_ID;
			payload[collision_event::field_found] = false;
			payload[collision_event::field_states] = json::array();

			//查找目标碰撞空间
			Collision_Region* target = backend.region_seek(region);
			//空间存在时输出状态快照
			if (target)
			{
				payload[collision_event::field_found] = true;
				//状态快照
				vector<Cross_State_Entry> entries;
				target->cross_state_snapshot(entries);
				//逐条写入查询结果
				for (const Cross_State_Entry& entry : entries)
				{
					//指定编号时只输出目标碰撞体
					if (single && entry.collider_ID != collider_ID)
						continue;

					//单条状态
					json item;
					item[collision_event::field_collider_ID] = entry.collider_ID;
					item[collision_event::field_state] =
						entry.known ? cross_state_text(entry.state) : "unknown";
					payload[collision_event::field_states].push_back(item);
				}
			}

			//发布查询结果事件
			event_publish(collision_event::region_cross_state_result, payload);
			return;
		}

		//碰撞体构建
		if (evt->tag == collision_event::collider_build)
		{
			//目标空间名称
			string region;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::碰撞体构建事件缺少有效字段(region)");
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
			payload[collision_event::field_region] = region;
			payload[collision_event::field_collider_ID] = *collider_ID;
			//发布碰撞体构建结果事件
			event_publish(collision_event::collider_build_result, payload);
			return;
		}

		//位移向量事件
		//运动事件
		if (evt->tag == collision_event::collider_motion)
		{
			//保存运动事件（供碰撞空间更新位置时重读）
			motion_register(evt->config);
			return;
		}

		//碰撞体卸载、转移、镜像与设置
		if (evt->tag == collision_event::collider_unload || evt->tag == collision_event::collider_transfer ||
			evt->tag == collision_event::collider_mirror || evt->tag == collision_event::collider_set)
		{
			//目标空间名称
			string region;
			//目标碰撞体编号
			uint64_t collider_ID = 0;
			//读取目标空间名称
			if (!text_read(evt->config, collision_event::field_region, region))
			{
				logger.warn("Collision_Proxy::碰撞体事件缺少有效字段(region)");
				return;
			}
			//读取目标碰撞体编号
			if (!number_read(evt->config, collision_event::field_collider_ID, collider_ID))
			{
				logger.warn("Collision_Proxy::碰撞体事件缺少有效字段(collider_ID)");
				return;
			}

			//碰撞体卸载
			if (evt->tag == collision_event::collider_unload)
				collider_unload(collider_ID, region);
			//碰撞体转移
			else if (evt->tag == collision_event::collider_transfer)
				collider_transfer(collider_ID, region);
			//碰撞体镜像
			else if (evt->tag == collision_event::collider_mirror)
				collider_mirror(collider_ID, region);
			//碰撞体设置
			else
				collider_config_apply(region, collider_ID, evt->config);
			return;
		}

		//未识别的碰撞模块指令
		logger.debug("Collision_Proxy::未识别事件({}/{})", evt->category, evt->tag);
	}

	//事件发布
	bool Collision_Proxy::event_publish(const string& tag, const json& payload)
	{
		//构造发布事件
		shared_ptr<Event> evt = event_terminal.build(module_name, "", collision_event::category, tag);
		//若事件内存分配失败
		if (!evt)
		{
			logger.error("Collision_Proxy::事件内存分配失败，发布中止");
			return false;
		}
		//写入事件载荷
		evt->config = payload;

		//发送事件（事件终端按单事件通道发送）
		if (!event_terminal.send(evt, acl_key))
		{
			logger.warn("Collision_Proxy::事件({})发送失败，事件发送通道不可用", tag);
			return false;
		}

		return true;
	}
}