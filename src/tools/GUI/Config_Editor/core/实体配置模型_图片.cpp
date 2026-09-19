//============================================================================
// 实体配置模型 —— 实体图片元数据（entity_image.json）
// 架构改革 阶段 6：档案详情页的图片上传/展示依赖独立的元数据文件
//   assets/config/entity_image.json：{ "实体类型": "UI/entity_images/xxx.png", ... }
// 相对路径以 assets/ 为基准；引擎契约四字段零侵入，实体 JSON 不写入图片字段。
// 元数据文件损坏时忽略该条映射，不影响实体配置加载。
//============================================================================
#include "src/tools/GUI/Config_Editor/实体配置模型_内部工具.h"

//引擎命名空间
namespace engine
{
    //获取实体图片相对路径（type → 相对 assets/ 的路径；未设置返回空串）
    std::string 实体配置仓库::获取实体图片(const std::string& type) const
    {
        auto it = 实体图片表.find(type);
        return it == 实体图片表.end() ? std::string() : it->second;
    }

    //设置实体图片相对路径（仅更新内存表；传空串表示清除该映射）
    void 实体配置仓库::设置实体图片(const std::string& type, const std::string& 相对路径)
    {
        if (相对路径.empty())
            实体图片表.erase(type);
        else
            实体图片表[type] = 相对路径;
    }

    //从磁盘读取实体图片元数据（assets/config/entity_image.json；文件缺失视为空表）
    bool 实体配置仓库::加载实体图片表(std::string& error)
    {
        实体图片表.clear();
        std::filesystem::path 路径 = assets_dir / "config" / "entity_image.json";
        if (!std::filesystem::is_regular_file(路径))
            return true;   //首次运行尚无元数据文件，视为空表

        nlohmann::json data;
        try
        {
            std::ifstream in(路径);
            if (!in.good())
            {
                error = "无法打开实体图片元数据文件：" + path_utf8(路径);
                return false;
            }
            in >> data;
        }
        catch (const std::exception& e)
        {
            error = "实体图片元数据解析失败（" + std::string(e.what()) + "）：" + path_utf8(路径);
            return false;
        }

        //顶层必须是 JSON 对象（type → 相对路径）
        if (!data.is_object())
        {
            error = "实体图片元数据格式错误（应为 JSON 对象）：" + path_utf8(路径);
            return false;
        }

        //逐条读取，损坏/非字符串条目忽略（计划：元数据损坏不影响实体配置加载）
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            if (it.value().is_string())
                实体图片表[it.key()] = it.value().get<std::string>();
        }
        return true;
    }

    //把实体图片元数据写入磁盘（原子写入；错误信息写入 error）
    bool 实体配置仓库::保存实体图片表(std::string& error) const
    {
        nlohmann::json data = nlohmann::json::object();
        for (const auto& [type, 相对路径] : 实体图片表)
            data[type] = 相对路径;

        std::filesystem::path 路径 = assets_dir / "config" / "entity_image.json";
        return 原子写入文件(路径, data.dump(4), error);
    }
}
