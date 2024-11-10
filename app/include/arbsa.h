#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"
#include "method.h"

int arbsa(bool isBaseline);

int arbsaMtx(bool isBaseline); // 多线程版本
// std::tuple<int, int, int> findSuitableLocForLutSet(bool isBaseline, int x, int y, int rangeDesired, Instance *inst);
// bool isValidForLutorSeqSet(bool isBaseline, int x, int y, int &z, Instance *inst);

// int changeTileForSet(bool isBaseline, std::tuple<int, int, int> originLoc, std::tuple<int, int, int> loc, Instance *inst);
bool isValid(bool isBaseline, int x, int y, int &z, Instance *inst);

int newArbsa(bool isBaseline, bool isSeqPack);
bool isPackValid(bool isBaseline, int x, int y, int &z, Instance *inst, bool isSeqPack);
std::tuple<int, int, int> findPackSuitableLoc(bool isBaseline, int x, int y, int rangeDesired, Instance *inst, bool isSeqPack);
int changePackTile(bool isBaseline, std::tuple<int, int, int> originLoc, std::tuple<int, int, int> loc, Instance *inst, bool isSeqPack);
int calculPackRelatedFitness(std::vector<std::pair<int, float>> &fitnessVec, std::map<int, int> &rangeDesiredMap, std::map<int, int> &rangeActualMap, const std::set<int> &instRelatedNetId);
int calculPackrangeMap(bool isBaseline, std::map<int, int> &rangeActualMap);
int calculPackRelatedRangeMap(bool isBaseline, std::map<int, int> &rangeActualMap, const std::set<int> &instRelatedNetId);
