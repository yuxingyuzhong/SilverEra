#pragma once
//预编译头
#include "common/前置头文件包含.h"
//获取预定义事件类型
#include "common/types/事件类型.h"
//获取事件终端
#include "src/core/event/Event_Terminal/事件终端.h"
//获取C++类型Lua端注册方法
#include "common/external/Sol2/sol类型注册.h"
//获取预定义sol2库类型别名
#include "common/external/Sol2/sol类型别名.h"


//游戏引擎命名空间
namespace engine
{
	//动态实体
	class Entity
	{
	private:
		//实体类型标签
		std::string type{};
		//实体编号
		int64_t ID = 0;
		//通用属性槽
		std::unordered_map<std::string, double>* property_slot;

		// ———— 事件相关 ———— 
	public:
		//事件终端
		Event_Terminal event_terminal;
	private:
		//权限密钥
		int64_t acl_key = 0;
		//行为脚本
		LuaState action;

	public:
		//构造函数
		Entity(void);
		//构造函数
		Entity(const int64_t& ID);
		//构造函数
		Entity(const int64_t& ID, const std::string& load_path);

		//禁用拷贝
		Entity(const Entity&) = delete;
		Entity& operator=(const Entity&) = delete;

		//移动构造函数
		Entity(Entity&&) = default;
		//移动赋值函数
		Entity& operator=(Entity&&) = default;

		//析构函数
		~Entity();

		//ID绑定
		void ID_bind(const uint64_t& ID);
		//ID信息获取
		uint64_t ID_get(void);
		//类型信息获取
		std::string type_get(void);
		//属性槽绑定
		void prop_slot_bind(std::unordered_map<std::string, double>* ptr);
		//行为加载
		void action_load(const std::string& load_path);
		//行为决策
		virtual void act(void);

	};

}