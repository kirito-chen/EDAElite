#include "global.h"

std::map<std::string, Lib *> glbLibMap;
std::map<int, Instance *> glbInstMap;
std::map<int, Net *> glbNetMap;
Arch chip;
RecSteinerMinTree rsmt;
std::string lineBreaker = "------------------------------------------";
int **isPLB; // 1表示该tile是PLB 为了避免部分tile不存在的情况，getTile会段错误

// 全局变量：用于存储LUT组合
std::map<int, std::set<Instance *>> lutGroups; // 新的组合实例映射，两两LUT组匹配成的
std::map<int, std::set<std::set<Instance *>>> plbGroups;
std::unordered_map<int, PLBPlacement> plbPlacementMap;
std::unordered_map<int, SEQBankPlacement> seqPlacementMap;
std::map<int, HPLB *> globalHPLBMap;

std::set<int> glbBigNet;                  // 存储引脚数过大的netid
int glbBigNetPinNum = 0;                  // bigNet的引脚和 默认为0
std::map<int, Instance *> glbPackInstMap; // 存储打包后的全局InstMap
std::map<int, Net *> glbPackNetMap;       // 存储打包后的全局NetMap

/****密度相关****/
std::vector<std::pair<int, int>> glbPinDensity; // 第一个是tile x*1000+y 第二个是密度的分子
int glbTopKNum;                                 // 统计PLB的5%数量   setPinDensityMapAndTopValues
int glbInitTopSum;                              // 记录初始top的分子之和

std::map<int, int> oldNetID2newNetID; // 全局映射，存放旧netID到新netID的映射
std::vector<std::tuple<int, int>> neighbor_PLB_xy;