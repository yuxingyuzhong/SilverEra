#include "../局部命名空间使用.h"
#include "src/tools/Non_GUI/Logging/日志系统.h"

namespace engine
{
	//属性槽分发密钥生成
	bool Entity_Manager::distribute_key_gen(void)
	{
		//若属性槽分发器未注册事件中转站
		if (!event_terminal.check("Prop_Distributor"))
		{
			Log::warn("Entity_Manager::属性槽分发器尚未注册事件中转站信息\n分发密钥生成失败");
			return false;
		}
		//若当前已生成分发密钥
		if (this->distribute_key.has_value())
		{
			Log::warn("Entity_Manager::分发密钥未已生成!!!");
			return false;
		}

		//若所有检验均通过则生成分发密钥
		this->distribute_key = []() {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<uint64_t> dis(0, (numeric_limits<uint64_t>::max)());
			return dis(gen);
			}();

		//构造密钥传送事件
		shared_ptr<config_event> event(new(nothrow) config_event());
		//记录事件发送者
		event->sender_object = "Entity_Manager";
		//记录事件接收者
		event->target_object = "Prop_Distributor";
		//记录事件大类
		event->category = "Key";
		//记录事件标签
		event->tag = "Distribute";
		//记录密钥
		event->config["key"] = this->distribute_key;
		//发送密钥传送事件
		event_terminal(event);
	}

	//属性槽获取
	Object_Pool<prop_record>* Entity_Manager::prop_slot_get(const uint64_t& distribute_key)
	{
		//若当前未生成分发密钥
		if (!this->distribute_key.has_value())
		{
			Log::warn("Entity_Manager::属性槽分发器尚未注册事件中转站信息\n分发密钥未生成");
			return nullptr;
		}
		//若分发密钥不匹配
		else if (this->distribute_key != distribute_key)
		{
			Log::warn("Entity_Manager::分发密钥匹配失败");
			return nullptr;
		}
		//若分发密钥匹配
		else
			return &prop_records;
	}
}
