#include "global.h"

std::map<std::string, Lib*> glbLibMap;
std::map<int, Instance*> glbInstMap;
std::map<int, Net*> glbNetMap;
Arch chip;
RecSteinerMinTree rsmt;
std::string lineBreaker = "------------------------------------------";
int** isPLB; //1表示该tile是PLB 为了避免部分tile不存在的情况，getTile会段错误


std::set<int> glbBigNet; //存储引脚数过大的netid 
int glbBigNetPinNum = 0;  //bigNet的引脚和 默认为0

/****密度相关****/
std::multiset<double> glbTopK; //存储前百分之5的pinDensity值 setPinDensityMapAndTopValues
std::map<int, double> glbPinDensityMap; //存储tile的pinDensity  key: x*1000+y  value： ratio setPinDensityMapAndTopValues
int glbTopKNum;  //统计PLB的5%数量   setPinDensityMapAndTopValues
double glbInitTopSum; //初始时前5%的pin密度之和 setPinDensityMapAndTopValues
