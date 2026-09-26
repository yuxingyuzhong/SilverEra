#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

//引擎命名空间
namespace engine
{
	//构造函数
	Effect_Manager::Effect_Manager()
	{
		//获取权限密钥
		acl_key = event_terminal.acl_key_gen();
	}

	//注册属性槽绑定通道
	void Effect_Manager::bind_entry_register(std::function<std::unordered_map<std::string, double>*
		(const uint64_t& ID)> bind_entry)
	{
		this->bind_entry = bind_entry;
	}

	//事件中转站接入
	void Effect_Manager::attach(void)
	{
		//订阅事件集合记录
		vector<event> needed_events;

		//构造配置加载事件
		needed_events.emplace_back("", "Effect_Manager", "Config", "Load", json::object());
		//构造效应构建事件
		needed_events.emplace_back("", "", "Effect", "Build", json::object());
		//构造效应卸载事件
		needed_events.emplace_back("", "", "Effect", "Unload", json::object());
		//构造效应执行事件
		needed_events.emplace_back("", "", "Effect", "Act", json::object());
		//构建事件接收入口
		auto receive_entry = [this](shared_ptr<event> evt)->void
			{
				this->event_process(evt);
			};
		//注册事件接收入口
		event_terminal->event_receiver_register(receive_entry);

		//更新接入信息
		event_terminal.attach("Effect_Manager", needed_events, acl_key);
	}

	//效应分组查找
	int64_t Effect_Manager::effect_group_seek(const uint64_t& inclusion)
	{
		return binary_search(effect_groups,inclusion,less(), &effect_group::inclusion);
	}

	//效应构建
	optional<uint64_t> Effect_Manager::effect_build(shared_ptr<event> evt)
	{
		//简化表示路径
		auto& config = evt->config;

		//若效应归属字段无效
		if (!Data_Validator::field_check<uint64_t>(config, "inclusion"))
		{
			Log::warn("Effect_Manager::未指定效应归属\n效应构建事件已驳回");
			return nullopt;
		}
		//若效应执行阶段字段无效
		if (!Data_Validator::field_check<uint64_t>(config, "act_phase"))
		{
			Log::warn("Effect_Manager::未指定效应执行阶段\n效应构建事件已驳回");
			return nullopt;
		}
		//若执行优先级字段非字符串和无符号整数
		if (!Data_Validator::field_check<string>(config, "priority") &&
			!Data_Validator::field_check<uint64_t>(config, "priority"))
		{
			Log::warn("Prop_Effect::未定义执行优先级字段\n效应无法加载");
			return nullopt;
	    }

		//构建新效应
		uint64_t ID = effect_set.build();
		//获取新效应记录
		auto new_record_it = effect_set.find(ID);
		auto* new_record = &(*new_record_it);
		//获取新效应
		auto& new_effect = new_record->pro_effect;

		//若效应配置解析异常
		if (!new_effect.config_read(config))
		{
			//卸载新效应
			effect_set.unload(ID);
			//返回无效值
			return nullopt;
		}
		//若效应配置解析正常
		else
		{
			//记录效应归属
			new_record->inclusion = config["inclusion"].get<uint64_t>();
			//绑定效应ID
			new_record->pro_effect.ID_bind(new_record->ID());
			//记录效应执行阶段(字符串转坏为唯一哈希) 
			new_record->act_phase = hash<string>{}(config["act_phase"].get<string>());
			//记录效应执行优先级
			if(config["priority"].is_string())
			{
				//若定义为最高执行优先级
				if(config["priority"].get<string>() == "max")
				   new_record->priority = (numeric_limits<uint64_t>::max)();
				//若为其余异常定义
				else
				{
					Log::warn("Effect_Manager::执行优先级字段内容异常");
					//卸载新效应
					effect_set.unload(ID);
					//返回无效值
					return nullopt;
				}
			}
			else
				new_record->priority = config["priority"].get<uint64_t>();
			//绑定效应修改对象
			new_record->pro_effect.effect_object_bind(bind_entry(new_record->inclusion));

			//获取事件终端
			auto& terminal = new_effect.event_terminal;
			//构造事件入口
			auto event_send_entry = [this](std::vector<std::shared_ptr<event>> events)->void
				{
					//直接转发至其余模块
					this->event_terminal.send(events,acl_key);
				};
			//配置事件终端
			terminal->event_sender_register(event_send_entry);

			//获取效应分组索引
			int64_t group_index = effect_group_seek(new_record->inclusion);
			//若返回索引无效
			if (group_index < 0)
			{
				//创建效应分组
				effect_groups.push_back({ new_record->inclusion ,{} });
				//设置效应分组索引
				group_index = effect_groups.size() - 1;
			}
			//获取目标效应分组
			auto& group = effect_groups[group_index];

			//配置效应信息
			evt->config["ID"] = new_record->ID();
			//发送修饰事件
			for (auto& effect : group.effects)
				effect->pro_effect.event_terminal.receive(evt);
			//将新建效应加入分组
			group.effects.push_back(new_record);

			//按执行优先级设置降序排列
			effect_set.sort_order_set(true, &effect_record::priority);
			
			//返回新效应ID
			return new_record->ID();
		}
	}

	//效应卸载
	bool Effect_Manager::effect_unload(std::shared_ptr<event> evt)
	{
		//简化表示路径
		auto& config = evt->config;

		//若效应ID字段无效
		if (!Data_Validator::field_check<uint64_t>(config, "target_ID"))
		{
			Log::warn("Effect_Manager::效应ID未定义\n效应卸载事件已驳回");
			return false;
		}

		//获取效应ID
		uint64_t target_ID = config["target_ID"].get<uint64_t>();
		//获取目标效应记录
		auto target_record_it = effect_set.find(target_ID);
		auto* target_record = &(*target_record_it);
		//获取效应分组索引
		int64_t group_index = effect_group_seek(target_record->inclusion);
		//获取效应分组
		auto& group = effect_groups[group_index];
		//获取效应集合
		auto& effects = group.effects;

		//若组内仅有目标效应
		if (effects.size() == 1)
			//卸载该效应分组
			effect_groups.erase(effect_groups.begin() + group_index);
		else
		{
			//配置效应信息
			evt->config["inclusion"] = target_record->inclusion;
			evt->config["name"] = target_record->pro_effect.effect_name_get();

			//目标效应索引记录
			int64_t target_index = -1;
			//匹配目标效应
			for (int match_time = 0; match_time < effects.size(); match_time++)
			{
				//简化表示路径
				auto& effect = effects[match_time];
				//若非目标效应则发送效应销毁事件
				if (effect->ID() != target_record->ID())
					effect->pro_effect.event_terminal.receive(evt);
				//若为目标效应则记录其索引
				else
					target_index = match_time;
			}

			//卸载组内效应记录
			effects.erase(effects.begin() + target_index);
		}

		//卸载目标效应
		effect_set.unload(target_ID);

		//返回卸载成功
		return true;
	}

	//效应执行
	void Effect_Manager::effect_act(uint64_t phase)
	{
		//执行效应
		for (auto& effect_record : effect_set.data())
		{
			//若记录有效
			if(effect_record.valid())
			{
				//简化表示路径
				auto& effect = effect_record.pro_effect;
				//若执行阶段标记匹配
				if (effect_record.act_phase == phase)
					effect.effect_act();
			}
		}
	}

	//事件处理
	void Effect_Manager::event_process(std::shared_ptr<event> evt)
	{
		//若为效应大类分支
		if (evt->category == "Effect")
		{
			//简化表示路径
			auto& tag = evt->tag;
			auto& config = evt->config;

			//若为效应构建事件
			if (tag == "Build")
				//构建新效应
				optional<uint64_t> effect_ID = effect_build(evt);
			//若为效应卸载事件
			else if (tag == "Unload")
				//卸载指定效应
				effect_unload(evt);
			//若为效应触发事件
			else if (tag == "Act")
			{
				//若效应执行阶段字段未定义
				if (!Data_Validator::field_check<string>(config, "act_phase"))
				{
					Log::warn("Effect_Manager::效应执行阶段未定义\n效应触发事件已驳回");
					return;
				}

				//获取效应执行时段(转化为唯一哈希)
				uint64_t act_phase = hash<string>{}(config["act_phase"].get<string>());
				//触发符合时间段的效应
				effect_act(act_phase);
			}
			//若为其余事件
			else
			{
				//若效应ID字段未定义
				if (!Data_Validator::field_check<uint64_t>(config, "target_ID"))
				{
					Log::warn("Effect_Manager::目标效应ID未定义\n未知事件已驳回");
					return;
				}

				//获取目标效应ID
				uint64_t target_ID = config["target_ID"].get<uint64_t>();
				//若目标效应不存在
				if (effect_set.find(target_ID) == effect_set.end())
				{
					Log::warn("Effect_Manager::目标效应ID不存在\n未知事件已驳回");
					return;
				}
				//若目标效应存在
				else
				{
					//获取目标效应
					auto* effect = &(*effect_set.find(target_ID));
					//发送事件
					effect->pro_effect.event_terminal.receive(evt);
				}	
			}
		}
	}

}

