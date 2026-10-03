#pragma once
//预编译头
#include "common/前置头文件包含.h"

namespace engine
{
    //节点缓存（调用处文件指针 → 节点指针；节点地址稳定，故缓存长期有效）
    template<typename Node>
    class Node_Cache
    {
    public:
        //缓存查找（未命中返回空指针）
        Node* cache_fetch(const char* file_name)
        {
            //持缓存锁查找
            std::lock_guard<std::mutex> lock(cache_mtx);
            //在缓存表中查找调用处文件指针
            typename std::unordered_map<const char*, Node*>::iterator it = cache_map.find(file_name);
            //未命中则返回空指针
            if (it == cache_map.end())
                return nullptr;
            //返回命中的节点指针
            return it->second;
        }
        //缓存登记（把调用处文件指针映射到节点指针）
        void cache_store(const char* file_name, Node* node)
        {
            //持缓存锁登记
            std::lock_guard<std::mutex> lock(cache_mtx);
            //登记映射
            cache_map[file_name] = node;
        }
    private:
        //缓存表（调用处文件指针 → 节点指针）
        std::unordered_map<const char*, Node*> cache_map{};
        //缓存互斥锁
        std::mutex cache_mtx{};
    };
}