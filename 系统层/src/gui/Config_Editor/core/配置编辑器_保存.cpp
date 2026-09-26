//============================================================================
// 配置编辑器 —— 保存（保存当前选中/保存全部）
// 由 配置编辑器.cpp 拆分而来（架构改革 阶段 1），行为与拆分前完全一致
//============================================================================
#include "gui/Config_Editor/配置编辑器.h"
#include "gui/Config_Editor/配置编辑器_内部工具.h"

namespace engine
{
    //保存当前选中配置（Ctrl+S / 菜单共用；按 当前模块 分发，成功后清除脏标记）
    void 配置编辑器::保存当前选中()
    {
        if (当前模块 == "Entity_Manager")
        {
            if (选中索引 >= 0 && 选中索引 < (int)仓库.获取全部().size())
            {
                实体配置& cfg = 仓库.获取全部()[选中索引];
                std::string error;
                if (仓库.保存实体(cfg, error))
                {
                    状态消息 = "已保存：" + cfg.config_path;
                    有未保存修改 = false;
                }
                else
                    状态消息 = "保存失败：" + error;
            }
            else
                状态消息 = "请先选择一个实体配置";
        }
        else if (当前模块 == "Property_Manager")
        {
            if (选中索引 >= 0 && 选中索引 < (int)仓库.获取属性槽全部().size())
            {
                属性槽配置& prop = 仓库.获取属性槽全部()[选中索引];
                std::string error;
                if (仓库.保存属性槽(prop, error))
                {
                    状态消息 = "已保存属性槽配置：" + prop.config_path;
                    有未保存修改 = false;
                }
                else
                    状态消息 = "保存失败：" + error;
            }
            else
                状态消息 = "请先选择一个属性槽配置";
        }
        else
        {
            if (选中通用配置索引 >= 0 &&
                选中通用配置索引 < (int)仓库.获取通用配置全部().size())
            {
                通用配置& cfg = 仓库.获取通用配置全部()[选中通用配置索引];
                std::string error;
                if (仓库.保存通用配置(cfg, error))
                {
                    状态消息 = "已保存：" + cfg.config_path;
                    有未保存修改 = false;
                }
                else
                    状态消息 = "保存失败：" + error;
            }
            else
                状态消息 = "请先选择一个配置";
        }
    }

    //保存全部配置（实体 + 属性槽 + 通用配置；菜单「保存全部」与关闭确认共用）
    void 配置编辑器::保存全部()
    {
        int 成功数 = 0;
        int 失败数 = 0;
        std::string 首个失败信息;

        //实体配置
        for (auto& cfg : 仓库.获取全部())
        {
            std::string error;
            if (仓库.保存实体(cfg, error))
                ++成功数;
            else
            {
                ++失败数;
                if (首个失败信息.empty())
                    首个失败信息 = error;
            }
        }
        //属性槽配置
        for (auto& prop : 仓库.获取属性槽全部())
        {
            std::string error;
            if (仓库.保存属性槽(prop, error))
                ++成功数;
            else
            {
                ++失败数;
                if (首个失败信息.empty())
                    首个失败信息 = error;
            }
        }
        //通用配置
        for (auto& cfg : 仓库.获取通用配置全部())
        {
            std::string error;
            if (仓库.保存通用配置(cfg, error))
                ++成功数;
            else
            {
                ++失败数;
                if (首个失败信息.empty())
                    首个失败信息 = error;
            }
        }

        if (失败数 == 0)
        {
            状态消息 = "已保存全部 " + std::to_string(成功数) + " 个配置";
            有未保存修改 = false;
        }
        else
            状态消息 = "已保存 " + std::to_string(成功数) + " 个，失败 " +
                std::to_string(失败数) + " 个（" + 首个失败信息 + "）";
    }

}
