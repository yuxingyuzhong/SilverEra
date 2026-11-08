#pragma once
#include "碰撞代理器.h"
#include "../Common/碰撞事件.h"
#include "Engine/EngineFoundation/src/event/Common/事件协议.h"

// ---------- 基础类型 ----------
using std::string;

// ---------- 容器 ----------
using std::unordered_map;
using std::vector;
using std::optional;
using std::nullopt;

// ---------- 智能指针 ----------
using std::shared_ptr;
using std::unique_ptr;

// ---------- 内存分配 ----------
using std::nothrow;

// ---------- nlohmann::json ---------
using nlohmann::json;
