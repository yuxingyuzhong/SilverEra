#include "src/tools/GUI/Config_Editor/实体配置模型_内部工具.h"

#include <set>

//引擎命名空间
namespace engine
{

    //========================================================================
    // 通用配置实现（自定义模块）
    //========================================================================

    //获取全部通用配置
    const std::vector<通用配置>& 实体配置仓库::获取通用配置全部() const
    {
        return 通用配置集合;
    }

    //获取全部通用配置（可修改）
    std::vector<通用配置>& 实体配置仓库::获取通用配置全部()
    {
        return 通用配置集合;
    }

    //按模块名 + 条目名查找通用配置
    通用配置* 实体配置仓库::查找通用配置(const std::string& module, const std::string& name)
    {
        for (auto& cfg : 通用配置集合)
            if (cfg.模块名 == module && cfg.条目名 == name)
                return &cfg;
        return nullptr;
    }

    //按格式定义生成默认字段值
    nlohmann::json 实体配置仓库::生成默认字段值(const 配置格式& fmt)
    {
        nlohmann::json data = nlohmann::json::object();
        for (const auto& f : fmt.字段)
        {
            switch (f.类型)
            {
            case 配置字段类型::文本:
            case 配置字段类型::脚本路径:
                data[f.字段名] = "";
                break;
            case 配置字段类型::文本列表:
                data[f.字段名] = nlohmann::json::array();
                break;
            case 配置字段类型::事件对列表:
                data[f.字段名] = nlohmann::json::array();
                break;
            case 配置字段类型::整数:
                data[f.字段名] = 0;
                break;
            case 配置字段类型::浮点数:
                data[f.字段名] = 0.0;
                break;
            case 配置字段类型::布尔:
                data[f.字段名] = false;
                break;
            }
        }
        return data;
    }

    //从字段值中提取条目名
    std::string 实体配置仓库::提取条目名(const nlohmann::json& data)
    {
        if (data.is_object())
        {
            //优先取 type，其次 name，再其次 id
            for (const char* key : { "type", "name", "id" })
            {
                if (data.contains(key) && data[key].is_string())
                    return data[key].get<std::string>();
            }
        }
        return std::string();
    }

    //新建通用配置
    bool 实体配置仓库::新建通用配置(通用配置& cfg, std::string& error)
    {
        if (cfg.模块名.empty())
        {
            error = "模块名不能为空";
            return false;
        }
        if (cfg.条目名.empty())
        {
            error = "条目名不能为空";
            return false;
        }
        if (查找通用配置(cfg.模块名, cfg.条目名) != nullptr)
        {
            error = "配置已存在：" + cfg.模块名 + "/" + cfg.条目名;
            return false;
        }
        配置格式* fmt = 查找格式(cfg.模块名);
        if (fmt == nullptr)
        {
            error = "模块格式不存在：" + cfg.模块名;
            return false;
        }
        if (fmt->内置)
        {
            error = "内置模块请使用专用创建入口：" + cfg.模块名;
            return false;
        }

        //生成配置路径并写入文件（原子写入）
        cfg.config_path = 生成通用配置路径(cfg.模块名, cfg.条目名);
        std::filesystem::path absolute_path = assets_dir / utf8_path(cfg.config_path);
        std::string write_error;
        if (!原子写入文件(absolute_path, cfg.字段值.dump(2), write_error))
        {
            error = "配置文件写入失败：" + cfg.config_path + "（" + write_error + "）";
            return false;
        }

        //追加自定义模块路由条目
        auto it = 自定义路由表.find(cfg.模块名);
        if (it == 自定义路由表.end())
        {
            nlohmann::json empty = nlohmann::json::array();
            it = 自定义路由表.emplace(cfg.模块名, std::move(empty)).first;
        }
        if (it->second.is_array())
            it->second.push_back({ {"module", cfg.模块名}, {"config_path", cfg.config_path} });
        if (!写入自定义路由(cfg.模块名, it->second))
        {
            error = "路由表写入失败：custom_" + 文件名清理(cfg.模块名);
            return false;
        }

        //加入内存集合
        通用配置集合.push_back(cfg);
        return true;
    }

    //保存通用配置
    bool 实体配置仓库::保存通用配置(通用配置& cfg, std::string& error)
    {
        if (cfg.模块名.empty())
        {
            error = "模块名不能为空";
            return false;
        }
        if (cfg.条目名.empty())
        {
            error = "条目名不能为空";
            return false;
        }
        //重名检查：条目名不得与「其他」同模块配置重复（防止覆盖其他配置文件）
        通用配置* other = 查找通用配置(cfg.模块名, cfg.条目名);
        if (other != nullptr && other != &cfg)
        {
            error = "配置已存在，无法保存（会覆盖已有配置）：" + cfg.模块名 + "/" + cfg.条目名;
            return false;
        }

        //确定目标路径（条目名变更时迁移文件）
        std::string target_path = 生成通用配置路径(cfg.模块名, cfg.条目名);
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
                    error = "配置文件迁移失败：" + cfg.config_path + " → " + target_path;
                    return false;
                }
            }
        }
        cfg.config_path = target_path;

        //写入 JSON（原子写入）
        std::filesystem::path absolute_path = assets_dir / utf8_path(cfg.config_path);
        std::string write_error;
        if (!原子写入文件(absolute_path, cfg.字段值.dump(2), write_error))
        {
            error = "配置文件写入失败：" + cfg.config_path + "（" + write_error + "）";
            return false;
        }

        //同步自定义模块路由表
        auto it = 自定义路由表.find(cfg.模块名);
        if (it == 自定义路由表.end())
        {
            nlohmann::json empty = nlohmann::json::array();
            it = 自定义路由表.emplace(cfg.模块名, std::move(empty)).first;
        }
        if (it->second.is_array())
        {
            bool found = false;
            for (auto& route_data : it->second)
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
                it->second.push_back({ {"module", cfg.模块名}, {"config_path", cfg.config_path} });
        }
        if (!写入自定义路由(cfg.模块名, it->second))
        {
            error = "路由表写入失败：custom_" + 文件名清理(cfg.模块名);
            return false;
        }

        return true;
    }

    //删除通用配置
    bool 实体配置仓库::删除通用配置(const std::string& module, const std::string& name, std::string& error)
    {
        通用配置* target = 查找通用配置(module, name);
        if (target == nullptr)
        {
            error = "配置不存在：" + module + "/" + name;
            return false;
        }
        std::string config_path = target->config_path;

        //从自定义模块路由表移除条目
        auto it = 自定义路由表.find(module);
        if (it != 自定义路由表.end() && it->second.is_array())
        {
            it->second.erase(
                std::remove_if(it->second.begin(), it->second.end(),
                    [&config_path](const nlohmann::json& item)
                    {
                        return item.is_object() &&
                            item.value("config_path", std::string()) == config_path;
                    }),
                it->second.end());
            if (!写入自定义路由(module, it->second))
            {
                error = "路由表写入失败：custom_" + 文件名清理(module);
                return false;
            }
        }

        //从内存集合移除
        通用配置集合.erase(
            std::remove_if(通用配置集合.begin(), 通用配置集合.end(),
                [&module, &name](const 通用配置& cfg)
                {
                    return cfg.模块名 == module && cfg.条目名 == name;
                }),
            通用配置集合.end());

        return true;
    }

    //校验通用配置
    bool 实体配置仓库::校验通用配置(const 配置格式& fmt, const 通用配置& cfg,
        std::vector<std::string>& errors, std::vector<std::string>& warnings) const
    {
        errors.clear();
        warnings.clear();

        for (const auto& f : fmt.字段)
        {
            const bool has = cfg.字段值.contains(f.字段名);
            const auto& value = cfg.字段值[f.字段名];

            switch (f.类型)
            {
            case 配置字段类型::文本:
            case 配置字段类型::脚本路径:
            {
                std::string str = has && value.is_string() ? value.get<std::string>() : std::string();
                if (f.必填 && str.empty())
                    errors.push_back("字段 " + f.字段名 + " 不能为空（必填）");
                //脚本路径存在性软检查
                if (f.类型 == 配置字段类型::脚本路径 && !str.empty())
                {
                    std::filesystem::path script_path = assets_dir / utf8_path(str);
                    if (!std::filesystem::is_regular_file(script_path))
                        warnings.push_back("脚本路径不存在：" + str + "（模块加载时可能失败）");
                }
                break;
            }
            case 配置字段类型::文本列表:
            {
                std::vector<std::string> list;
                if (has && value.is_array())
                {
                    for (const auto& item : value)
                        if (item.is_string())
                            list.push_back(item.get<std::string>());
                }
                if (f.必填 && list.empty())
                    errors.push_back("字段 " + f.字段名 + " 不能为空列表（必填）");
                for (const auto& item : list)
                    if (item.empty())
                        errors.push_back("字段 " + f.字段名 + " 中存在空字符串条目");
                break;
            }
            case 配置字段类型::事件对列表:
            {
                std::vector<std::pair<std::string, std::string>> list;
                if (has && value.is_array())
                {
                    for (const auto& item : value)
                    {
                        if (item.is_array() && item.size() >= 2 &&
                            item[0].is_string() && item[1].is_string())
                            list.emplace_back(item[0].get<std::string>(), item[1].get<std::string>());
                        else if (item.is_object() &&
                            item.contains("category") && item["category"].is_string() &&
                            item.contains("tag") && item["tag"].is_string())
                            list.emplace_back(item["category"].get<std::string>(),
                                item["tag"].get<std::string>());
                    }
                }
                if (f.必填 && list.empty())
                    errors.push_back("字段 " + f.字段名 + " 不能为空列表（必填）");
                for (const auto& item : list)
                {
                    if (item.first.empty())
                        errors.push_back("字段 " + f.字段名 + " 中存在空的事件分类（category）");
                    if (item.second.empty())
                        errors.push_back("字段 " + f.字段名 + " 中存在空的事件标签（tag）");
                }
                break;
            }
            case 配置字段类型::整数:
                if (!has || !value.is_number_integer())
                    errors.push_back("字段 " + f.字段名 + " 必须是整数");
                break;
            case 配置字段类型::浮点数:
                if (!has || !value.is_number())
                    errors.push_back("字段 " + f.字段名 + " 必须是数字");
                break;
            case 配置字段类型::布尔:
                if (!has || !value.is_boolean())
                    errors.push_back("字段 " + f.字段名 + " 必须是布尔值");
                break;
            }
        }

        return errors.empty();
    }

    //读取自定义模块路由表
    bool 实体配置仓库::读取自定义路由(const std::string& module, nlohmann::json& out)
    {
        std::filesystem::path route_path = assets_dir / "config" / "route" /
            ("custom_" + 文件名清理(module) + ".json");
        if (!std::filesystem::is_regular_file(route_path))
            return false;

        try
        {
            std::ifstream file(route_path);
            if (!file.is_open())
                return false;
            file >> out;
        }
        catch (const std::exception&)
        {
            out = nlohmann::json();
            return false;
        }

        return out.is_array();
    }

    //写入自定义模块路由表（原子写入）
    bool 实体配置仓库::写入自定义路由(const std::string& module, const nlohmann::json& data)
    {
        std::filesystem::path route_path = assets_dir / "config" / "route" /
            ("custom_" + 文件名清理(module) + ".json");
        std::string write_error;
        return 原子写入文件(route_path, data.dump(2), write_error);
    }

    //加载单个自定义模块的全部通用配置（返回跳过的损坏/缺失配置数量）
    int 实体配置仓库::加载模块通用配置(const 配置格式& fmt)
    {
        int 跳过数 = 0;
        nlohmann::json route_data;
        if (!读取自定义路由(fmt.模块名, route_data))
        {
            //路由缺失不算致命错误：可后续在编辑器内新建
            自定义路由表[fmt.模块名] = nlohmann::json::array();
            return 0;
        }
        自定义路由表[fmt.模块名] = route_data;

        for (const auto& route_item : route_data)
        {
            if (!route_item.is_object())
            {
                ++跳过数;
                continue;
            }
            if (!route_item.contains("config_path") || !route_item["config_path"].is_string())
            {
                ++跳过数;
                continue;
            }

            std::string config_path = route_item["config_path"];
            //安全校验：必须以 config/ 开头，禁止 .. 跳转
            if (config_path.rfind("config/", 0) != 0)
            {
                ++跳过数;
                continue;
            }
            if (config_path.find("..") != std::string::npos)
            {
                ++跳过数;
                continue;
            }

            std::filesystem::path absolute_path = assets_dir / utf8_path(config_path);
            if (!std::filesystem::is_regular_file(absolute_path))
            {
                ++跳过数;
                continue;
            }

            nlohmann::json data;
            try
            {
                std::ifstream file(absolute_path);
                if (!file.is_open())
                {
                    ++跳过数;
                    continue;
                }
                file >> data;
            }
            catch (const std::exception&)
            {
                ++跳过数;
                continue;
            }
            if (!data.is_object())
            {
                ++跳过数;
                continue;
            }

            通用配置 cfg;
            cfg.模块名 = fmt.模块名;
            cfg.字段值 = std::move(data);
            cfg.条目名 = 提取条目名(cfg.字段值);
            if (cfg.条目名.empty())
            {
                //无 type/name 字段时用文件名（去扩展名）兜底
                cfg.条目名 = path_utf8(absolute_path.stem());
            }
            cfg.config_path = config_path;
            通用配置集合.push_back(std::move(cfg));
        }

        return 跳过数;
    }

    //按模块名生成通用配置路径
    std::string 实体配置仓库::生成通用配置路径(const std::string& module, const std::string& name) const
    {
        return "config/custom/" + 文件名清理(module) + "/" + 文件名清理(name) + ".json";
    }
}
