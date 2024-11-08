#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"

void calculateTileRemain();
void FM();
void generateOutputFile(bool isBaseline, const std::string &filename);
std::string getValue(const std::string& jsonContent, const std::string& key);
int setIsPLB(); 

bool findBigNetId(int pinNumLimit); //如果存在 引脚数 > pinNumLimit 的netId 的net则返回true，id存储在 glbBigNet 中