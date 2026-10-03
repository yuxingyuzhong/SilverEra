#pragma once
//============================================================================
// 前置头文件包含（EngineSystem 预编译头）
//============================================================================
// 位置：Engine/EngineSystem/common/前置头文件包含.h
//
// 为什么 EngineSystem 需要自己这一份
//     EngineSystem 源码统一写 #include "Engine/EngineSystem/common/前置头文件包含.h"。
//     项目内部一律用「项目根相对全路径」，路径归属明确、不会与其它层的同名头互撞。
//
//     EngineSystem 需要的不只是 EngineCore 那些东西，还要有 Sol2（sol::）——Sol2 位于
//     EngineSystem/external/Sol2。本层 common/external/Sol2/ 下的类型别名与类型注册
//     头文件只 include 了预编译头就直接使用 sol::table / sol::state，于是
//     「谁提供 sol」这件事必须由本层这份预编译头兜住。
//
//     做法：先整体吃下 EngineCore 那份（保持单一真源，避免两份清单各自漂移），
//     再补上本层额外需要的外部库。
//============================================================================

// 引擎层的通用前置包含（C/C++ 标准库、nlohmann json、GLFW、windows）
#include "Engine/EngineCore/common/前置头文件包含.h"

//============================================================================
// 本层额外需要的
//============================================================================
// Sol2 —— Lua 绑定库（纯头文件）。本层 common/external/Sol2/ 下的
// sol类型别名.h / sol类型注册.h 会直接使用 sol::table / sol::function /
// sol::state 等类型，所以这里必须有它。
// 对应的包含路径（external/Sol2/include、external/Lua）由
// cmake/对外接口.cmake 声明，SystemCore 与上层用的是同一份声明。
#include <sol/sol.hpp>
