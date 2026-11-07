#pragma once
//测试层级状态 —— 按子 CMake 层级判定「测试库链接的下层版本」是否落后于当前可用版本
//----------------------------------------------------------------------------
// 位置：Application/Test/src/主调/测试层级状态.h
// 用途：图形界面的选择树按层级分组；某层级的下层库已有新版号而该层测试库尚未重建时，
//       把该层级单独隔离并提示用户先重建对应子工程（避免用旧库跑新头，结论失真）。
// 数据来源：
//   ① 各层构建期写出的 依赖版本.cmake（tests/<子工程>/out/build/x64-<配置>/）
//   ② 当前可用版本：Engine/版本锁.cmake 的锁值优先，锁空则读该层构建树的 版本锚点.h
//----------------------------------------------------------------------------

#include <map>
#include <string>
#include <vector>

namespace engine
{
    //测试层级（编号与选择模型的层级分组一致）
    enum class Test_Layer : int
    {
        Self = 0,        //本层自测（EngineTests 自身收编的用例）
        Foundation = 1,  //tests/Foundation
        System = 2,      //tests/System
        Renderer = 3,    //tests/Renderer
        Unknown = 4      //未识别来源（正常不应出现）
    };

    //参与分组判定的层级数量（不含 Unknown）
    constexpr int test_layer_count = 4;

    //层级显示名（界面分组标题）
    const char* test_layer_name(Test_Layer layer);

    //单个层级与下层版本的比对结果
    struct Layer_Version_State
    {
        //层级
        Test_Layer layer = Test_Layer::Unknown;
        //是否待更新（该层级链接的下层版本落后于当前可用版本，或缺少构建记录）
        bool outdated = false;
        //是否缺少依赖版本记录（该子工程尚未构建过）
        bool record_missing = false;
        //待更新原因（逐条，界面直接展示）
        std::vector<std::string> reasons;
    };

    //版本比对（纯逻辑，供单测）
    //recorded：该层级构建时记录的「层名 → 版本哈希」；current：当前可用的「层名 → 版本哈希」
    //返回是否不一致；不一致时把逐条原因写进 reasons
    bool version_mismatch(const std::map<std::string, std::string>& recorded,
        const std::map<std::string, std::string>& current,
        std::vector<std::string>& reasons);

    //读取并判定四个层级的状态（文件访问；缺记录按待更新处理）
    std::vector<Layer_Version_State> layer_version_states_get(void);

    //当前配置名（Debug / Release，与本可执行文件的构建配置一致）
    const char* current_config_name(void);

    //项目根目录（可执行文件目录上溯两级：Application/Test → Application → 项目根）
    std::string project_root_path(void);
}
