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

	//碰撞响应登记（回复事件存入本地回复信箱）
	void Collision_Proxy::collision_response_register(const json& config)
	{
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//读取目标碰撞体编号
		if (!number_read(config, "collider_ID", collider_ID))
		{
			Log::warn("Collision_Proxy::碰撞响应缺少有效字段(collider_ID)");
			return;
		}

		//存入回复信箱（同编号的新回复覆盖旧回复）
		collision_responses[collider_ID] = config;
	}

	//碰撞响应取件（取出并从信箱中移除）
	bool Collision_Proxy::collision_response_take(const uint64_t collider_ID, json& receiver)
	{
		//查找该编号的回复
		auto it = collision_responses.find(collider_ID);
		//若该编号尚无回复
		if (it == collision_responses.end())
			return false;

		//取出回复并从信箱中移除
		receiver = it->second;
		collision_responses.erase(it);
		return true;
	}

	//碰撞响应施加（按回复内容作废或改写位移）
	bool Collision_Proxy::collision_response_apply(const uint64_t collider_ID, const json& response)
	{
		//回复类型
		string kind;
		//读取回复类型
		if (!text_read(response, "response", kind))
		{
			Log::warn("Collision_Proxy::碰撞体({})的碰撞响应缺少有效字段(response)", collider_ID);
			return false;
		}

		//停止运动：位移向量作废
		if (kind == "stop")
		{
			//对持有该编号的空间作废位移
			return collider_set_dispatch(collider_ID, [collider_ID](Collision_Region& region)
				{
					return region.collider_displacement_void(collider_ID);
				});
		}

		//继续运动：位移向量保持
		if (kind == "keep")
			return true;

		//继续运动：位移向量变化
		if (kind == "change")
		{
			//改写后的位移向量
			Vector3 displacement;
			//读取改写后的位移向量
			if (!vector_read(response, "displacement", displacement))
			{
				Log::warn("Collision_Proxy::碰撞体({})的碰撞响应字段(displacement)非法", collider_ID);
				return false;
			}

			//更新已保存的位移事件（供后续更新位置时重读）
			auto it = displacement_events.find(collider_ID);
			if (it != displacement_events.end())
				it->second["displacement"] =
					json::array({ displacement.x(), displacement.y(), displacement.z() });

			//对持有该编号的空间改写位移
			return collider_set_dispatch(collider_ID, [collider_ID, &displacement](Collision_Region& region)
				{
					return region.collider_displacement_replace(collider_ID, displacement);
				});
		}

		//未识别的回复类型
		Log::warn("Collision_Proxy::碰撞体({})的碰撞响应类型({})未识别", collider_ID, kind);
		return false;
	}

	//碰撞响应流程（发布碰撞事件、回查回复、搁置重发、施加响应）
	void Collision_Proxy::collision_protocol(const string& region, const vector<Collision_Result>& pairs)
	{
		//碰撞体编号 → 该碰撞体参与的碰撞对集合
		unordered_map<uint64_t, json> collisions;
		//逐个碰撞对登记到两侧碰撞体
		for (const Collision_Result& pair : pairs)
		{
			//单个碰撞对载荷
			json item;
			//写入碰撞体编号
			item["collider_A"] = pair.collider_A;
			item["collider_B"] = pair.collider_B;
			//分别登记到碰撞对两侧
			for (uint64_t collider_ID : { pair.collider_A, pair.collider_B })
			{
				//首次登记时初始化数组
				if (!collisions.count(collider_ID))
					collisions[collider_ID] = json::array();
				//追加碰撞对
				collisions[collider_ID].push_back(item);
			}
		}

		//第一轮：逐个碰撞体发布碰撞事件并立即回查回复
		vector<uint64_t> pending;
		for (const auto& [collider_ID, peer_array] : collisions)
		{
			//碰撞事件载荷
			json payload;
			//写入空间名称与碰撞体编号
			payload["region"] = region;
			payload["collider_ID"] = collider_ID;
			//写入参与碰撞对
			payload["collisions"] = peer_array;
			//发布碰撞事件
			event_publish("ColliderCollision", payload);

			//回复
			json response;
			//立即回查回复信箱
			if (collision_response_take(collider_ID, response))
				collision_response_apply(collider_ID, response);
			//未收到回复则暂时搁置
			else
				pending.push_back(collider_ID);
		}

		//第二轮：其余正常碰撞体处理完毕后再次回查搁置项
		for (uint64_t collider_ID : pending)
		{
			//回复
			json response;
			//再次回查回复信箱
			if (collision_response_take(collider_ID, response))
			{
				//施加响应
				collision_response_apply(collider_ID, response);
				continue;
			}

			//碰撞事件载荷（重发时重建）
			json payload;
			//写入空间名称与碰撞体编号
			payload["region"] = region;
			payload["collider_ID"] = collider_ID;
			//写入参与碰撞对
			payload["collisions"] = collisions[collider_ID];
			//仍未收到回复则再次发布碰撞事件
			event_publish("ColliderCollision", payload);

			//重发后再次回查
			if (collision_response_take(collider_ID, response))
			{
				//施加响应
				collision_response_apply(collider_ID, response);
				continue;
			}

			//仍无响应则日志报错并搁置处理
			Log::error("Collision_Proxy::碰撞体({})未收到碰撞响应，事件搁置处理", collider_ID);
		}
	}
}