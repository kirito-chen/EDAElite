#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"
#include "global.h"
#include "legal.h"
#include "arbsa.h"
#include <unordered_map>
#include <unordered_set>

void calculateTileRemain();
void FM();
void generateOutputFile(bool isBaseline, const std::string &filename);
std::string getValue(const std::string& jsonContent, const std::string& key);
int setIsPLB(); 

// void initialPlacement();
void matchLUTPairs(std::map<int, Instance*>& glbInstMap, bool isLutPack = true, bool isSeqPack = true);
void matchLUTGroupsToPLB(std::map<int, std::set<Instance*>> &lutGroups, std::map<int, std::set<std::set<Instance*>>> &plbGroups);
// void assignLUTSiteInPLB(std::map<int, std::set<Instance*>> &plbGroups);

void updatePLBLocations(std::map<int, std::set<std::set<Instance*>>> &plbGroups);
// void buildGlobalPLBPlacement(const std::map<int, std::set<std::set<Instance *>>> &plbGroups);
void initializePLBPlacementMap(const std::map<int, std::set<std::set<Instance*>>> &plbGroups);
void initializeSEQPlacementMap(const std::map<int, Instance*>& glbInstMap);
void initializePLBGroupLocations(std::unordered_map<int, PLBPlacement>& plbPlacementMap);
void initializeSEQGroupLocations(std::unordered_map<int, SEQBankPlacement>& seqPlacementMap);
void updateLUTLocations(std::unordered_map<int, PLBPlacement>& plbPlacementMap);
void updateSEQLocations(std::unordered_map<int, SEQBankPlacement> &seqBankMap);
bool updateInstancesToTiles(bool isSeqPack);
void printPLBInformation();
std::vector<std::tuple<int, int>> getNeighborTiles(int x, int y, int rangeDesired);
bool compareOuterSets(const std::set<std::set<Instance*>>& a, const std::set<std::set<Instance*>>& b);
void sortPLBGrouptList(std::vector<std::set<std::set<Instance*>>>& nonFixedPLBGrouptList);
void matchFixedLUTGroupsToPLB(std::map<int, std::set<Instance *>> &lutGroups, std::map<int, std::set<std::set<Instance *>>> &plbGroups);
void printInstanceInformation();
// std::tuple<int, int> getNeighborTile(int x, int y);
int calculateTwoInstanceWireLength(Instance* inst1, Instance* inst2, bool isBaseLine);
std::tuple<int, int> getNeighborTile(int x, int y, bool isLeft = true);
void initialGlbPackInstMap();
void initialGlbPackNetMap();



