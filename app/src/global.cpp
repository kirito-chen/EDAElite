#include "global.h"

std::map<std::string, Lib*> glbLibMap;
std::map<int, Instance*> glbInstMap;
std::map<int, Net*> glbNetMap;
Arch chip;
RecSteinerMinTree rsmt;
std::string lineBreaker = "------------------------------------------";
int** isPLB; //1表示该tile是PLB 为了避免部分tile不存在的情况，getTile会段错误

