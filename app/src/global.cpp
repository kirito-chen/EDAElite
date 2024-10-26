#include "global.h"

std::map<std::string, Lib*> glbLibMap;
std::map<int, Instance*> glbInstMap;
std::map<int, Net*> glbNetMap;
Arch chip;
RecSteinerMinTree rsmt;
std::string lineBreaker = "------------------------------------------";

// 全局变量：用于存储LUT组合
std::map<int, std::set<Instance*>> lutGroups; // 新的组合实例映射
std::map<int, std::set<std::set<Instance*>>> plbGroups;
std::unordered_map<int, PLBPlacement> plbPlacementMap;