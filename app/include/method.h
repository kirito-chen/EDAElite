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
int setIsPLB(); 

bool findBigNetId(int pinNumLimit); //如果存在 引脚数 > pinNumLimit 的netId 的net则返回true，id存储在 glbBigNet 中

std::string extractFileName(const std::string& filePath); //提取 casex.nodes 中的数字 比如 xxx/case1.nodes 提取case1.nodes

// 解析嵌套结构：外层是字符串（"1"、"2"等），内层包含键值对（"60": 11, "1180": 100 等）
using NestedMap = std::map<std::string, std::map<std::string, int>>;
bool fileExists(const std::string& filePath);
NestedMap readJsonFile(const std::string& filename);
void writeJsonFile(const std::string& filename, const NestedMap& data);