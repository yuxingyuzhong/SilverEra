#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统.h"

namespace engine
{
	//文件内部辅助设施（不对外暴露）
	namespace
	{
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

	//位移事件登记（保存位移事件配置，同编号的新事件覆盖旧事件）
	void Collision_Proxy::displacement_register(const json& config)
	{
		//目标碰撞体编号
		uint64_t collider_ID = 0;
		//读取目标碰撞体编号
		if (!number_read(config, "collider_ID", collider_ID))
		{
			Log::warn("Collision_Proxy::位移事件缺少有效字段(collider_ID)");
			return;
		}

		//位移向量
		Vector3 displacement;
		//读取位移向量（非法配置不予保存）
		if (!vector_read(config, "displacement", displacement))
		{
			Log::warn("Collision_Proxy::碰撞体({})字段(displacement)非法", collider_ID);
			return;
		}

		//保存位移事件（同编号的新事件覆盖旧事件）
		displacement_events[collider_ID] = config;
		//新位移事件重新启用位移（解除此前碰撞响应产生的作废）
		collider_set_dispatch(collider_ID, [collider_ID, &displacement](Collision_Region& region)
			{
				return region.collider_displacement_replace(collider_ID, displacement);
			});
	}

	//位移向量查询（供碰撞空间在更新位置时读取最新位移事件）
	bool Collision_Proxy::displacement_seek(const uint64_t collider_ID, Vector3& receiver) const
	{
		//查找该编号的位移事件
		auto it = displacement_events.find(collider_ID);
		//若该编号尚无位移事件
		if (it == displacement_events.end())
			return false;

		//重读事件配置内的位移向量
		return vector_read(it->second, "displacement", receiver);
	}

	//位移读取回调注入（碰撞空间经此回调读取最新位移事件）
	void Collision_Proxy::region_displacement_link(Collision_Region& region)
	{
		//注入读取回调（每次调用都取当前保存的最新位移事件）
		region.displacement_reader_set([this](const uint64_t collider_ID, Vector3& receiver)
			{
				return this->displacement_seek(collider_ID, receiver);
			});
	}
}