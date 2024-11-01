#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"
#include "global.h"


#include <unordered_map>
#include <unordered_set>

void calculateTileRemain();
void FM();
void generateOutputFile(const std::string &filename);

// void initialPlacement();
void matchLUTPairs(std::map<int, Instance*>& glbInstMap);
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
bool updateInstancesToTiles();
void printPLBInformation();
std::vector<std::tuple<int, int>> getNeighborTiles(int x, int y, int maxCols, int maxRows);
bool compareOuterSets(const std::set<std::set<Instance*>>& a, const std::set<std::set<Instance*>>& b);
void sortPLBGrouptList(std::vector<std::set<std::set<Instance*>>>& nonFixedPLBGrouptList);

