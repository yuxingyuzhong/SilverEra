#include "gui/Config_Editor/实体配置模型_内部工具.h"

#include <cstdio>
#include <set>

//引擎命名空间
namespace engine
{

    //========================================================================
    // 配置格式系统实现
    //========================================================================

    //获取全部配置格式
    const std::vector<配置格式>& 实体配置仓库::获取格式全部() const
    {
        return 格式集合;
    }

    //获取全部配置格式（可修改）
    std::vector<配置格式>& 实体配置仓库::获取格式全部()
    {
        return 格式集合;
    }

    //按模块名查找配置格式
    配置格式* 实体配置仓库::查找格式(const std::string& module)
    {
        for (auto& fmt : 格式集合)
            if (fmt.模块名 == module)
                return &fmt;
        return nullptr;
    }

    //字段类型 → 显示名
    const char* 实体配置仓库::字段类型名称(配置字段类型 type)
    {
        switch (type)
        {
        case 配置字段类型::文本:       return "文本";
        case 配置字段类型::脚本路径:   return "脚本路径";
        case 配置字段类型::文本列表:   return "文本列表";
        case 配置字段类型::事件对列表: return "事件对列表";
        case 配置字段类型::整数:       return "整数";
        case 配置字段类型::浮点数:     return "浮点数";
        case 配置字段类型::布尔:       return "布尔";
        default:                       return "文本";
        }
    }

    //字段类型 → JSON 序列化键名
    const char* 实体配置仓库::字段类型键(配置字段类型 type)
    {
        switch (type)
        {
        case 配置字段类型::文本:       return "string";
        case 配置字段类型::脚本路径:   return "script";
        case 配置字段类型::文本列表:   return "string_list";
        case 配置字段类型::事件对列表: return "pair_list";
        case 配置字段类型::整数:       return "int";
        case 配置字段类型::浮点数:     return "float";
        case 配置字段类型::布尔:       return "bool";
        default:                       return "string";
        }
    }

    //按 JSON 键名解析字段类型
    配置字段类型 实体配置仓库::解析字段类型(const std::string& key)
    {
        if (key == "script")       return 配置字段类型::脚本路径;
        if (key == "string_list")  return 配置字段类型::文本列表;
        if (key == "pair_list")    return 配置字段类型::事件对列表;
        if (key == "int")          return 配置字段类型::整数;
        if (key == "float")        return 配置字段类型::浮点数;
        if (key == "bool")         return 配置字段类型::布尔;
        return 配置字段类型::文本;
    }

    //判断磁盘格式与引擎内置种子是否一致（契约键：模块名/目录/路由/字段集合）
    //字段以「字段名 + 类型 + 必填」为契约；显示名/说明是展示文案，不影响引擎加载，不纳入比较
    static bool 内置格式一致(const 配置格式& a, const 配置格式& b)
    {
        if (a.模块名 != b.模块名) return false;
        if (a.配置目录 != b.配置目录) return false;
        if (a.路由文件名 != b.路由文件名) return false;
        if (a.字段.size() != b.字段.size()) return false;
        for (size_t i = 0; i < a.字段.size(); ++i)
        {
            if (a.字段[i].字段名 != b.字段[i].字段名) return false;
            if (a.字段[i].类型 != b.字段[i].类型) return false;
            if (a.字段[i].必填 != b.字段[i].必填) return false;
        }
        return true;
    }

    //确保内置格式种子存在（Entity_Manager / Property_Manager）
    //引擎契约演进时自动升级已存在的内置格式文件（原子写入会保留 .bak 兜底）
    void 实体配置仓库::确保内置格式()
    {
        //内置格式定义（与引擎 Entity_Manager / Property_Manager 期望的字段一致）
        const 配置格式 内置格式[] =
        {
            {
                "Entity_Manager",
                "entities",
                "entity.json",
                true,   //内置
                {
                    { "type",              "type（实体类型）",          配置字段类型::文本,     true,  "实体类型标识，保存后配置文件会自动迁移" },
                    { "decision_load_path", "decision_load_path（决策树行为脚本）", 配置字段类型::脚本路径, true,  "决策树行为脚本路径，相对 assets/ 目录，Entity_Manager 使用" },
                    { "acls",              "acls（从属权限列表）",      配置字段类型::文本列表, true,  "允许从属的实体类型列表（master 为实体类型自身）" },
                    { "needed_events",     "needed_events（订阅事件列表）", 配置字段类型::事件对列表, true,  "订阅事件列表，每项 = [分类, 标签]，如 [Entity, Request]" },
                }
            },
            {
                "Property_Manager",
                "property",
                "property.json",
                true,   //内置
                {
                    { "type",            "type（实体类型）",      配置字段类型::文本,     true,  "实体类型标识，与实体配置的 type 一致" },
                    { "initialize_path", "initialize_path（属性槽初始化脚本）", 配置字段类型::脚本路径, true,  "属性槽初始化 Lua 脚本路径，相对 assets/ 目录，Property_Manager 使用" },
                }
            },
        };

        //逐个检查：格式文件缺失则写入种子；已存在但与引擎契约不一致则自动升级
        for (const auto& fmt : 内置格式)
        {
            std::filesystem::path file_path = format_dir / (文件名清理(fmt.模块名) + ".json");
            if (!std::filesystem::is_regular_file(file_path))
            {
                写入格式文件(fmt);
                continue;
            }

            //格式演进一致性：内置格式以引擎契约为准，磁盘版本落后时自动升级
            配置格式 existing;
            if (!读取格式文件(file_path, existing) || !内置格式一致(existing, fmt))
            {
                std::printf("[ConfigEditor] 内置格式「%s」与引擎契约不一致，已自动升级为最新版本。\n",
                    fmt.模块名.c_str());
                写入格式文件(fmt);
            }
        }
    }

    //读取单个格式定义文件
    bool 实体配置仓库::读取格式文件(const std::filesystem::path& path, 配置格式& out)
    {
        try
        {
            nlohmann::json data;
            std::ifstream file(path);
            if (!file.is_open())
                return false;
            file >> data;

            if (!data.is_object())
                return false;
            out = 配置格式();
            out.模块名 = data.value("module", std::string());
            out.配置目录 = data.value("dir", std::string());
            out.路由文件名 = data.value("route", std::string());
            out.内置 = data.value("builtin", false);

            //解析字段列表
            if (data.contains("fields") && data["fields"].is_array())
            {
                for (const auto& f : data["fields"])
                {
                    if (!f.is_object())
                        continue;
                    配置字段定义 field;
                    field.字段名 = f.value("name", std::string());
                    field.显示名 = f.value("display", std::string());
                    field.类型 = 解析字段类型(f.value("type", std::string("string")));
                    field.必填 = f.value("required", true);
                    field.说明 = f.value("desc", std::string());
                    if (!field.字段名.empty())
                        out.字段.push_back(std::move(field));
                }
            }
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    //写入单个格式定义文件（原子写入）
    bool 实体配置仓库::写入格式文件(const 配置格式& fmt)
    {
        nlohmann::json data;
        data["module"] = fmt.模块名;
        data["dir"] = fmt.配置目录;
        data["route"] = fmt.路由文件名;
        data["builtin"] = fmt.内置;
        data["fields"] = nlohmann::json::array();
        for (const auto& f : fmt.字段)
        {
            nlohmann::json field;
            field["name"] = f.字段名;
            field["display"] = f.显示名;
            field["type"] = 字段类型键(f.类型);
            field["required"] = f.必填;
            field["desc"] = f.说明;
            data["fields"].push_back(std::move(field));
        }

        std::filesystem::path file_path = format_dir / (文件名清理(fmt.模块名) + ".json");
        std::string write_error;
        return 原子写入文件(file_path, data.dump(2), write_error);
    }

    //加载全部格式定义
    bool 实体配置仓库::加载格式()
    {
        //首次运行：确保内置格式种子文件存在
        确保内置格式();

        //读取 format/ 目录下全部格式文件（按文件名排序，保证顺序稳定）
        std::vector<std::filesystem::path> files;
        std::error_code ec;
        if (std::filesystem::is_directory(format_dir))
        {
            for (auto it = std::filesystem::directory_iterator(format_dir, ec);
                it != std::filesystem::directory_iterator(); it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }
                if (it->is_regular_file(ec) && it->path().extension() == ".json")
                    files.push_back(it->path());
            }
        }
        std::sort(files.begin(), files.end());

        for (const auto& path : files)
        {
            配置格式 fmt;
            if (读取格式文件(path, fmt) && !fmt.模块名.empty())
                格式集合.push_back(std::move(fmt));
        }

        return !格式集合.empty();
    }

    //保存格式定义
    bool 实体配置仓库::保存格式(const 配置格式& fmt, std::string& error)
    {
        //内置格式拒绝覆盖（防止误改引擎要求的格式）
        if (fmt.内置)
        {
            error = "内置格式（" + fmt.模块名 + "）不可编辑保存";
            return false;
        }
        if (fmt.模块名.empty())
        {
            error = "模块名不能为空";
            return false;
        }
        if (fmt.配置目录.empty() || fmt.路由文件名.empty())
        {
            error = "配置目录与路由文件名不能为空";
            return false;
        }

        //写入格式文件
        if (!写入格式文件(fmt))
        {
            error = "格式文件写入失败：" + fmt.模块名;
            return false;
        }

        //更新内存集合：已存在则替换，否则追加
        配置格式* existing = 查找格式(fmt.模块名);
        if (existing != nullptr)
            *existing = fmt;
        else
            格式集合.push_back(fmt);

        return true;
    }

    //删除格式定义（同时清理该模块的孤儿路由文件与配置目录）
    bool 实体配置仓库::删除格式(const std::string& module, std::string& error)
    {
        配置格式* target = 查找格式(module);
        if (target == nullptr)
        {
            error = "格式不存在：" + module;
            return false;
        }
        if (target->内置)
        {
            error = "内置格式不可删除：" + module;
            return false;
        }

        std::error_code ec;

        //删除格式文件
        std::filesystem::remove(format_dir / (文件名清理(module) + ".json"), ec);

        //清理孤儿路由文件（route/custom_<模块>.json，格式都没了路由就是死的）
        std::filesystem::path route_file = assets_dir / "config" / "route" /
            ("custom_" + 文件名清理(module) + ".json");
        std::filesystem::remove(route_file, ec);

        //清理该模块的配置目录（config/custom/<模块>/，避免遗留无法编辑的孤儿配置）
        std::filesystem::path config_dir = assets_dir / "config" / "custom" / 文件名清理(module);
        if (std::filesystem::is_directory(config_dir))
            std::filesystem::remove_all(config_dir, ec);

        //从内存集合移除
        格式集合.erase(
            std::remove_if(格式集合.begin(), 格式集合.end(),
                [&module](const 配置格式& fmt) { return fmt.模块名 == module; }),
            格式集合.end());

        //同时移除该模块的通用配置与路由缓存
        通用配置集合.erase(
            std::remove_if(通用配置集合.begin(), 通用配置集合.end(),
                [&module](const 通用配置& cfg) { return cfg.模块名 == module; }),
            通用配置集合.end());
        自定义路由表.erase(module);

        return true;
    }

    //扫描并删除未被任何路由表引用的孤儿配置文件
    int 实体配置仓库::清理孤儿配置(std::vector<std::string>& 删除列表, std::string& error)
    {
        删除列表.clear();
        try
        {
            //1. 收集所有路由表引用的 config_path（实体 / 属性槽 / 自定义模块）
            std::set<std::string> 被引用;
            if (route_json.is_array())
            {
                for (const auto& item : route_json)
                    if (item.is_object() && item.contains("config_path") && item["config_path"].is_string())
                        被引用.insert(item["config_path"].get<std::string>());
            }
            if (property_route_json.is_array())
            {
                for (const auto& item : property_route_json)
                    if (item.is_object() && item.contains("config_path") && item["config_path"].is_string())
                        被引用.insert(item["config_path"].get<std::string>());
            }
            for (const auto& kv : 自定义路由表)
            {
                if (!kv.second.is_array())
                    continue;
                for (const auto& item : kv.second)
                    if (item.is_object() && item.contains("config_path") && item["config_path"].is_string())
                        被引用.insert(item["config_path"].get<std::string>());
            }

            //2. 扫描配置目录下所有 .json（entities / property / custom），未被引用的删除；
            //   顺带清理原子写入残留的 .bak / .tmp（格式目录只清理残留，不动格式文件）
            const std::filesystem::path 候选目录[] =
            {
                assets_dir / "config" / "entities",
                assets_dir / "config" / "property",
                assets_dir / "config" / "custom",
                format_dir,
            };
            std::error_code ec;
            int 删除数 = 0;
            for (const auto& dir : 候选目录)
            {
                if (!std::filesystem::is_directory(dir, ec))
                    continue;
                for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
                    it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    if (!it->is_regular_file(ec))
                        continue;

                    const std::string ext = it->path().extension().string();
                    std::string relative = path_utf8(it->path().lexically_relative(assets_dir));
                    std::replace(relative.begin(), relative.end(), '\\', '/');

                    //备份/临时残留：直接删除（不属于任何路由配置）
                    if (ext == ".bak" || ext == ".tmp")
                    {
                        std::filesystem::remove(it->path(), ec);
                        if (ec)
                        {
                            error = "删除失败：" + relative + "（" + ec.message() + "）";
                            continue;
                        }
                        删除列表.push_back(relative + "（备份残留）");
                        ++删除数;
                        continue;
                    }

                    //格式目录只清理残留，不按路由判定孤儿（格式文件由格式管理维护）
                    if (dir == format_dir || ext != ".json")
                        continue;

                    if (被引用.count(relative) > 0)
                        continue;

                    //未被任何路由引用：视为孤儿配置，删除
                    std::filesystem::remove(it->path(), ec);
                    if (ec)
                    {
                        error = "删除失败：" + relative + "（" + ec.message() + "）";
                        continue;
                    }
                    删除列表.push_back(relative);
                    ++删除数;
                }
            }
            return 删除数;
        }
        catch (const std::exception& e)
        {
            error = std::string("清理异常：") + e.what();
            return -1;
        }
    }
}
