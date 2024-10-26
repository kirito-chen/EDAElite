#pragma once

#include <map>
#include <string>
#include "object.h"
#include "lib.h"
#include "arch.h"
#include "rsmt.h"
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <algorithm>
#include <mutex>
#include <thread>

// global variables
extern std::map<std::string, Lib*> glbLibMap;
extern std::map<int, Instance*> glbInstMap;
extern std::map<int, Net*> glbNetMap;
extern Arch chip;
extern RecSteinerMinTree rsmt;
extern std::string lineBreaker;

// 全局变量：用于存储LUT组合
extern std::map<int, std::set<Instance*>> lutGroups; // 新的组合实例映射
extern std::map<int, std::set<std::set<Instance*>>> plbGroups;