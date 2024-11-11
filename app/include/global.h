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
extern int** isPLB;

// 全局变量：用于存储LUT组合
extern std::map<int, std::set<Instance*>> lutGroups; // 新的组合实例映射
extern std::map<int, std::set<std::set<Instance*>>> plbGroups;
// 全局 PLBPlacement 映射
extern std::unordered_map<int, PLBPlacement> plbPlacementMap;
// 全局SEQ映射
extern std::unordered_map<int, SEQBankPlacement> seqPlacementMap;





extern std::set<int> glbBigNet; //存储引脚数过大的netid 
extern int glbBigNetPinNum; //bigNet的引脚和 默认为0

/****密度相关****/
extern std::vector<std::pair<int,int> > glbPinDensity; //第一个是tile x*1000+y 第二个是密度的分子
extern int glbTopKNum;  //统计PLB的5%数量   setPinDensityMapAndTopValues
extern int glbInitTopSum; //记录初始top的分子之和


