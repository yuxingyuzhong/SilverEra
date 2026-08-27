#include "src/tools/GUI/Config_Editor/实体配置模型_内部工具.h"
#include "src/tools/Non_GUI/Engine_Env/引擎环境.h"

#include <set>

//引擎命名空间
namespace engine
{
    //========================================================================
    // 实体配置仓库
    //========================================================================

    //构造函数：定位资源目录
    实体配置仓库::实体配置仓库()
    {
        //资源基目录 = exe 所在目录 / assets（与 Config_Loader::base_dir_get 一致）
        assets_dir = Engine_Env::exe_dir_get() / "assets";
        //路由文件路径
        route_path = assets_dir / "config" / "route" / "entity.json";
        //属性槽路由文件路径
        property_route_path = assets_dir / "config" / "route" / "property.json";
        //配置格式定义目录
        format_dir = assets_dir / "config" / "format";
    }

    //加载全部实体配置
    bool 实体配置仓库::加载()
    {
        //清空旧数据
        实体集合.clear();
        route_json = nlohmann::json();
        属性槽集合.clear();
        property_route_json = nlohmann::json();
        //重置损坏统计
        上次加载跳过数 = 0;

        //读取路由表
        if (!读取路由())
            return false;

        //遍历路由条目，读取每个实体配置
        for (const auto& route_data : route_json)
        {
            //字段存在性与类型检查
            if (!route_data.is_object())
            {
                ++上次加载跳过数;
                continue;
            }
            if (!route_data.contains("config_path") || !route_data["config_path"].is_string())
            {
                ++上次加载跳过数;
                continue;
            }

            //获取配置相对路径
            std::string config_path = route_data["config_path"];
            //构建绝对路径（安全校验：必须以 config/ 开头，禁止 .. 跳转）
            if (config_path.rfind("config/", 0) != 0)
            {
                ++上次加载跳过数;
                continue;
            }
            if (config_path.find("..") != std::string::npos)
            {
                ++上次加载跳过数;
                continue;
            }

            std::filesystem::path absolute_path = assets_dir / utf8_path(config_path);
            if (!std::filesystem::is_regular_file(absolute_path))
            {
                ++上次加载跳过数;
                continue;
            }

            //读取实体 JSON
            nlohmann::json data;
            try
            {
                std::ifstream file(absolute_path);
                if (!file.is_open())
                {
                    ++上次加载跳过数;
                    continue;
                }
                file >> data;
            }
            catch (const std::exception&)
            {
                ++上次加载跳过数;
                continue;
            }

            //解析为数据模型
            实体配置 cfg;
            std::string parse_error;
            if (!解析实体配置(data, cfg, parse_error))
            {
                ++上次加载跳过数;
                continue;
            }

            //记录来源路径（用于保存时写回原文件）
            cfg.config_path = config_path;
            实体集合.push_back(std::move(cfg));
        }

        // —— 加载属性槽配置（独立于实体配置，config/property/）——
        //属性槽路由缺失不算致命错误：属性槽配置可后续在编辑器内新建
        if (读取属性槽路由())
        {
            for (const auto& route_data : property_route_json)
            {
                if (!route_data.is_object())
                {
                    ++上次加载跳过数;
                    continue;
                }
                if (!route_data.contains("config_path") || !route_data["config_path"].is_string())
                {
                    ++上次加载跳过数;
                    continue;
                }

                std::string config_path = route_data["config_path"];
                if (config_path.rfind("config/", 0) != 0)
                {
                    ++上次加载跳过数;
                    continue;
                }
                if (config_path.find("..") != std::string::npos)
                {
                    ++上次加载跳过数;
                    continue;
                }

                std::filesystem::path absolute_path = assets_dir / utf8_path(config_path);
                if (!std::filesystem::is_regular_file(absolute_path))
                {
                    ++上次加载跳过数;
                    continue;
                }

                nlohmann::json data;
                try
                {
                    std::ifstream file(absolute_path);
                    if (!file.is_open())
                    {
                        ++上次加载跳过数;
                        continue;
                    }
                    file >> data;
                }
                catch (const std::exception&)
                {
                    ++上次加载跳过数;
                    continue;
                }

                属性槽配置 prop;
                std::string parse_error;
                if (!解析属性槽配置(data, prop, parse_error))
                {
                    ++上次加载跳过数;
                    continue;
                }

                prop.config_path = config_path;
                属性槽集合.push_back(std::move(prop));
            }
        }

        // —— 加载配置格式定义（内置种子 + 磁盘格式文件）——
        格式集合.clear();
        通用配置集合.clear();
        自定义路由表.clear();
        加载格式();

        // —— 加载自定义模块的通用配置 ——
        //遍历所有非内置格式（自定义模块），读取各自路由 + 配置 JSON
        for (const auto& fmt : 格式集合)
        {
            if (fmt.内置)
                continue;
            上次加载跳过数 += 加载模块通用配置(fmt);
        }

        return true;
    }

    //获取全部实体配置
    const std::vector<实体配置>& 实体配置仓库::获取全部() const
    {
        return 实体集合;
    }

    //获取全部实体配置（可修改）
    std::vector<实体配置>& 实体配置仓库::获取全部()
    {
        return 实体集合;
    }

    //按类型查找实体配置
    实体配置* 实体配置仓库::查找类型(const std::string& type)
    {
        for (auto& cfg : 实体集合)
            if (cfg.type == type)
                return &cfg;
        return nullptr;
    }

    //获取资源目录
    const std::filesystem::path& 实体配置仓库::资源目录() const
    {
        return assets_dir;
    }

    //获取脚本相对路径列表
    std::vector<std::string> 实体配置仓库::获取脚本列表() const
    {
        std::vector<std::string> result;
        std::filesystem::path scripts_dir = assets_dir / "scripts";
        if (!std::filesystem::is_directory(scripts_dir))
            return result;

        //递归扫描 scripts 目录下的所有 .lua 文件
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(scripts_dir, ec);
            it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec))
                continue;
            if (it->path().extension() != ".lua")
                continue;

            //转换为相对 assets/ 的路径（正斜杠，UTF-8）
            std::string relative = path_utf8(it->path().lexically_relative(assets_dir));
            std::replace(relative.begin(), relative.end(), '\\', '/');
            result.push_back(std::move(relative));
        }

        //按字典序排序，方便 UI 浏览
        std::sort(result.begin(), result.end());
        return result;
    }

    //校验配置
    bool 实体配置仓库::校验(const 实体配置& cfg,
        std::vector<std::string>& errors,
        std::vector<std::string>& warnings) const
    {
        errors.clear();
        warnings.clear();

        // —— 硬性检查（复刻 Config_Checker::field_check 逻辑） ——

        //字段 "type"：string 且非空
        if (cfg.type.empty())
            errors.push_back("字段 type 不能为空（引擎 field_check 要求）");

        //字段 "decision_load_path"：string 且非空（Entity_Manager 新格式必填）
        if (cfg.decision_load_path.empty())
            errors.push_back("字段 decision_load_path 不能为空（Entity_Manager 现要求该字段，旧配置需补填）");

        //字段 "acls"：vector<string> 且非空
        if (cfg.acls.empty())
            errors.push_back("字段 acls 不能为空（引擎 field_check 对容器有非空检查）");
        for (const auto& acl : cfg.acls)
            if (acl.empty())
                errors.push_back("字段 acls 中存在空字符串条目");

        //字段 "needed_events"：vector<pair<string,string>> 且非空
        if (cfg.needed_events.empty())
            errors.push_back("字段 needed_events 不能为空（引擎 field_check 对容器有非空检查）");
        for (const auto& evt : cfg.needed_events)
        {
            if (evt.first.empty())
                errors.push_back("needed_events 中存在空的事件分类（category）");
            if (evt.second.empty())
                errors.push_back("needed_events 中存在空的事件标签（tag）");
        }

        // —— 软性检查（警告） ——

        //决策树脚本存在性检查
        if (!cfg.decision_load_path.empty())
        {
            std::filesystem::path script_path = assets_dir / utf8_path(cfg.decision_load_path);
            if (!std::filesystem::is_regular_file(script_path))
                warnings.push_back("决策树脚本不存在：" + cfg.decision_load_path +
                    "（Dynamic_Entity 加载时会调用 load_file，路径错误将导致实体行为初始化失败）");
        }

        //acls 引用类型存在性检查
        for (const auto& acl : cfg.acls)
        {
            if (acl.empty())
                continue;
            bool found = false;
            for (const auto& other : 实体集合)
                if (other.type == acl)
                {
                    found = true;
                    break;
                }
            //若引用的类型尚未登记（或正指向自身），仅提示
            if (!found && acl != cfg.type)
                warnings.push_back("acls 引用的实体类型未登记：" + acl);
        }

        //acls 重复条目检查
        for (size_t i = 0; i < cfg.acls.size(); ++i)
            for (size_t j = i + 1; j < cfg.acls.size(); ++j)
                if (cfg.acls[i] == cfg.acls[j])
                {
                    warnings.push_back("acls 存在重复条目：" + cfg.acls[i]);
                    break;
                }

        //needed_events 重复检查
        for (size_t i = 0; i < cfg.needed_events.size(); ++i)
            for (size_t j = i + 1; j < cfg.needed_events.size(); ++j)
                if (cfg.needed_events[i] == cfg.needed_events[j])
                {
                    warnings.push_back("needed_events 存在重复条目：[" +
                        cfg.needed_events[i].first + "," + cfg.needed_events[i].second + "]");
                    break;
                }

        //返回是否存在致命错误
        return errors.empty();
    }

    //保存单个实体配置
    bool 实体配置仓库::保存实体(实体配置& cfg, std::string& error)
    {
        //type 不得为空
        if (cfg.type.empty())
        {
            error = "实体类型（type）不能为空";
            return false;
        }
        //重名检查：type 不得与「其他」实体重复（防止覆盖其他实体的配置文件）
        实体配置* other = 查找类型(cfg.type);
        if (other != nullptr && other != &cfg)
        {
            error = "实体类型已存在，无法保存（会覆盖已有配置）：" + cfg.type;
            return false;
        }

        //确定目标路径：type 变更时迁移文件
        std::string target_path = 生成配置路径(cfg.type);
        std::filesystem::path absolute_target = assets_dir / utf8_path(target_path);

        //若原路径存在且与目标路径不同，迁移文件（重命名）
        if (!cfg.config_path.empty() && cfg.config_path != target_path)
        {
            std::filesystem::path absolute_old = assets_dir / utf8_path(cfg.config_path);
            if (std::filesystem::exists(absolute_old))
            {
                std::error_code ec;
                std::filesystem::rename(absolute_old, absolute_target, ec);
                if (ec)
                {
                    error = "实体文件迁移失败：" + cfg.config_path + " → " + target_path;
                    return false;
                }
            }
        }
        //若为全新实体（无原路径），直接使用目标路径
        cfg.config_path = target_path;

        //序列化并原子写入实体 JSON（临时文件 + 流状态校验 + .bak 备份 + rename）
        nlohmann::json data = 序列化实体配置(cfg);
        std::string write_error;
        if (!原子写入文件(absolute_target, data.dump(2), write_error))
        {
            error = "实体文件写入失败：" + cfg.config_path + "（" + write_error + "）";
            return false;
        }

        //同步路由表（条目存在则更新 config_path，不存在则追加）
        if (route_json.is_array())
        {
            bool found = false;
            for (auto& route_data : route_json)
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
                route_json.push_back({ {"module", "Entity_Manager"}, {"config_path", cfg.config_path} });
        }
        if (!写入路由())
        {
            error = "路由表写入失败：" + path_utf8(route_path);
            return false;
        }

        return true;
    }

    //新建实体配置
    bool 实体配置仓库::新建实体(实体配置& cfg, std::string& error)
    {
        //type 不得为空
        if (cfg.type.empty())
        {
            error = "实体类型（type）不能为空";
            return false;
        }
        //type 不得重复
        if (查找类型(cfg.type) != nullptr)
        {
            error = "实体类型已存在：" + cfg.type;
            return false;
        }

        //生成配置路径并写入文件（原子写入：临时文件 + 流状态校验 + .bak 备份 + rename）
        cfg.config_path = 生成配置路径(cfg.type);
        nlohmann::json data = 序列化实体配置(cfg);
        std::filesystem::path absolute_path = assets_dir / utf8_path(cfg.config_path);
        std::string write_error;
        if (!原子写入文件(absolute_path, data.dump(2), write_error))
        {
            error = "实体文件写入失败：" + cfg.config_path + "（" + write_error + "）";
            return false;
        }

        //追加路由条目
        if (route_json.is_array())
            route_json.push_back({ {"module", "Entity_Manager"}, {"config_path", cfg.config_path} });
        if (!写入路由())
        {
            error = "路由表写入失败：" + path_utf8(route_path);
            return false;
        }

        //加入内存集合
        实体集合.push_back(cfg);
        return true;
    }

    //删除实体配置（仅移除路由条目，实体文件保留避免误删）
    bool 实体配置仓库::删除实体(const std::string& type, std::string& error)
    {
        //查找目标实体
        实体配置* target = 查找类型(type);
        if (target == nullptr)
        {
            error = "实体类型不存在：" + type;
            return false;
        }
        std::string config_path = target->config_path;

        //从路由表移除条目
        if (route_json.is_array())
        {
            route_json.erase(
                std::remove_if(route_json.begin(), route_json.end(),
                    [&config_path](const nlohmann::json& item)
                    {
                        return item.is_object() &&
                            item.value("config_path", std::string()) == config_path;
                    }),
                route_json.end());
        }
        if (!写入路由())
        {
            error = "路由表写入失败：" + path_utf8(route_path);
            return false;
        }

        //从内存集合移除
        实体集合.erase(
            std::remove_if(实体集合.begin(), 实体集合.end(),
                [&type](const 实体配置& cfg) { return cfg.type == type; }),
            实体集合.end());

        return true;
    }

    //========================================================================
    // 私有实现
    //========================================================================

    //读取路由表
    bool 实体配置仓库::读取路由()
    {
        if (!std::filesystem::is_regular_file(route_path))
            return false;

        try
        {
            std::ifstream file(route_path);
            if (!file.is_open())
                return false;
            file >> route_json;
        }
        catch (const std::exception&)
        {
            route_json = nlohmann::json();
            return false;
        }

        //路由表必须是 JSON 数组
        if (!route_json.is_array())
        {
            route_json = nlohmann::json();
            return false;
        }

        return true;
    }

    //写入路由表（原子写入：临时文件 + 流状态校验 + .bak 备份 + rename）
    bool 实体配置仓库::写入路由()
    {
        std::string write_error;
        return 原子写入文件(route_path, route_json.dump(2), write_error);
    }

    //解析单个实体 JSON（兼容旧格式）
    bool 实体配置仓库::解析实体配置(const nlohmann::json& data, 实体配置& out, std::string& error)
    {
        if (!data.is_object())
        {
            error = "配置内容非 JSON 对象";
            return false;
        }

        // —— type ——
        if (data.contains("type") && data["type"].is_string())
            out.type = data["type"];
        else
            out.type.clear();

        // —— decision_load_path ——（新格式字段；旧配置缺失时保持为空，由校验提示补填）
        if (data.contains("decision_load_path") && data["decision_load_path"].is_string())
            out.decision_load_path = data["decision_load_path"];
        else
            out.decision_load_path.clear();

        // —— acls ——
        out.acls.clear();
        if (data.contains("acls"))
        {
            const auto& acls_field = data["acls"];
            //新格式：字符串数组
            if (acls_field.is_array())
            {
                for (const auto& item : acls_field)
                    if (item.is_string())
                        out.acls.push_back(item.get<std::string>());
            }
            //旧格式：对象 { master, minion_set }
            else if (acls_field.is_object())
            {
                if (acls_field.contains("minion_set") && acls_field["minion_set"].is_array())
                {
                    for (const auto& item : acls_field["minion_set"])
                        if (item.is_string())
                            out.acls.push_back(item.get<std::string>());
                }
                //若对象仅含 master（字符串），将 master 作为唯一从属
                if (out.acls.empty() && acls_field.contains("master") && acls_field["master"].is_string())
                    out.acls.push_back(acls_field["master"].get<std::string>());
            }
        }

        // —— needed_events ——
        out.needed_events.clear();
        if (data.contains("needed_events") && data["needed_events"].is_array())
        {
            for (const auto& item : data["needed_events"])
            {
                //新格式：两元素数组 ["分类","标签"]
                if (item.is_array() && item.size() >= 2 &&
                    item[0].is_string() && item[1].is_string())
                {
                    out.needed_events.emplace_back(item[0].get<std::string>(),
                        item[1].get<std::string>());
                }
                //旧格式：对象 { category, tag }
                else if (item.is_object() &&
                    item.contains("category") && item["category"].is_string() &&
                    item.contains("tag") && item["tag"].is_string())
                {
                    out.needed_events.emplace_back(item["category"].get<std::string>(),
                        item["tag"].get<std::string>());
                }
            }
        }

        return true;
    }

    //序列化实体配置为标准 JSON
    nlohmann::json 实体配置仓库::序列化实体配置(const 实体配置& cfg) const
    {
        nlohmann::json data;
        data["type"] = cfg.type;
        data["decision_load_path"] = cfg.decision_load_path;

        //acls：字符串数组
        data["acls"] = nlohmann::json::array();
        for (const auto& acl : cfg.acls)
            data["acls"].push_back(acl);

        //needed_events：两元素数组的数组（引擎期望 vector<pair<string,string>>）
        data["needed_events"] = nlohmann::json::array();
        for (const auto& evt : cfg.needed_events)
            data["needed_events"].push_back({ evt.first, evt.second });

        return data;
    }

    //按类型生成默认配置路径
    std::string 实体配置仓库::生成配置路径(const std::string& type) const
    {
        return "config/entities/" + 文件名清理(type) + ".json";
    }

    //字符串清理（剔除路径非法字符）
    std::string 实体配置仓库::文件名清理(const std::string& name)
    {
        std::string cleaned = name;
        for (auto& ch : cleaned)
        {
            switch (ch)
            {
            case '\\': case '/': case ':': case '*':
            case '?': case '"': case '<': case '>':
            case '|':
                ch = '_';
                break;
            default:
                break;
            }
        }
        //去除首尾空白
        while (!cleaned.empty() && (cleaned.front() == ' ' || cleaned.front() == '\t'))
            cleaned.erase(cleaned.begin());
        while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '\t'))
            cleaned.pop_back();
        if (cleaned.empty())
            cleaned = "未命名实体";
        return cleaned;
    }
}
