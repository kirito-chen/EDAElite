#pragma once

#include <map>
#include <string>
#include "object.h"
#include "lib.h"
#include "arch.h"
#include "rsmt.h"
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <algorithm>
#include <mutex>
#include <thread>

// global variables
extern std::map<std::string, Lib*> glbLibMap;
extern std::map<int, Instance*> glbInstMap;
extern std::map<int, Net*> glbNetMap;
extern Arch chip;
extern RecSteinerMinTree rsmt;
extern std::string lineBreaker;

