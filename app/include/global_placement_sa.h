#include <cmath>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <map>
#include <tuple>
#include <vector>
#include "object.h"  // 包含相关的定义和全局变量声明
#include "global.h"
#include "method.h"

void global_placement_sa(bool isBaseline);
PLBPlacement* selectGroup(std::unordered_map<int, PLBPlacement>& plbPlacementMap);