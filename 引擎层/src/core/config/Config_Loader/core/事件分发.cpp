#include "../局部命名空间使用.h"
#include "src/tools/Logging/日志系统运行包.h"

//引擎命名空间
namespace engine
{
    //事件处理
    void Config_Loader::event_process(std::shared_ptr<Event> evt)
    {
        //接收者答复（存在性检查的答复）
        if (evt->category == "Subscriber" && evt->tag == "Call")
        {
            //获取目标对象迭代器
            auto it = waited_objects.find(evt->sender_object);
            //若事件发送者为配置待发送对象
            if(it != waited_objects.end())
            {
                if (config_routes[evt->sender_object].empty())
                {
                    logger.warn("Config_Loader::配置请求对象不存在配置缓存\n");
                    logger.warn("Config_Loader::配置加载请求已驳回\n");
                }
                //逐份加载路由目录
                for (const auto& cotent : config_routes[evt->sender_object])
                    route_load(cotent,true,evt->sender_object);
                //清除该配置待发送对象信息
                waited_objects.erase(it);
            }
            return;
        }

        //可信根目录修改
        if (evt->category == "Belived_Root")
        {
            //若载荷可信根目录字段无效(包含非空检查)
            if (!detail::field_check<string>(evt->config, "route") ||
                !detail::field_check<string>(evt->config, "config"))
            {
                logger.error("Config_Loader::未定义可信根目录字段\n已驳回");
                return;
            }

            //取可信根索引目录与配置目录
            const Config_Content root(evt->config["route"].get<std::string>(),
                evt->config["config"].get<std::string>());
            //若可信根目录字段为空
            if (root.route.empty() || root.config.empty())
            {
                logger.error("Config_Loader::可信根目录字段为空\n已驳回");
                return;
            }

            //若为可信根目录加载(增量加载)
            if (evt->tag == "Load")
            {
                //若该可信根目录已登记
                if (believed_roots.count(root))
                {
                    logger.warn("Config_Loader::可信根目录已登记，已跳过添加: {}",
                        detail::path_to_string(root.config));
                    return;
                }
                //登记可信根目录
                believed_roots.insert(root);
                //扫描建立对象缓存
                route_load(root, true);
                return;
            }
            //若为可信根目录卸载
            else if (evt->tag == "Unload")
            {
                //若该可信根目录未登记
                if (!believed_roots.count(root))
                {
                    logger.warn("Config_Loader::可信根目录未登记，已略过卸载");
                    return;
                }
                //清除该可信根目录下的对象缓存
                root_cache_clear(root);
                //取消可信根目录登记
                believed_roots.erase(root);
                return;
            }
        }

        //配置路由加载
        if (evt->category == "Route" && evt->tag == "load")
        {
            //若配置路由加载对象未定义
            if (!detail::field_check<string>(evt->config, "object"))
            {
                logger.error("Config_Loader::未定义路由重加载对象\n已略过该事件处理");
                return;
            }
            //若载荷配置目录字段无效
            if (!detail::field_check<string>(evt->config, "route") ||
                !detail::field_check<string>(evt->config, "config"))
            {
                logger.error("Config_Loader::未定义配置目录字段\n已驳回");
                return;
            }

            //去索引目录加载对象
            const string object = evt->config["object"].get<std::string>();
            //取索引目录与配置目录
            const path route_dir = detail::string_to_path(evt->config["route"].get<std::string>());
            const path config_dir = detail::string_to_path(evt->config["config"].get<std::string>());
            //构造配置目录对象
            Config_Content content(route_dir, config_dir);

            //若目标对象不存在
            if (!config_routes.count(object))
            {
                logger.warn("Config_Loader::配置索引加载对象不存在\n已略过该事件处理");
                return;
            }
            //若未缓存目标配置目录
            else if (!config_routes[object].count(content))
            {
                logger.warn("Config_Loader::配置索引目录未缓存\n已略过该事件处理");
                return;
            }
            //执行配置路由加载
            route_load(Config_Content(route_dir, config_dir),true,object);
            return;
        }

        //对象配置加载
        if (evt->category == "Config" && evt->tag == "load")
        {
            //若载荷缺少对象字段
            if (!detail::field_check<string>(evt->config,"object"))
            {
                logger.error("Config_Loader::对象字段无效\n已略过该事件处理");
                return;
            }
            //取目标对象名称
            const std::string object = evt->config["object"];
            //指定文件清单（缺省表示该对象全部文件）
            vector<std::string> files{};
            //若事件指定加载文件清单
            if (detail::field_check<vector<string>>(evt->config, "files"))
            {
                //逐个收集有效文件路径
                for (const auto& file : evt->config["files"])
                {
                    //若文件非空
                    if (!file.empty())
                        files.push_back(file.get<std::string>());
                }
            }
            //若事件未指定加载文件清单
            else
                files = evt->config["files"];
            //执行对象配置加载（脏标记命中者告警并跳过）
            config_load(object, files);
            return;
        }
    }
}