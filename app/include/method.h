#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"

#include <unordered_map>
#include <unordered_set>

void calculateTileRemain();
void FM();
void generateOutputFile(const std::string &filename);

// void initialPlacement();
void matchLUTPairs(std::map<int, Instance*>& glbInstMap);
