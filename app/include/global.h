#pragma once

#include <map>
#include <string>
#include "object.h"
#include "lib.h"
#include "arch.h"
#include "rsmt.h"
#include <queue>

// global variables
extern std::map<std::string, Lib*> glbLibMap;
extern std::map<int, Instance*> glbInstMap;
extern std::map<int, Net*> glbNetMap;
extern Arch chip;
extern RecSteinerMinTree rsmt;
extern std::string lineBreaker;
extern int** isPLB;


extern std::set<int> glbBigNet; //存储引脚数过大的netid 
extern int glbBigNetPinNum; //bigNet的引脚和 默认为0

/****密度相关****/
extern std::multiset<double> glbTopK; //存储前百分之5的pinDensity值
extern std::map<int, double> glbPinDensityMap;
extern int glbTopKNum;  //统计PLB的5%数量 
extern double glbInitTopSum; //初始时前5%的pin密度之和

