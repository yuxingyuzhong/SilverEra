#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "common/types/事件类型.h"
//获取事件终端
#include "src/core/event/Event_Terminal/事件终端.h"
//获取配置检查器
#include "src/tools/Non_GUI/Config_Checker/配置检查器.h"
//获取C++类型Lua端注册方法
#include "common/external/Sol2/sol类型注册.h"
//获取预定义sol2库类型别名
#include "common/external/Sol2/sol类型别名.h"
//获取预定义记录类型
#include "common/types/对象类型.h"
//获取对象池
#include "src/core/object/Object_Pool/对象池.h"
//获取引擎环境
#include "src/tools/Non_GUI/Engine_Env/引擎环境.h"
//获取路径字符串转化方法
#include "src/tools/Non_Gui/Auxi_Algorithm/路径字符串转换.h"
//获取二分查找算法
#include "src/tools/Non_GUI/Auxi_Algorithm/二分查找.h"

namespace engine
{
	//属性槽分发器
	class Prop_Distributor
	{
	private:
		//属性槽记录集合
		Object_Pool<Prop>* props = nullptr;
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//事件发送权限密钥
		int64_t acl_key = 0;

		//属性槽分发权限密钥
		std::optional<uint64_t> distribute_key = std::nullopt;

	public:
		//构造函数
		Prop_Distributor();
		//析构函数
		~Prop_Distributor();

		//事件中转站接入
		void attach(void);
		//属性槽集合绑定
		void prop_slots_bind(std::function< Object_Pool<Prop>*
			(const uint64_t& distribute_key)> bind_entry);
		//属性槽获取
		std::unordered_map<std::string, double>* prop_slot_get(const uint64_t& ID);
		//只读属性槽获取
		const std::unordered_map<std::string, double>* const_prop_slot_get(const uint64_t& ID) const;
	private:
		//事件处理
		void event_process(std::shared_ptr<event> evt);
	};
}
