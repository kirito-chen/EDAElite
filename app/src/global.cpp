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
std::vector<std::pair<int,int> > glbPinDensity; //第一个是tile x*1000+y 第二个是密度的分子
int glbTopKNum;  //统计PLB的5%数量   setPinDensityMapAndTopValues
int glbInitTopSum; //记录初始top的分子之和
