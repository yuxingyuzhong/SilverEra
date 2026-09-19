#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//构造函数
	Prop_Distributor::Prop_Distributor()
	{
	}

	//析构函数
	Prop_Distributor::~Prop_Distributor()
	{

	}

	//事件中转站接入
	void Prop_Distributor::attach(void)
	{
		//构造事件接收入口
		auto event_receive_entry = [this](shared_ptr<event> evt)-> void
			{
				this->event_process(evt);
			};
		//注册事件接收入口
		event_terminal->event_receiver_register(event_receive_entry);
		//更新接入信息
		event_terminal.attach("Prop_Distributor", {}, acl_key);
	}

	//属性槽集合绑定
	void Prop_Distributor::prop_slots_bind(std::function<Object_Pool<Prop>*
		(const uint64_t& distribute_key)> bind_entry)
	{
		//若尚未获取分发权限密钥
		if (!distribute_key.has_value())
		{
			Log::warn("Prop_Distributor::尚未获取分发权限密钥\n无法绑定属性槽集合");
			return;
		}
		props = bind_entry(distribute_key.value());
	}

	//属性槽获取
	unordered_map<string, double>* Prop_Distributor::prop_slot_get(const uint64_t& ID)
	{
		return const_cast<unordered_map<string, double>*>(const_prop_slot_get(ID));
	}

	//只读属性槽获取
	const unordered_map<string, double>* Prop_Distributor::const_prop_slot_get(const uint64_t& ID) const
	{
		//获取属性槽记录迭代器
		auto prop_slot = props->find(ID);
		//若目标属性槽存在
		if (prop_slot != props->end())
			return &prop_slot->prop_get();
		//若目标属性槽不存在
		else
			return nullptr;
	}

	//事件处理
	void Prop_Distributor::event_process(std::shared_ptr<event> evt)
	{
		//若当前为密钥传送事件
		if (evt->category == "Key" && evt->tag == "Distributor")
			//获取权限密钥
			this->distribute_key = evt->config["key"].get<uint64_t>();
	}
}
