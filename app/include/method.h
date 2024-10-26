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
void matchLUTGroupsToPLB(std::map<int, std::set<Instance*>> &lutGroups, std::map<int, std::set<std::set<Instance*>>> &plbGroups);
// void assignLUTSiteInPLB(std::map<int, std::set<Instance*>> &plbGroups);

void updatePLBLocations(std::map<int, std::set<std::set<Instance*>>> &plbGroups);