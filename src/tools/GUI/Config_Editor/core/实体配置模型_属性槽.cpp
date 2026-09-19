#include "src/tools/GUI/Config_Editor/实体配置模型_内部工具.h"

#include <set>

//引擎命名空间
namespace engine
{

    //========================================================================
    // 属性槽配置公开实现
    //========================================================================

    //获取全部属性槽配置（只读）
    const std::vector<属性槽配置>& 实体配置仓库::获取属性槽全部() const
    {
        return 属性槽集合;
    }

    //获取全部属性槽配置（可修改）
    std::vector<属性槽配置>& 实体配置仓库::获取属性槽全部()
    {
        return 属性槽集合;
    }

    //按类型查找属性槽配置
    属性槽配置* 实体配置仓库::查找属性槽(const std::string& type)
    {
        for (auto& cfg : 属性槽集合)
            if (cfg.type == type)
                return &cfg;
        return nullptr;
    }

    //校验属性槽配置
    bool 实体配置仓库::校验属性槽(const 属性槽配置& cfg,
        std::vector<std::string>& errors,
        std::vector<std::string>& warnings) const
    {
        errors.clear();
        warnings.clear();

        //硬性检查
        if (cfg.type.empty())
            errors.push_back("属性槽配置：字段 type 不能为空（Property_Manager field_check 要求）");
        if (cfg.initialize_path.empty())
            errors.push_back("属性槽配置：字段 initialize_path 不能为空（Property_Manager field_check 要求）");

        //软性检查：初始化脚本存在性
        if (!cfg.initialize_path.empty())
        {
            std::filesystem::path script_path = assets_dir / utf8_path(cfg.initialize_path);
            if (!std::filesystem::is_regular_file(script_path))
                warnings.push_back("属性槽初始化脚本不存在：" + cfg.initialize_path +
                    "（Property_Manager 加载时会调用 load_file，路径错误将导致属性槽初始化失败）");
        }

        return errors.empty();
    }

    //保存单个属性槽配置
    bool 实体配置仓库::保存属性槽(属性槽配置& cfg, std::string& error)
    {
        if (cfg.type.empty())
        {
            error = "属性槽配置：实体类型（type）不能为空";
            return false;
        }
        //重名检查：type 不得与「其他」属性槽重复（防止覆盖其他配置文件）
        属性槽配置* other = 查找属性槽(cfg.type);
        if (other != nullptr && other != &cfg)
        {
            error = "属性槽配置已存在，无法保存（会覆盖已有配置）：" + cfg.type;
            return false;
        }

        //确定目标路径（type 变更时迁移文件）
        std::string target_path = 生成属性槽配置路径(cfg.type);
        std::filesystem::path absolute_target = assets_dir / utf8_path(target_path);

        if (!cfg.config_path.empty() && cfg.config_path != target_path)
        {
            std::filesystem::path absolute_old = assets_dir / utf8_path(cfg.config_path);
            if (std::filesystem::exists(absolute_old))
            {
                std::error_code ec;
                std::filesystem::rename(absolute_old, absolute_target, ec);
                if (ec)
                {
                    error = "属性槽文件迁移失败：" + cfg.config_path + " → " + target_path;
                    return false;
                }
            }
        }
        cfg.config_path = target_path;

        //序列化并原子写入属性槽 JSON（临时文件 + 流状态校验 + .bak 备份 + rename）
        nlohmann::json data = 序列化属性槽配置(cfg);
        std::string write_error;
        if (!原子写入文件(absolute_target, data.dump(2), write_error))
        {
            error = "属性槽文件写入失败：" + cfg.config_path + "（" + write_error + "）";
            return false;
        }

        //同步属性槽路由表
        if (property_route_json.is_array())
        {
            bool found = false;
            for (auto& route_data : property_route_json)
            {
                if (route_data.is_object() &&
                    route_data.value("config_path", std::string()) == cfg.config_path)
                {
                    route_data["config_path"] = cfg.config_path;
                    found = true;
                    break;
                }
            }
            if (!found)
                property_route_json.push_back({ {"module", "Property_Manager"}, {"config_path", cfg.config_path} });
        }
        if (!写入属性槽路由())
        {
            error = "属性槽路由表写入失败：" + path_utf8(property_route_path);
            return false;
        }

        return true;
    }

    //新建属性槽配置
    bool 实体配置仓库::新建属性槽(属性槽配置& cfg, std::string& error)
    {
        if (cfg.type.empty())
        {
            error = "属性槽配置：实体类型（type）不能为空";
            return false;
        }
        if (查找属性槽(cfg.type) != nullptr)
        {
            error = "属性槽配置已存在：" + cfg.type;
            return false;
        }

        cfg.config_path = 生成属性槽配置路径(cfg.type);
        nlohmann::json data = 序列化属性槽配置(cfg);
        std::filesystem::path absolute_path = assets_dir / utf8_path(cfg.config_path);
        std::string write_error;
        if (!原子写入文件(absolute_path, data.dump(2), write_error))
        {
            error = "属性槽文件写入失败：" + cfg.config_path + "（" + write_error + "）";
            return false;
        }

        if (property_route_json.is_array())
            property_route_json.push_back({ {"module", "Property_Manager"}, {"config_path", cfg.config_path} });
        if (!写入属性槽路由())
        {
            error = "属性槽路由表写入失败：" + path_utf8(property_route_path);
            return false;
        }

        属性槽集合.push_back(cfg);
        return true;
    }

    //删除属性槽配置（仅移除路由条目，属性槽文件保留）
    bool 实体配置仓库::删除属性槽(const std::string& type, std::string& error)
    {
        属性槽配置* target = 查找属性槽(type);
        if (target == nullptr)
        {
            error = "属性槽配置不存在：" + type;
            return false;
        }
        std::string config_path = target->config_path;

        if (property_route_json.is_array())
        {
            property_route_json.erase(
                std::remove_if(property_route_json.begin(), property_route_json.end(),
                    [&config_path](const nlohmann::json& item)
                    {
                        return item.is_object() &&
                            item.value("config_path", std::string()) == config_path;
                    }),
                property_route_json.end());
        }
        if (!写入属性槽路由())
        {
            error = "属性槽路由表写入失败：" + path_utf8(property_route_path);
            return false;
        }

        属性槽集合.erase(
            std::remove_if(属性槽集合.begin(), 属性槽集合.end(),
                [&type](const 属性槽配置& cfg) { return cfg.type == type; }),
            属性槽集合.end());

        return true;
    }

    //读取属性槽路由表
    bool 实体配置仓库::读取属性槽路由()
    {
        if (!std::filesystem::is_regular_file(property_route_path))
            return false;

        try
        {
            std::ifstream file(property_route_path);
            if (!file.is_open())
                return false;
            file >> property_route_json;
        }
        catch (const std::exception&)
        {
            property_route_json = nlohmann::json();
            return false;
        }

        if (!property_route_json.is_array())
        {
            property_route_json = nlohmann::json();
            return false;
        }

        return true;
    }

    //写入属性槽路由表（原子写入）
    bool 实体配置仓库::写入属性槽路由()
    {
        std::string write_error;
        return 原子写入文件(property_route_path, property_route_json.dump(2), write_error);
    }

    //解析单个属性槽 JSON
    bool 实体配置仓库::解析属性槽配置(const nlohmann::json& data, 属性槽配置& out, std::string& error)
    {
        if (!data.is_object())
        {
            error = "属性槽配置内容非 JSON 对象";
            return false;
        }

        if (data.contains("type") && data["type"].is_string())
            out.type = data["type"];
        else
            out.type.clear();

        if (data.contains("initialize_path") && data["initialize_path"].is_string())
            out.initialize_path = data["initialize_path"];
        else
            out.initialize_path.clear();

        return true;
    }

    //序列化属性槽配置为标准 JSON
    nlohmann::json 实体配置仓库::序列化属性槽配置(const 属性槽配置& cfg) const
    {
        nlohmann::json data;
        data["type"] = cfg.type;
        data["initialize_path"] = cfg.initialize_path;
        return data;
    }

    //按类型生成属性槽默认配置路径
    std::string 实体配置仓库::生成属性槽配置路径(const std::string& type) const
    {
        return "config/property/" + 文件名清理(type) + ".json";
    }
}
