#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"
#include "method.h"

int arbsa(bool isBaseline,  std::string nodesFile); 

int arbsaMtx(bool isBaseline); //多线程版本
std::tuple<int, int, int> findSuitableLocForLutSet(bool isBaseline, int x, int y, int rangeDesired, Instance *inst);
bool isValidForLutorSeqSet(bool isBaseline, int x, int y, int &z, Instance *inst);

int changeTileForSet(bool isBaseline, std::tuple<int, int, int> originLoc, std::tuple<int, int, int> loc, Instance *inst);



