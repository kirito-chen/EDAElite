#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"

void calculateTileRemain();
void FM();
void generateOutputFile(bool isBaseline, const std::string &filename);
std::string getValue(const std::string& jsonContent, const std::string& key);