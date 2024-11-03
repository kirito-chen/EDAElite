#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <algorithm>
#include <random>

#include "global_placement_sa.h"

// // 定义PLB和SEQ的容量限制
// constexpr int MAX_LUT_CAPACITY = 8;  // PLB内LUT组的最大容量
// constexpr int MAX_SEQ_CAPACITY = 16; // SEQ组的最大容量

// 全局随机数生成器
std::mt19937 &get_random_engine()
{
    static std::mt19937 gen;
    return gen;
}

// 设置随机种子
void set_random_seed(unsigned int seed)
{
    get_random_engine().seed(seed);
}

// 生成一个[min, max]区间的随机整数
int generate_random_int(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(get_random_engine());
}

// 生成[min, max]范围内的随机浮点数
double generate_random_double(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(get_random_engine());
}

// 计算PLB和SEQ组的总线长
int calculateWirelength() {
    int totalWirelength = 0;
    // 统计每个net线长
    for (auto iter : glbNetMap)
    {
        Net *net = iter.second;
        net->setNetHPWL(false); // 计算并设置net的半周线长
        totalWirelength += net->getNonCritWireLength(false);
        totalWirelength += net->getCritWireLength(false);

    }
    return totalWirelength;
}

// 尝试合并小容量的PLB组到同一位置
bool tryMergeSmallPLBs(PLBPlacement *selectedGroup, const std::tuple<int, int> &targetLocation)
{
    int availableSlots = MAX_LUT_CAPACITY - selectedGroup->getTotalLUTCount();

    // 遍历plbPlacementMap，找到可以合并的PLB组
    for (auto &[plbID, plb] : plbPlacementMap)
    {
        if (plb.getLocation() == targetLocation && &plb != selectedGroup)
        {
            int currentSlots = plb.getTotalLUTCount();
            if (currentSlots <= availableSlots)
            {
                int z_offset = plb.getTotalLUTCount(); // 设置新的Z编号偏移
                for (auto &lutGroup : selectedGroup->getLUTGroups())
                {
                    for (auto &lut : lutGroup)
                    {
                        std::tuple<int, int, int> temp_targetLocation = std::make_tuple(std::get<0>(targetLocation), std::get<1>(targetLocation), z_offset++);
                        lut->setLocation(temp_targetLocation);
                    }
                    plb.addLUTGroup(lutGroup);
                }
                // plb.addLUTGroup(selectedGroup->getLUTGroups());
                return true;
            }
        }
    }
    return false;
}

// 找到合适的放置位置
std::tuple<int, int> findSuitableLoc(PLBPlacement *selectedGroup)
{
    int x = generate_random_int(0, 149);
    int y = generate_random_int(0, 299);
    auto targetLocation = std::make_tuple(x, y);

    // 检查是否可以合并
    if (selectedGroup->getTotalLUTCount() < MAX_LUT_CAPACITY)
    {
        if (tryMergeSmallPLBs(selectedGroup, targetLocation))
        {
            return targetLocation;
        }
    }
    return targetLocation;
}

// 模拟退火算法
void global_placement_sa(bool isBaseline)
{
    auto start = std::chrono::high_resolution_clock::now();
    int cost = calculateWirelength();
    int costNew = 0;
    float T = 100.0;
    float alpha = 0.9;
    float threshold = 1e-3;

    // initializePLBGroupLocations(plbPlacementMap);
    // initializeSEQGroupLocations(seqPlacementMap);
    updateInstancesToTiles();
    // 模拟退火主循环
    while (T > threshold)
    {
        for (int iter = 0; iter < 100; ++iter)
        {
            // 随机选择一个PLB或SEQ组
            PLBPlacement *selectedGroup = selectGroup(plbPlacementMap);
            auto originLocation = selectedGroup->getLocation();
            auto newLocation = findSuitableLoc(selectedGroup);

            selectedGroup->setPLBLocation(newLocation);
            costNew = calculateWirelength();

            int deltaCost = costNew - cost;
            if (deltaCost < 0 || exp(-deltaCost / T) > generate_random_double(0.0, 1.0))
            {
                cost = costNew;
            }
            else
            {
                selectedGroup->setPLBLocation(originLocation); // 复原
            }
        }
        T *= alpha; // 降温
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "运行时间: " << duration.count() << " 秒" << std::endl;
}

PLBPlacement *selectGroup(std::unordered_map<int, PLBPlacement> &plbPlacementMap)
{
    std::vector<int> ids; // 存放所有未固定的组ID
    for (const auto &entry : plbPlacementMap)
    {
        // 如果组未固定，则将其ID添加到集合中
        if (!entry.second.getFixed())
        {
            ids.push_back(entry.first);
        }
    }

    // 若没有未固定的组，返回空指针
    if (ids.empty())
    {
        return nullptr;
    }

    // 随机选择一个未固定的组ID
    int randomIndex = generate_random_int(0, ids.size() - 1);
    int selectedGroupID = ids[randomIndex];

    // 返回所选组的指针
    return &plbPlacementMap[selectedGroupID];
}