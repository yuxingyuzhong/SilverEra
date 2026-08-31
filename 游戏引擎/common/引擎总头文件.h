#pragma once

//公共头文件
#include "前置头文件包含.h"

//空间查询模块
#include "src/core/space/quadtree/四叉树.h"
#include "src/core/space/quadtree_manager/四叉树管理器.h"

//实体模块
#include "src/core/entity/Entity/实体.h"
#include "src/core/entity/Prop_Distributor/属性槽分发器.h"
#include "src/core/entity/Entity_Manager/实体管理器.h"

//事件模块
#include "src/core/event/Event_Broker/事件中转器.h"

//工具模块
#include "src/tools/Non_GUI/Auxi_Algorithm/二分查找.h"
#include "src/tools/Non_GUI/Config_Loader/配置加载器.h"
#include "src/tools/Non_GUI/Engine_Env/引擎环境.h"
#include "src/tools/Non_GUI/Random/随机数生成器.h"

