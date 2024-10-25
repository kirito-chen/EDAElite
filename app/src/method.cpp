#include <iomanip>
// #include "legal.h"
#include "global.h"
#include "object.h"
#include "method.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

#include <thread>
#include <mutex>



// 共享数据结构的锁
std::mutex lutMutex;
std::unordered_set<int> matchedLUTs; // 存储已匹配的 LUT IDs

// 检查实例是否为 LUT 类型
bool isLUTType(const std::string &modelName) {
    return modelName.rfind("LUT", 0) == 0; // 确认modelName以"LUT"开头
}

// 计算两个 unordered_set 的并集
std::unordered_set<int> unionSets(const std::unordered_set<int>& set1, const std::unordered_set<int>& set2) {
    std::unordered_set<int> result(set1); // 先将 set1 拷贝到结果集合中
    result.insert(set2.begin(), set2.end()); // 插入 set2 的所有元素，重复的元素会自动忽略
    return result;
}

// 计算PLB余量的函数——吴白轩——2024年10月18日
void calculateTileRemain()
{
    std::cout << "计算PLB类型的Tile中LUT与DFF的余量" << std::endl;
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {
            Tile *tile = chip.getTile(i, j);
            tile->getRemainingPLBResources(false);
        }
    }
}

int calculateGain(Instance *inst, Tile *newTile)
{
    int originalHPWL = inst->getAllRelatedNetHPWL(); // 获取移动前的HPWL

    // 临时将inst移动到newTile，计算新的HPWL
    auto originalLocation = inst->getLocation();
    inst->setLocation(std::make_tuple(newTile->getCol(), newTile->getRow(), 0));
    inst->updateInstRelatedNet(false);
    int newHPWL = inst->getAllRelatedNetHPWL();

    // 恢复原始位置
    inst->setLocation(originalLocation);

    // 计算增益：HPWL减少的量
    return originalHPWL - newHPWL;
}

bool moveInstance(Instance *inst, Tile *newTile)
{
    int instID = std::stoi(inst->getInstanceName().substr(5));           // 从第5个字符开始截取，转换为整数
    int offset = newTile->findOffset(inst->getModelName(), inst, false); // cjq modify 获取合并引脚数不超过6的最大引脚数的插槽位置
    if (offset == -1)
    {
        // std::cout<<"Unable to find any available space to place\n";
        return false;
    }
    // 可以进行移动
    Tile *oldTile = chip.getTile(std::get<0>(inst->getLocation()), std::get<1>(inst->getLocation()));
    if (oldTile != nullptr)
    {
        oldTile->removeInstance(inst); // 从旧Tile中移除inst
    }
    newTile->addInstance(instID, offset, inst->getModelName(), false);                // 将inst添加到新Tile
    inst->setLocation(std::make_tuple(newTile->getCol(), newTile->getRow(), offset)); // 更新inst的位置
    return true;
}

void FM()
{
    std::cout << "--------FM--------" << std::endl;
    bool isBaseline = false; // 可以根据需求选择是否计算 baseline 状态

    // 统计每个net线长
    for (auto iter : glbNetMap)
    {
        Net *net = iter.second;
        net->setNetHPWL(isBaseline); // 计算并设置net的半周线长
    }

    // 统计每个inst的所有相关net的线长之和以及平均线长
    for (auto &instPair : glbInstMap)
    {
        Instance *inst = instPair.second;
        inst->calculateAllRelatedNetHPWL(isBaseline); // 计算与该inst相关的net的总HPWL
        inst->generateMovableRegion();                // 生成可移动区域
    }

    // 根据每个实例的HPWL平均值进行排序
    std::vector<std::pair<int, int>> instHPWL; // {instID, HPWLaver}
    for (auto &instPair : glbInstMap)
    {
        int instanceID = instPair.first;
        Instance *inst = instPair.second;
        int HPWLAver = inst->getAllRelatedNetHPWLAver(); // 获取HPWL平均值
        instHPWL.emplace_back(instanceID, HPWLAver);     // 将instID与HPWLAver存入
    }
    int allInstNum = instHPWL.size();
    std::cout << "instance总数: " << allInstNum << std::endl;
    // int count = 0;

    // // 对实例按HPWL降序排序
    // std::sort(instHPWL.begin(), instHPWL.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b)
    //           {
    //               return a.second > b.second; // 降序
    //           });

    // 遍历排序后的实例，尝试移动
    for (const auto &instPair : instHPWL)
    {
        Instance *inst = glbInstMap[instPair.first]; // 获取实例
        // 获取实例可移动区域的四个角和中心点
        int x_min = inst->getMovableRegion()[0];
        int y_min = inst->getMovableRegion()[1];
        int x_max = inst->getMovableRegion()[2];
        int y_max = inst->getMovableRegion()[3];

        if (inst->isFixed() || inst->getAllRelatedNetHPWL() == 0 || ((x_min == x_max) && (y_min == y_max)))
            continue; // 跳过固定的实例

        Tile *bestTile = nullptr; // 最佳的Tile位置
        int bestGain = 0;         // 最佳增益

        std::vector<std::pair<int, int>> pointsToCheck = {
            {x_min, y_min}, {x_min, y_max}, {x_max, y_min}, {x_max, y_max}, {(x_min + x_max) / 2, (y_min + y_max) / 2}};
        // std::vector<std::pair<int, int>> pointsToCheck = {
        //     {(x_min + x_max) / 2, (y_min + y_max) / 2}};

        for (const auto &point : pointsToCheck)
        {
            Tile *tile = chip.getTile(point.first, point.second); // 获取目标Tile
            if (tile == nullptr || !tile->matchType(inst->getModelName()))
            {
                continue; // 如果Tile不存在或类型不匹配，跳过
            }

            // 检查Tile是否有足够资源容纳inst
            tile->getRemainingPLBResources(false);
            if (!tile->hasEnoughResources(inst))
            {
                continue; // 如果资源不足，跳过
            }

            // 计算当前移动的增益
            int gain = calculateGain(inst, tile);
            if (gain > bestGain)
            {
                bestGain = gain;
                bestTile = tile; // 更新最佳Tile
            }
        }

        // 如果找到最佳位置并且增益为正，则移动inst
        if (bestTile != nullptr && bestGain > 0)
        {
            moveInstance(inst, bestTile);
        }
        // count++;

        // std::cout << count << std::endl;
    }

    std::cout << "--------FM完成--------" << std::endl;
}

// 统计每种类型的数量
void generateOutputFile(const std::string &filename)
{
    std::ofstream outFile(filename);
    if (!outFile)
    {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    // 预定义表头中涉及的类型名称和顺序
    std::vector<std::string> modelOrder = {
        "CARRY4", "F7MUX", "LUT1", "LUT2", "LUT3", "LUT4", "LUT5", "LUT6",
        "LUT6X", "SEQ", "DRAM", "DSP", "RAMA", "IOA", "IOB", "GCLK"};

    // 用于统计每种类型实例的数量
    std::map<std::string, int> typeCountMap;

    // 遍历 glbInstMap 统计每种类型的数量
    for (const auto &instPair : glbInstMap)
    {
        Instance *inst = instPair.second;
        std::string type = inst->getModelName();
        typeCountMap[type]++;
    }

    // 输出统计表头，按照固定顺序
    outFile << "# List of Models in This Case: \n";
    for (const auto &model : modelOrder)
    {
        outFile << "#   " << std::setw(25) << std::left << model << ": " << typeCountMap[model] << "\n";
    }
    outFile << "\n";

    // 遍历 glbInstMap 输出实例的位置信息和类型
    for (const auto &instPair : glbInstMap)
    {
        Instance *inst = instPair.second;
        int instID = instPair.first;

        // 获取实例的基础位置 (x, y, z)
        auto loc = inst->getBaseLocation();
        int x = std::get<0>(loc);
        int y = std::get<1>(loc);
        int z = std::get<2>(loc);

        // 获取实例的类型
        std::string type = inst->getModelName();

        // 输出实例位置信息
        outFile << "X" << x << "Y" << y << "Z" << z << " " << type << " inst_" << instID;

        // 如果实例是固定的，输出 FIXED
        if (inst->isFixed())
        {
            outFile << " FIXED";
        }

        outFile << "\n";
    }

    outFile.close();
    std::cout << "文件生成成功: " << filename << std::endl;
}
// 匹配LUT对的函数
void matchLUTPairs11(std::map<int, Instance *> &glbInstMap)
{
    std::unordered_map<int, std::unordered_set<int>> lutNetMap; // LUT ID -> 连接的net ID集合（仅输入引脚）
    std::unordered_map<int, std::unordered_set<int>> netLUTMap; // Net ID -> 连接到该net的LUT ID集合
    std::vector<int> unmatchedLUTs;                             // 未匹配的LUT集合

    // 构建 LUT 和 Net 的映射
    for (const auto &instPair : glbInstMap)
    {
        int instID = instPair.first;
        Instance *inst = instPair.second;

        // 跳过固定的实例或类型不是以 "LUT" 开头的实例
        if (inst->isFixed() || !isLUTType(inst->getModelName()))
        {
            continue;
        }

        // 添加未匹配的LUT
        unmatchedLUTs.push_back(instID);

        // 遍历当前实例的输入引脚nets
        for (int i = 0; i < inst->getNumInpins(); ++i)
        {
            Pin *pin = inst->getInpin(i);
            int netID = pin->getNetID();

            // 跳过未连接的引脚或为CLOCK的net
            if (netID == -1 || pin->getProp() == PIN_PROP_CLOCK)
            {
                continue;
            }

            // 更新 lutNetMap 和 netLUTMap 的映射
            lutNetMap[instID].insert(netID);
            netLUTMap[netID].insert(instID);
        }
    }

    // 按引脚数量从多到少排序 unmatchedLUTs
    std::sort(unmatchedLUTs.begin(), unmatchedLUTs.end(), [&](int a, int b) {
        return glbInstMap[a]->getNumInpins() > glbInstMap[b]->getNumInpins();
    });

    // 进行LUT匹配
    while (!unmatchedLUTs.empty())
    {
        int bestMatchedLUTID = -1;
        int maxSharedNets = -1;

        // 从未匹配的LUT中选择一个
        int currentLUTID = unmatchedLUTs.front();
        Instance *currentLUT = glbInstMap[currentLUTID];
        unmatchedLUTs.erase(unmatchedLUTs.begin()); // 从未匹配集合中移除

        // 获取当前LUT连接的所有net（输入引脚）
        const auto &currentLUTNets = lutNetMap[currentLUTID];

        // 遍历这些net，找到共享的LUT
        for (int netID : currentLUTNets)
        {
            const auto &relatedLUTs = netLUTMap[netID];

            for (int otherLUTID : relatedLUTs)
            {
                if (otherLUTID == currentLUTID || std::find(unmatchedLUTs.begin(), unmatchedLUTs.end(), otherLUTID) == unmatchedLUTs.end())
                {
                    continue; // 跳过自身或已匹配的LUT
                }

                Instance *otherLUT = glbInstMap[otherLUTID];

                if (otherLUT->isFixed()) // 跳过固定的LUT
                {
                    continue;
                }

                // 计算共享net数量
                const auto &otherLUTNets = lutNetMap[otherLUTID];
                int sharedNetCount = 0;
                for (int net : currentLUTNets)
                {
                    if (otherLUTNets.find(net) != otherLUTNets.end())
                    {
                        sharedNetCount++;
                    }
                }

                // 使用 unionSets 计算合并后的引脚数量
                std::unordered_set<int> unionPins = unionSets(currentLUTNets, otherLUTNets);
                int totalInpins = unionPins.size();

                // 更新最佳匹配
                if (sharedNetCount > maxSharedNets && totalInpins <= 6)
                {
                    maxSharedNets = sharedNetCount;
                    bestMatchedLUTID = otherLUTID;
                }
            }
        }

        // 如果找到最佳匹配，则进行标记并移除
        if (bestMatchedLUTID != -1)
        {
            currentLUT->setMatchedLUTID(bestMatchedLUTID);
            glbInstMap[bestMatchedLUTID]->setMatchedLUTID(currentLUTID);
            unmatchedLUTs.erase(std::remove(unmatchedLUTs.begin(), unmatchedLUTs.end(), bestMatchedLUTID), unmatchedLUTs.end());
        }
    }
}


// 修改后的匹配LUT对的多线程函数
void matchLUTPairsThread(std::map<int, Instance *> &glbInstMap, std::vector<int> &unmatchedLUTs, 
                         std::unordered_map<int, std::unordered_set<int>> &lutNetMap, 
                         std::unordered_map<int, std::unordered_set<int>> &netLUTMap, int start, int end)
{
    while (start < end)
    {
        int bestMatchedLUTID = -1;
        int maxSharedNets = -1;

        int currentLUTID = unmatchedLUTs[start];
        Instance *currentLUT = glbInstMap[currentLUTID];

        // 检查当前 LUT 是否已被匹配
        {
            std::lock_guard<std::mutex> lock(lutMutex);
            if (matchedLUTs.find(currentLUTID) != matchedLUTs.end()) {
                ++start; // 跳过已匹配的 LUT
                continue;
            }
        }

        const auto &currentLUTNets = lutNetMap[currentLUTID];

        for (int netID : currentLUTNets)
        {
            const auto &relatedLUTs = netLUTMap[netID];

            for (int otherLUTID : relatedLUTs)
            {
                if (otherLUTID == currentLUTID) continue;

                Instance *otherLUT = glbInstMap[otherLUTID];

                {
                    std::lock_guard<std::mutex> lock(lutMutex);
                    if (matchedLUTs.find(otherLUTID) != matchedLUTs.end()) continue;
                }

                const auto &otherLUTNets = lutNetMap[otherLUTID];
                int sharedNetCount = 0;
                for (int net : currentLUTNets) {
                    if (otherLUTNets.find(net) != otherLUTNets.end()) {
                        sharedNetCount++;
                    }
                }

                std::unordered_set<int> unionPins = unionSets(currentLUTNets, otherLUTNets);
                int totalInpins = unionPins.size();

                if (sharedNetCount > maxSharedNets && totalInpins <= 6)
                {
                    maxSharedNets = sharedNetCount;
                    bestMatchedLUTID = otherLUTID;
                }
            }
        }

        if (bestMatchedLUTID != -1)
        {
            std::lock_guard<std::mutex> lock(lutMutex);
            if (matchedLUTs.find(currentLUTID) == matchedLUTs.end() && 
                matchedLUTs.find(bestMatchedLUTID) == matchedLUTs.end())
            {
                matchedLUTs.insert(currentLUTID);
                matchedLUTs.insert(bestMatchedLUTID);
                currentLUT->setMatchedLUTID(bestMatchedLUTID);
                glbInstMap[bestMatchedLUTID]->setMatchedLUTID(currentLUTID);
            }
        }

        ++start;
    }
}

void matchLUTPairs(std::map<int, Instance *> &glbInstMap)
{
    std::unordered_map<int, std::unordered_set<int>> lutNetMap; // LUT ID -> 连接的net ID集合（仅输入引脚）
    std::unordered_map<int, std::unordered_set<int>> netLUTMap; // Net ID -> 连接到该net的LUT ID集合
    std::vector<int> unmatchedLUTs;                             // 未匹配的LUT集合

    // 构建 LUT 和 Net 的映射
    for (const auto &instPair : glbInstMap)
    {
        int instID = instPair.first;
        Instance *inst = instPair.second;

        // 跳过类型不是以 "LUT" 开头的实例
        if (!isLUTType(inst->getModelName()))
        {
            continue;
        }

        // 添加未匹配的LUT
        unmatchedLUTs.push_back(instID);

        // 遍历当前实例的输入引脚nets
        for (int i = 0; i < inst->getNumInpins(); ++i)
        {
            Pin *pin = inst->getInpin(i);
            int netID = pin->getNetID();

            // 跳过未连接的引脚或为CLOCK的net
            if (netID == -1 || pin->getProp() == PIN_PROP_CLOCK)
            {
                continue;
            }

            // 更新 lutNetMap 和 netLUTMap 的映射
            lutNetMap[instID].insert(netID);
            netLUTMap[netID].insert(instID);
        }
    }

    // 按引脚数量从多到少排序 unmatchedLUTs
    std::sort(unmatchedLUTs.begin(), unmatchedLUTs.end(), [&](int a, int b) {
        return glbInstMap[a]->getNumInpins() > glbInstMap[b]->getNumInpins();
    });

    // 使用多线程进行匹配
    const int numThreads = 8; // 线程数量
    int chunkSize = unmatchedLUTs.size() / numThreads; // 每个线程处理的LUT数量
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i)
    {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? unmatchedLUTs.size() : (i + 1) * chunkSize;

        threads.emplace_back(matchLUTPairsThread, std::ref(glbInstMap), std::ref(unmatchedLUTs),
                             std::ref(lutNetMap), std::ref(netLUTMap), start, end);
    }

    for (auto &t : threads)
    {
        t.join(); // 等待所有线程完成
    }
}