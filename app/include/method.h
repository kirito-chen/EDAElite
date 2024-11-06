#pragma once

#include <iostream>
#include <map>
#include <regex>
#include "object.h"
#include "arch.h"

void calculateTileRemain();
void FM();
void generateOutputFile(bool isBaseline, const std::string &filename);
std::string getValue(const std::string& jsonContent, const std::string& key);
int setIsPLB(); //提前统计哪个tile是PLB，提高效率
void statistics(); //统计函数
int extractNumber(const std::string& filePath); //提取 casex.nodes 中的数字 比如 xxx/case1.nodes 提取1
bool findBigNetId(int pinNumLimit); //如果存在 引脚数 > pinNumLimit 的netId 的net则返回true，id存储在 glbBigNet 中