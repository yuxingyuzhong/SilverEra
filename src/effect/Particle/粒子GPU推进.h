#pragma once
//预编译头
#include "Engine/EngineSystem/common/前置头文件包含.h"
//渲染器门面（层内 GPU 粒子推进入口在渲染器公共头中声明）
#include "Engine/EngineSystem/src/core/Renderer/src/render/Renderer/渲染器.h"

/*
GPU 粒子推进（EngineSystem；2026-11-03 自渲染层下沉）
渲染层已作为本层子层（src/core/Renderer），故粒子常驻推进的**模拟语义**（发射循环、
环形写指针、活跃位图、死亡结算、紧凑索引）整体归本层，渲染层只提供计算原语
（compute_pipeline_load / compute_buffer_bind / compute_dispatch / compute_buffer_read 等）
与 GPU 缓冲。入口在 `engine::particle_gpu` 命名空间，声明见渲染器公共头（friend 授权访问私有实现）。
*/
