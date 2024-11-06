#include <iomanip>
// #include "legal.h"
#include "object.h"
#include "method.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

#include <thread>
#include <mutex>

bool isLUTType(const std::string &modelName);

// 定义新的实例ID生成器
int generateNewInstanceID()
{
    static int newID = 0;
    return newID++;
}

// 共享数据结构的锁
std::mutex lutMutex;
std::unordered_set<int> matchedLUTs; // 存储已匹配的 LUT IDs
std::mutex seqPlacementMutex;        // 互斥锁保护对 seqPlacementMap 的访问

// LUT组匹配,将剩余未加入LUT组的LUT单独加入新的LUT组——已确定正确
void populateLUTGroups(std::map<int, Instance *> &glbInstMap)
{
    for (const auto &instPair : glbInstMap)
    {
        int instID = instPair.first;
        Instance *inst = instPair.second;

        // 跳过非LUT的实例
        if (!isLUTType(inst->getModelName()))
        {
            continue;
        }

        // 检查实例的 matchedLUTID
        int matchedID = inst->getMatchedLUTID();

        if (matchedID == -1)
        {
            // 实例未匹配，作为单个 LUT 插入 lutGroups
            int newInstanceID = generateNewInstanceID();
            lutGroups[newInstanceID] = {inst};
            inst->setLUTSetID(newInstanceID);
        }
    }
}

// 如果组内有一个固定的LUT且另一个是非固定的LUT，则更新非固定LUT的位置
// 如果组内的LUT都是非固定的，将第二个LUT的位置更新为第一个LUT的位置——已确定正确
void refreshLUTGroups(std::map<int, std::set<Instance *>> &lutGroups)
{
    for (auto &groupPair : lutGroups)
    {
        auto &lutGroup = groupPair.second;

        // 初始化指针用于存储固定和非固定的LUT实例
        Instance *fixedLUT = nullptr;
        Instance *movableLUT1 = nullptr;
        Instance *movableLUT2 = nullptr;

        // 遍历LUT组中的实例
        for (Instance *lut : lutGroup)
        {
            if (lut->isFixed())
            {
                fixedLUT = lut;
                break; // 一旦找到固定的LUT，立即停止循环
            }
            else
            {
                if (!movableLUT1)
                {
                    movableLUT1 = lut;
                }
                else if (!movableLUT2)
                {
                    movableLUT2 = lut;
                    break; // 找到第二个非固定的LUT后即可停止循环
                }
            }
        }

        // 如果有一个固定的LUT，且组内有其他非固定LUT，则更新非固定LUT的位置
        if (fixedLUT && movableLUT1)
        {
            movableLUT1->setLocation(fixedLUT->getLocation());
            movableLUT1->modifyFixed(true);
        }

        // 如果组内只有非固定的LUT，将第二个LUT位置更新为第一个LUT的位置
        if (!fixedLUT && movableLUT1 && movableLUT2)
        {
            movableLUT2->setLocation(movableLUT1->getLocation());
        }
    }
}

// 检查实例是否为 LUT 类型
bool isLUTType(const std::string &modelName)
{
    return modelName.rfind("LUT", 0) == 0; // 确认modelName以"LUT"开头
}

// 计算两个 unordered_set 的并集
std::unordered_set<int> unionSets(const std::unordered_set<int> &set1, const std::unordered_set<int> &set2)
{
    std::unordered_set<int> result(set1);    // 先将 set1 拷贝到结果集合中
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
void generateOutputFile(bool isBaseline, const std::string &filename)
{
    std::ofstream outFile(filename);
    if (!outFile)
    {
        std::cerr << "fail to open file " << filename << std::endl;
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
        std::tuple<int, int, int> loc;
        if (isBaseline)
            loc = inst->getBaseLocation();
        else
            loc = inst->getLocation();
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
    std::cout << "Result file generate in " << filename << std::endl;
}

// 获取json文件信息
std::string getValue(const std::string &jsonContent, const std::string &key)
{
    // 构造查找键的格式，例如："libFile":
    std::string searchKey = "\"" + key + "\":";

    // 找到键的位置
    size_t startPos = jsonContent.find(searchKey);
    if (startPos == std::string::npos)
    {
        return "";
    }

    // 跳过键和引号，定位到值的起始位置
    startPos = jsonContent.find("\"", startPos + searchKey.length()) + 1;
    size_t endPos = jsonContent.find("\"", startPos);

    // 返回提取到的值
    return jsonContent.substr(startPos, endPos - startPos);
}

// 赋值isPLB数组
int setIsPLB()
{
    int numCol = chip.getNumCol(); // 150
    int numRow = chip.getNumRow(); // 300
    isPLB = new int *[numCol];
    for (int i = 0; i < numCol; i++)
    {
        isPLB[i] = new int[numRow];
    }
    Tile ***tileArray = chip.getTileArray();
    for (int i = 0; i < numCol; i++)
    {
        for (int j = 0; j < numRow; j++)
        {
            if (tileArray[i][j] != nullptr && tileArray[i][j]->matchType("PLB"))
            {
                isPLB[i][j] = 1;
            }
            else
            {
                isPLB[i][j] = 0;
            }
        }
    }
    // std::ofstream outputFile;
    // outputFile.open("isPLB.csv");
    // for(int i = 0; i < numCol; i++){
    //     for(int j = 0; j < numRow; j++){
    //         outputFile<<isPLB[i][j]<<',';
    //     }
    //     outputFile<<std::endl;
    // }
    // outputFile.close();
    return 0;
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
    std::sort(unmatchedLUTs.begin(), unmatchedLUTs.end(), [&](int a, int b)
              { return glbInstMap[a]->getNumInpins() > glbInstMap[b]->getNumInpins(); });

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
        int inputPinNum = currentLUT->getUsedNumInpins();
        {
            std::lock_guard<std::mutex> lock(lutMutex);
            if (matchedLUTs.find(currentLUTID) != matchedLUTs.end())
            {
                ++start;
                continue;
            }
        }

        const auto &currentLUTNets = lutNetMap[currentLUTID];

        for (int netID : currentLUTNets)
        {
            const auto &relatedLUTs = netLUTMap[netID];

            for (int otherLUTID : relatedLUTs)
            {
                if (otherLUTID == currentLUTID)
                    continue;

                Instance *otherLUT = glbInstMap[otherLUTID];

                // 距离限制判断
                int twoinstwirelength = calculateTwoInstanceWireLength(currentLUT, otherLUT, false);
                if (twoinstwirelength >= 10)
                {
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(lutMutex);
                    if (matchedLUTs.find(otherLUTID) != matchedLUTs.end())
                        continue;
                }

                // 添加条件：如果两个 LUT 都是固定的，则跳过
                if (currentLUT->isFixed() && otherLUT->isFixed())
                    continue;

                const auto &otherLUTNets = lutNetMap[otherLUTID];
                int sharedNetCount = 0;
                for (int net : currentLUTNets)
                {
                    if (otherLUTNets.find(net) != otherLUTNets.end())
                    {
                        sharedNetCount++;
                    }
                }

                std::unordered_set<int> unionPins = unionSets(currentLUTNets, otherLUTNets);
                int totalInpins = unionPins.size();

                if (sharedNetCount > maxSharedNets && totalInpins == inputPinNum && currentLUTNets.size() == otherLUTNets.size())
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

                int newInstanceID = generateNewInstanceID();
                lutGroups[newInstanceID] = {currentLUT, glbInstMap[bestMatchedLUTID]};
                currentLUT->setLUTSetID(newInstanceID);
                glbInstMap[bestMatchedLUTID]->setLUTSetID(newInstanceID);
            }
        }

        ++start;
    }
}

// 打包代码
void matchLUTPairs(std::map<int, Instance *> &glbInstMap, bool isLutPack, bool isSeqPack)
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
    std::sort(unmatchedLUTs.begin(), unmatchedLUTs.end(), [&](int a, int b)
              { return glbInstMap[a]->getNumInpins() > glbInstMap[b]->getNumInpins(); });

    // 使用多线程进行匹配
    const int numThreads = 8;                          // 线程数量
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
    if (isLutPack)
    {
        populateLUTGroups(glbInstMap);
        refreshLUTGroups(lutGroups); // 这里会根据LUT组其中一个固定的位置修改另一个未固定的LUT位置并且将其固定
        matchFixedLUTGroupsToPLB(lutGroups, plbGroups);
        updatePLBLocations(plbGroups);
    }
    if (isSeqPack)
    {
        initializeSEQPlacementMap(glbInstMap);
        updateSEQLocations(seqPlacementMap);
    }
    updateInstancesToTiles();
}

// PLB打包，将LUT组打包成PLB组
void matchFixedLUTGroupsToPLB(std::map<int, std::set<Instance *>> &lutGroups, std::map<int, std::set<std::set<Instance *>>> &plbGroups)
{
    std::unordered_map<int, std::unordered_set<int>> lutGroupNetMap; // LUT组 ID -> 所有连接的net ID（并集）
    std::unordered_map<int, std::unordered_set<int>> netLUTGroupMap; // Net ID -> 所有连接该net的LUT组 ID
    std::set<int> unmatchedLUTGroups;                                // 未匹配的LUT组集合
    const int maxGroupCount = 8;                                     // 一个PLB最多容纳8个LUT组

    int unfixedCount = 0;

    // 构建 LUT组 和 Net 的映射
    for (const auto &groupPair : lutGroups)
    {
        int groupID = groupPair.first;
        const auto &lutGroup = groupPair.second;

        // 初始化未匹配的LUT组
        unmatchedLUTGroups.insert(groupID);

        // 遍历每个 LUT 组内的所有实例的 net 集合，取并集
        std::unordered_set<int> groupNets;
        for (Instance *lut : lutGroup)
        {
            for (int i = 0; i < lut->getNumInpins(); ++i)
            {
                Pin *pin = lut->getInpin(i);
                int netID = pin->getNetID();

                // 跳过未连接的引脚或属性为 CLOCK 的 net
                if (netID == -1 || pin->getProp() == PIN_PROP_CLOCK)
                {
                    continue;
                }

                groupNets.insert(netID); // 添加到LUT组的net并集中
                netLUTGroupMap[netID].insert(groupID);
            }

            for (int i = 0; i < lut->getNumOutpins(); ++i)
            {
                Pin *pin = lut->getOutpin(i);
                int netID = pin->getNetID();

                // 跳过未连接的引脚或属性为 CLOCK 的 net
                if (netID == -1 || pin->getProp() == PIN_PROP_CLOCK)
                {
                    continue;
                }

                groupNets.insert(netID); // 添加到LUT组的net并集中
                netLUTGroupMap[netID].insert(groupID);
            }
        }
        lutGroupNetMap[groupID] = std::move(groupNets); // 添加LUT组的net集合到 lutGroupNetMap
    }

    // 开始匹配 LUT 组
    while (!unmatchedLUTGroups.empty())
    {
        int currentPLBID = plbGroups.size();                       // 新的PLB ID
        std::set<std::set<Instance *>> currentPLB;                 // 当前PLB内的LUT组集合
        std::tuple<int, int, int> plbFixedLocation = {-1, -1, -1}; // 初始化固定位置为无效值
        Tile *tilePtr = nullptr;                                   // 初始化 tilePtr

        // 从未匹配的LUT组中选择一个作为初始组
        int currentGroupID = *unmatchedLUTGroups.begin();
        Instance *currentFirstInstance = *lutGroups[currentGroupID].begin();

        if (!currentFirstInstance->isFixed())
        {
            unmatchedLUTGroups.erase(currentGroupID);
            // unfixedCount ++;
            continue;
        }

        // 将同tile下的固定的LUT组添加到PLB
        currentPLB.insert(lutGroups[currentGroupID]);
        unmatchedLUTGroups.erase(currentGroupID);
        plbFixedLocation = currentFirstInstance->getLocation();
        tilePtr = chip.getTile(std::get<0>(plbFixedLocation), std::get<1>(plbFixedLocation));
        if (tilePtr != nullptr && tilePtr->getTileTypes().count("PLB"))
        {
            std::vector<std::set<Instance *>> temp = tilePtr->getFixedOptimizedLUTGroups(); // 获取tile下的固定的LUT
            for (const auto &lutGroup : temp)
            {
                std::set<Instance *> extendedLUTGroup = lutGroup; // 创建扩展的 LUT 组

                // 遍历 lutGroup 中的每个 Instance，检查其是否有匹配的 LUT
                for (Instance *lut : lutGroup)
                {
                    std::tuple<int, int, int> lutLocation = lut->getLocation();
                    if (std::get<0>(lutLocation) == std::get<0>(plbFixedLocation) &&
                        std::get<1>(lutLocation) == std::get<1>(plbFixedLocation))
                    {
                        int matchedID = lut->getMatchedLUTID(); // 获取匹配 LUT 的 ID
                        if (matchedID != -1)                    // 如果存在匹配 LUT
                        {
                            auto matchedIt = glbInstMap.find(matchedID); // 在 glbInstMap 中查找
                            if (matchedIt != glbInstMap.end())
                            {
                                Instance *matchedLUT = matchedIt->second;
                                extendedLUTGroup.insert(matchedLUT); // 将匹配的 LUT 添加到扩展组
                            }
                        }
                        // 尝试插入到 currentPLB
                        auto result = currentPLB.insert(extendedLUTGroup); // 使用 set 的特性，自动去重
                        for (Instance *lut : extendedLUTGroup)
                        {
                            lut->setPLBGroupID(currentPLBID); // 设置plbGroupID
                        }

                        // 如果插入成功，说明该组是新的组，需从 unmatchedLUTGroups 中擦除
                        if (result.second) // result.second 为 true 表示插入成功
                        {
                            // 遍历并从 unmatchedLUTGroups 中擦除对应的 LUT 组
                            for (const auto &instance : lutGroup)
                            {
                                int groupID = instance->getLUTSetID(); // 假设有一个方法获取组 ID
                                unmatchedLUTGroups.erase(groupID);
                            }
                        }
                    }
                }
            }
            int currentFixedLUTCount = currentPLB.size(); // 检查该tile的LUT资源是否足够
            if (currentFixedLUTCount >= maxGroupCount)
            {
                plbGroups[currentPLBID] = currentPLB;
                continue; // 跳过资源不足的 tile
            }
        }

        // std::set<int> availableSites = {0, 1, 2, 3, 4, 5, 6, 7};
        // 检查tilePtr的DRAM占用情况，占用0则要去除LUT的0-3编号，占用1则要去除LUT的4-7编号
        std::vector<int> dramInTile = tilePtr->getFixedOptimizedDRAMGroups();
        int dramNum = dramInTile.size();

        // 继续添加其他LUT组到当前PLB，直到达到限制或没有匹配项
        while (currentPLB.size() < MAX_LUT_CAPACITY - dramNum * 4)
        {
            int bestMatchedGroupID = -1;
            int maxSharedNets = -1;

            // 获取当前LUT组的net集合
            const auto &currentGroupNets = lutGroupNetMap[currentGroupID];

            // 遍历当前组的net，找到与该net共享的所有LUT组
            for (int netID : currentGroupNets)
            {
                const auto &relatedLUTGroups = netLUTGroupMap[netID];

                for (int otherGroupID : relatedLUTGroups)
                {
                    Instance *luttemp = *lutGroups[otherGroupID].begin();

                    if (unmatchedLUTGroups.find(otherGroupID) == unmatchedLUTGroups.end() || luttemp->isFixed())
                    {
                        continue; // 跳过已匹配的LUT组和固定的LUT组
                    }

                    const auto &otherGroupNets = lutGroupNetMap[otherGroupID];

                    // 计算共享网络数量
                    int sharedNetCount = 0;
                    for (int net : currentGroupNets)
                    {
                        if (otherGroupNets.find(net) != otherGroupNets.end())
                        {
                            sharedNetCount++;
                        }
                    }

                    if (sharedNetCount > maxSharedNets)
                    {
                        maxSharedNets = sharedNetCount;
                        bestMatchedGroupID = otherGroupID;
                    }
                }
            }

            // 如果找到最佳匹配，将其添加到PLB中
            if (bestMatchedGroupID != -1)
            {
                for (Instance *lut : lutGroups[bestMatchedGroupID])
                {
                    if (!lut->isFixed())
                    {
                        unmatchedLUTGroups.erase(bestMatchedGroupID); // 从未匹配列表中移除
                        currentPLB.insert(lutGroups[bestMatchedGroupID]);
                        lut->setPLBGroupID(currentPLBID); // 设置plbGroupID
                    }
                }
            }
            else
            {
                break; // 没有更多可添加的LUT组
            }
        }

        // 将当前PLB添加到plbGroups
        plbGroups[currentPLBID] = currentPLB;
    }

    // 遍历所有LUT组，检查未分配的PLBGroupID的组
    std::set<int> unassignedLUTGroups;
    for (const auto &groupPair : lutGroups)
    {
        int groupID = groupPair.first;
        const auto &lutGroup = groupPair.second;
        for (Instance *lut : lutGroup)
        {
            if (lut->getPLBGroupID() == -1)
            {
                unassignedLUTGroups.insert(groupID); // 添加未分配的LUT组ID
                break;                               // 只需添加一次
            }
        }
    }

    // 为未分配的LUT组创建新的PLB组——已经正确
    while (!unassignedLUTGroups.empty())
    {
        int newPLBID = plbGroups.size();
        std::set<Instance *> newPLBGroup;

        // 从未分配的LUT组中选择组进行处理
        int currentGroupID = *unassignedLUTGroups.begin();
        newPLBGroup = lutGroups[currentGroupID];
        newPLBGroup.insert(newPLBGroup.begin(), newPLBGroup.end()); // 复制组内所有实例
        for (Instance *lut : newPLBGroup)
        {

            lut->setPLBGroupID(newPLBID); // 设置新的PLB组ID
        }
        plbGroups[newPLBID].insert(newPLBGroup);   // 添加到plbGroups
        unassignedLUTGroups.erase(currentGroupID); // 从未分配列表中移除
    }

    std::cout << "plbGroups size : " << plbGroups.size() << std::endl;
}

// PLB打包，将LUT组打包成PLB组
void matchLUTGroupsToPLB(std::map<int, std::set<Instance *>> &lutGroups, std::map<int, std::set<std::set<Instance *>>> &plbGroups)
{
    std::unordered_map<int, std::unordered_set<int>> lutGroupNetMap; // LUT组 ID -> 所有连接的net ID（并集）
    std::unordered_map<int, std::unordered_set<int>> netLUTGroupMap; // Net ID -> 所有连接该net的LUT组 ID
    std::set<int> unmatchedLUTGroups;                                // 未匹配的LUT组集合
    const int maxGroupCount = 8;                                     // 一个PLB最多容纳8个LUT组

    // 构建 LUT组 和 Net 的映射
    for (const auto &groupPair : lutGroups)
    {
        int groupID = groupPair.first;
        const auto &lutGroup = groupPair.second;

        // 初始化未匹配的LUT组
        unmatchedLUTGroups.insert(groupID);

        // 遍历每个 LUT 组内的所有实例的 net 集合，取并集
        std::unordered_set<int> groupNets;
        for (Instance *lut : lutGroup)
        {
            for (int i = 0; i < lut->getNumInpins(); ++i)
            {
                Pin *pin = lut->getInpin(i);
                int netID = pin->getNetID();

                // 跳过未连接的引脚或属性为 CLOCK 的 net
                if (netID == -1 || pin->getProp() == PIN_PROP_CLOCK)
                {
                    continue;
                }

                groupNets.insert(netID); // 添加到LUT组的net并集中
                netLUTGroupMap[netID].insert(groupID);
            }

            for (int i = 0; i < lut->getNumOutpins(); ++i)
            {
                Pin *pin = lut->getOutpin(i);
                int netID = pin->getNetID();

                // 跳过未连接的引脚或属性为 CLOCK 的 net
                if (netID == -1 || pin->getProp() == PIN_PROP_CLOCK)
                {
                    continue;
                }

                groupNets.insert(netID); // 添加到LUT组的net并集中
                netLUTGroupMap[netID].insert(groupID);
            }
        }
        lutGroupNetMap[groupID] = std::move(groupNets); // 添加LUT组的net集合到 lutGroupNetMap
    }

    // 开始匹配 LUT 组
    while (!unmatchedLUTGroups.empty())
    {
        int currentPLBID = plbGroups.size();                       // 新的PLB ID
        std::set<std::set<Instance *>> currentPLB;                 // 当前PLB内的LUT组集合
        std::tuple<int, int, int> plbFixedLocation = {-1, -1, -1}; // 初始化固定位置为无效值
        Tile *tilePtr = nullptr;                                   // 初始化 tilePtr

        // 从未匹配的LUT组中选择一个作为初始组
        int currentGroupID = *unmatchedLUTGroups.begin();
        // 将当前的LUT组添加到PLB
        currentPLB.insert(lutGroups[currentGroupID]);

        unmatchedLUTGroups.erase(currentGroupID);

        // 检查当前组是否包含固定的 LUT
        bool hasFixedLUT = false;

        // 处理固定的 LUT 组
        for (Instance *lut : lutGroups[currentGroupID])
        {
            lut->setPLBGroupID(currentPLBID); // 设置plbGroupID
            // lut->setLUTInitial(true);
            if (lut->isFixed())
            {
                plbFixedLocation = lut->getLocation(); // 设置PLB的固定位置
                hasFixedLUT = true;
            }
        }

        // 如果有固定的LUT，优先处理固定的LUT
        if (hasFixedLUT)
        {
            tilePtr = chip.getTile(std::get<0>(plbFixedLocation), std::get<1>(plbFixedLocation));
            if (tilePtr != nullptr && tilePtr->getTileTypes().count("PLB"))
            {
                std::vector<std::set<Instance *>> temp = tilePtr->getFixedOptimizedLUTGroups();
                for (const auto &lutGroup : temp)
                {
                    std::set<Instance *> extendedLUTGroup = lutGroup; // 创建扩展的 LUT 组

                    // 遍历 lutGroup 中的每个 Instance，检查其是否有匹配的 LUT
                    for (Instance *lut : lutGroup)
                    {
                        std::tuple<int, int, int> lutLocation = lut->getLocation();
                        if (std::get<0>(lutLocation) == std::get<0>(plbFixedLocation) &&
                            std::get<1>(lutLocation) == std::get<1>(plbFixedLocation))
                        {
                            int matchedID = lut->getMatchedLUTID(); // 获取匹配 LUT 的 ID
                            if (matchedID != -1)                    // 如果存在匹配 LUT
                            {
                                auto matchedIt = glbInstMap.find(matchedID); // 在 glbInstMap 中查找
                                if (matchedIt != glbInstMap.end())
                                {
                                    Instance *matchedLUT = matchedIt->second;
                                    extendedLUTGroup.insert(matchedLUT); // 将匹配的 LUT 添加到扩展组
                                }
                            }
                            // 尝试插入到 currentPLB
                            auto result = currentPLB.insert(extendedLUTGroup); // 使用 set 的特性，自动去重
                            for (Instance *lut : extendedLUTGroup)
                            {
                                lut->setPLBGroupID(currentPLBID); // 设置plbGroupID
                            }

                            // 如果插入成功，说明该组是新的组，需从 unmatchedLUTGroups 中擦除
                            if (result.second) // result.second 为 true 表示插入成功
                            {
                                // 遍历并从 unmatchedLUTGroups 中擦除对应的 LUT 组
                                for (const auto &instance : lutGroup)
                                {
                                    int groupID = instance->getLUTSetID(); // 假设有一个方法获取组 ID
                                    unmatchedLUTGroups.erase(groupID);
                                }
                            }
                        }
                    }
                }
                int currentFixedLUTCount = currentPLB.size(); // 检查该tile的LUT资源是否足够
                if (currentFixedLUTCount >= maxGroupCount)
                {
                    // std::cout << "Error: Tile at fixed location has insufficient resources." << std::endl;
                    plbGroups[currentPLBID] = currentPLB;
                    continue; // 跳过资源不足的 tile
                }
            }
            else
            {
                std::cout << "Error: Fixed LUT location is invalid or not a PLB tile." << std::endl;
                plbGroups[currentPLBID] = currentPLB;
                continue; // 跳过无效或非PLB tile的固定位置
            }
        }

        // 继续添加其他LUT组到当前PLB，直到达到限制或没有匹配项
        while (currentPLB.size() < maxGroupCount)
        {
            int bestMatchedGroupID = -1;
            int maxSharedNets = -1;

            // 获取当前LUT组的net集合
            const auto &currentGroupNets = lutGroupNetMap[currentGroupID];

            // 遍历当前组的net，找到与该net共享的所有LUT组
            for (int netID : currentGroupNets)
            {
                const auto &relatedLUTGroups = netLUTGroupMap[netID];

                for (int otherGroupID : relatedLUTGroups)
                {
                    if (unmatchedLUTGroups.find(otherGroupID) == unmatchedLUTGroups.end())
                    {
                        continue; // 跳过已匹配的LUT组
                    }

                    const auto &otherGroupNets = lutGroupNetMap[otherGroupID];

                    // 计算共享网络数量
                    int sharedNetCount = 0;
                    for (int net : currentGroupNets)
                    {
                        if (otherGroupNets.find(net) != otherGroupNets.end())
                        {
                            sharedNetCount++;
                        }
                    }

                    if (sharedNetCount > maxSharedNets)
                    {
                        maxSharedNets = sharedNetCount;
                        bestMatchedGroupID = otherGroupID;
                    }
                }
            }

            // 如果找到最佳匹配，将其添加到PLB中
            if (bestMatchedGroupID != -1)
            {
                unmatchedLUTGroups.erase(bestMatchedGroupID); // 从未匹配列表中移除
                // // 从未匹配列表中移除对应的 LUT 实例 ID
                // for (Instance *lut : lutGroups[bestMatchedGroupID])
                // {
                //     int lutInstID = lut->getInstID();    // 获取实例 ID
                //     unmatchedLUTGroups.erase(lutInstID); // 从未匹配列表中移除实例 ID
                //     // 设置 plbGroupID
                //     lut->setPLBGroupID(currentPLBID);
                // }
                // // 遍历并从 unmatchedLUTGroups 中擦除对应的 LUT 组
                // for (const auto &instance : lutGroups[bestMatchedGroupID])
                // {
                //     int groupID = instance->getMatchedLUTID(); // 假设有一个方法获取组 ID
                //     unmatchedLUTGroups.erase(groupID);
                // }
                for (Instance *lut : lutGroups[bestMatchedGroupID])
                {
                    if (!lut->isFixed())
                    {
                        currentPLB.insert(lutGroups[bestMatchedGroupID]);
                        lut->setPLBGroupID(currentPLBID); // 设置plbGroupID
                    }
                }
            }
            else
            {
                break; // 没有更多可添加的LUT组
            }
        }

        // 将当前PLB添加到plbGroups
        plbGroups[currentPLBID] = currentPLB;
    }

    // 遍历所有LUT组，检查未分配的PLBGroupID的组
    std::set<int> unassignedLUTGroups;
    for (const auto &groupPair : lutGroups)
    {
        int groupID = groupPair.first;
        const auto &lutGroup = groupPair.second;
        for (Instance *lut : lutGroup)
        {
            if (lut->getPLBGroupID() == -1)
            {
                unassignedLUTGroups.insert(groupID); // 添加未分配的LUT组ID
                break;                               // 只需添加一次
            }
        }
    }

    // 为未分配的LUT组创建新的PLB组——已经正确
    while (!unassignedLUTGroups.empty())
    {
        int newPLBID = plbGroups.size();
        std::set<Instance *> newPLBGroup;

        // 从未分配的LUT组中选择组进行处理
        int currentGroupID = *unassignedLUTGroups.begin();
        newPLBGroup = lutGroups[currentGroupID];
        newPLBGroup.insert(newPLBGroup.begin(), newPLBGroup.end()); // 复制组内所有实例
        for (Instance *lut : newPLBGroup)
        {
            lut->setPLBGroupID(newPLBID); // 设置新的PLB组ID
        }
        plbGroups[newPLBID].insert(newPLBGroup);   // 添加到plbGroups
        unassignedLUTGroups.erase(currentGroupID); // 从未分配列表中移除
    }

    std::cout << "plbGroups size : " << plbGroups.size() << std::endl;
}

// 更新分配PLB组内部LUT的位置和编号，仅限PLB组内部有固定LUT的
void updatePLBLocations(std::map<int, std::set<std::set<Instance *>>> &plbGroups)
{
    for (auto &plbGroup : plbGroups)
    {
        // 获取PLB组的ID和包含的LUT组集合
        int plbID = plbGroup.first;
        auto &lutGroupSet = plbGroup.second;

        std::set<int> availableSites = {0, 1, 2, 3, 4, 5, 6, 7}; // 可用的 LUT 站点索引

        // 检查是否有固定的LUT组
        std::tuple<int, int, int> fixedLocation(-1, -1, -1);
        bool hasFixedLUT = false;

        for (auto &lutGroup : lutGroupSet)
        {
            for (Instance *lut : lutGroup)
            {
                if (lut->isFixed())
                {
                    fixedLocation = lut->getLocation();
                    hasFixedLUT = true;
                    break;
                }
            }
        }

        // 如果有固定的LUT组，则更新同PLB内所有LUT组的位置
        if (hasFixedLUT)
        {
            // 第一遍遍历清理掉固定lut的编号，防止重复
            for (auto &lutGroup : lutGroupSet)
            {
                int siteIndex = *availableSites.begin();
                for (Instance *lut : lutGroup)
                {
                    if (lut->isFixed())
                    {
                        auto tempLocation = lut->getLocation();
                        availableSites.erase(std::get<2>(tempLocation));
                        break;
                    }
                }
            }
            for (auto &lutGroup : lutGroupSet)
            {
                int siteIndex = *availableSites.begin();
                for (Instance *lut : lutGroup)
                {
                    if (!lut->isFixed())
                    {
                        auto newLocation = std::make_tuple(
                            std::get<0>(fixedLocation),
                            std::get<1>(fixedLocation),
                            siteIndex);

                        lut->setLocation(newLocation);
                        lut->setFixed(true);
                        availableSites.erase(siteIndex); // 标记该站点为已使用
                    }
                }
            }
        }
    }
}

// 初始化 plbPlacementMap 的函数
void initializePLBPlacementMap(const std::map<int, std::set<std::set<Instance *>>> &plbGroups)
{
    for (const auto &plbGroup : plbGroups)
    {
        // 创建带有唯一 ID 的 PLBPlacement 实例
        int plbID = plbGroup.first;
        PLBPlacement plbPlacement(plbID);

        // 检查第一个 LUT 是否固定
        const auto &firstLUTGroup = *plbGroup.second.begin();

        bool isGroupFixed = !firstLUTGroup.empty() && (*firstLUTGroup.begin())->isFixed();

        // 存储每个 PLB 的内部 LUT 组信息
        for (const auto &lutGroup : plbGroup.second)
        {
            plbPlacement.addLUTGroup(lutGroup);

            // 获取并合并每个 LUT 的连接 nets
            for (const Instance *lut : lutGroup)
            {
                std::set<Net *> relatedNets = lut->getRelatedNets();
                for (const auto &net : relatedNets)
                {
                    plbPlacement.addConnectedNet(net->getId());
                }
            }
        }

        // 根据第一个 LUT 的固定状态设置 isFixed
        plbPlacement.setFixed(isGroupFixed);
        if (isGroupFixed)
        {
            // 获取 firstLUTGroup 的第一个 LUT
            const Instance *firstLUT = *firstLUTGroup.begin();

            // 提取 location 并将 x 和 y 存储在变量中
            int temp_x = std::get<0>(firstLUT->getLocation());
            int temp_y = std::get<1>(firstLUT->getLocation());
            std::tuple<int, int> temp_Location = std::make_tuple(temp_x, temp_y);
            plbPlacement.setPLBLocation(temp_Location);
        }

        // 将 PLBPlacement 插入到全局 plbPlacementMap 中
        plbPlacementMap[plbID] = plbPlacement;
    }
}

// 初始化seqPlacementMap的函数
void initializeSEQPlacementMap(const std::map<int, Instance *> &glbInstMap)
{
    int bankID = 0;

    for (const auto &instPair : glbInstMap)
    {
        Instance *inst = instPair.second;

        // 检查是否为 SEQ 类型，如果不是则跳过
        if (inst->getModelName().substr(0, 3) != "SEQ")
        {
            continue;
        }

        bool placed = false;

        // 尝试将当前 SEQ 实例放入现有的 bank
        for (auto &[_, bank] : seqPlacementMap)
        {
            // 判断是否符合 bank 的约束：检查连接情况并确保数量限制在 8 个以内
            if (bank.getSEQCount() < 8 && bank.addInstance(inst))
            {
                placed = true;
                inst->setSEQID(bank.getBankID());
                break;
            }
        }

        // 如果没有找到合适的 Bank，新建一个 Bank 并添加
        if (!placed)
        {
            SEQBankPlacement newBank(bankID++);

            // 如果该 SEQ 是固定的，设置 bank 的固定信息和位置
            if (inst->isFixed())
            {
                newBank.setFixed(true);
                auto [x, y, _] = inst->getLocation(); // 假设 Location 中包含 (x, y, z)
                newBank.setLocation(x, y);
            }

            newBank.addInstance(inst);
            inst->setSEQID(newBank.getBankID());
            seqPlacementMap[newBank.getBankID()] = newBank;
        }
    }
}

void processSEQInstances(const std::map<int, Instance *> &glbInstMapPart, int &bankID)
{
    for (const auto &instPair : glbInstMapPart)
    {
        Instance *inst = instPair.second;

        // 检查是否为 SEQ 类型，如果不是则跳过
        if (inst->getModelName().substr(0, 3) != "SEQ")
        {
            continue;
        }

        bool placed = false;

        {
            // 尝试将当前 SEQ 实例放入现有的 bank，使用锁保护共享访问
            std::lock_guard<std::mutex> lock(seqPlacementMutex);
            for (auto &[_, bank] : seqPlacementMap)
            {
                // 判断是否符合 bank 的约束：检查连接情况并确保数量限制在 8 个以内
                if (bank.getSEQCount() < 8 && bank.addInstance(inst))
                {
                    placed = true;
                    break;
                }
            }
        }

        // 如果没有找到合适的 Bank，新建一个 Bank 并添加
        if (!placed)
        {
            SEQBankPlacement newBank(bankID++);

            // 如果该 SEQ 是固定的，设置 bank 的固定信息和位置
            if (inst->isFixed())
            {
                newBank.setFixed(true);
                auto [x, y, _] = inst->getLocation(); // 假设 Location 中包含 (x, y, z)
                newBank.setLocation(x, y);
            }

            // 添加新 Bank
            {
                std::lock_guard<std::mutex> lock(seqPlacementMutex);
                newBank.addInstance(inst);
                seqPlacementMap[newBank.getBankID()] = newBank;
            }
        }
    }
}

void initializeSEQPlacementMap_duo(const std::map<int, Instance *> &glbInstMap)
{
    int bankID = 0;
    int numThreads = 8;
    std::vector<std::thread> threads;
    int partSize = glbInstMap.size() / numThreads;

    auto it = glbInstMap.begin();
    for (int i = 0; i < numThreads; ++i)
    {
        // 每个线程处理 glbInstMap 的一部分
        std::map<int, Instance *> glbInstMapPart;
        for (int j = 0; j < partSize && it != glbInstMap.end(); ++j, ++it)
        {
            glbInstMapPart[it->first] = it->second;
        }
        // 启动线程
        threads.emplace_back(processSEQInstances, std::cref(glbInstMapPart), std::ref(bankID));
    }

    // 等待所有线程完成
    for (auto &thread : threads)
    {
        thread.join();
    }
}

// 为 PLB 组生成初始布局
void initializePLBGroupLocations(std::unordered_map<int, PLBPlacement> &plbPlacementMap)
{
    for (auto &entry : plbPlacementMap)
    {
        PLBPlacement &plbGroup = entry.second;
        if (plbGroup.getFixed())
        {
            // // 如果组中有固定元素，直接获取其位置
            // const auto& lutGroups = plbGroup.getLUTGroups();
            // if (!lutGroups.empty() && !lutGroups[0].empty()) {
            //     auto fixedLocation = (*lutGroups[0].begin())->getLocation();
            //     plbGroup.setPLBLocation({std::get<0>(fixedLocation), std::get<1>(fixedLocation)});
            // }
            continue; // 如果固定则跳过，因为固定的PLB组都已经有位置了
        }
        else
        {
            // 计算组内 LUT 的位置几何中心
            int sumX = 0, sumY = 0, count = 0;
            for (const auto &lutGroup : plbGroup.getLUTGroups())
            {
                for (const auto &lut : lutGroup)
                {
                    auto loc = lut->getLocation();
                    sumX += std::get<0>(loc);
                    sumY += std::get<1>(loc);
                    count++;
                }
            }
            if (count > 0)
            {
                plbGroup.setPLBLocation({sumX / count, sumY / count});
            }
        }
    }
    updateLUTLocations(plbPlacementMap);
}

// 根据PLB组的坐标更新其内部LUT的坐标并给予新的合法的z编号
void updateLUTLocations(std::unordered_map<int, PLBPlacement> &plbPlacementMap)
{
    for (auto &entry : plbPlacementMap)
    {
        PLBPlacement &plbGroup = entry.second;
        if (plbGroup.getFixed())
        {
            continue;
        }
        const auto &lutGroups = plbGroup.getLUTGroups();

        // 获取 PLB 的位置
        auto plbLocation = plbGroup.getLocation();
        int baseX = std::get<0>(plbLocation);
        int baseY = std::get<1>(plbLocation);

        // 更新 LUT 组的坐标
        int zIndex = 0; // z 编号从 0 开始
        for (const auto &lutGroup : lutGroups)
        {
            for (const auto &lut : lutGroup)
            {
                // 设置 LUT 的新位置
                std::tuple<int, int, int> newLocation = std::make_tuple(baseX, baseY, zIndex);
                lut->setLocation(newLocation);
            }
            zIndex++; // 更新 z 编号
        }
    }
}

// 为 SEQ 组生成初始布局
void initializeSEQGroupLocations(std::unordered_map<int, SEQBankPlacement> &seqPlacementMap)
{
    for (auto &entry : seqPlacementMap)
    {
        SEQBankPlacement &seqGroup = entry.second;
        if (seqGroup.getFixed())
        {
            continue;
        }
        else
        {
            // 计算组内 SEQ 元素的位置几何中心
            int sumX = 0, sumY = 0, count = 0;
            for (const auto &seq : seqGroup.getSEQInstances())
            {
                auto loc = seq->getLocation();
                sumX += std::get<0>(loc);
                sumY += std::get<1>(loc);
                count++;
            }
            if (count > 0)
            {
                seqGroup.setLocation(sumX / count, sumY / count);
            }
        }
    }
    updateSEQLocations(seqPlacementMap);
}
// 将同一个SEQ组下的seq坐标改为第一个seq的坐标并且重新编号
void updateSEQLocations(std::unordered_map<int, SEQBankPlacement> &seqBankMap)
{
    for (auto &pair : seqBankMap)
    {
        SEQBankPlacement &seqBank = pair.second;
        auto inst = *seqBank.getSEQInstances().begin();
        auto newlocation = inst->getLocation();
        // 获取该SEQ Bank的当前坐标
        int x = std::get<0>(newlocation);
        int y = std::get<1>(newlocation);
        // auto [x, y] = seqBank.getLocation();
        seqBank.setLocation(x, y);
        // 获取该SEQ Bank内的所有SEQ实例
        const auto &seqInstances = seqBank.getSEQInstances();

        // 更新每个SEQ实例的坐标
        int z = 0; // 从0开始编号
        for (Instance *seq : seqInstances)
        {
            // 设置SEQ实例的新位置，z坐标使用当前的编号
            seq->setLocation(std::make_tuple(x, y, z));
            z++; // 更新z编号
        }
        int dummy = 0;
    }
}

bool updateInstancesToTiles()
{
    // 清除所有 tile 中的 LUT 和 SEQ 实例
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {
            Tile *tile = chip.getTile(i, j);
            tile->clearLUTOptimizedInstances(); // 清理 LUT 类型的实例
        }
    }

    // 第一遍：遍历 plbGroups，放置固定实例
    for (const auto &plbGroupPair : plbGroups)
    {
        int plbGroupID = plbGroupPair.first;
        const auto &lutGroupSet = plbGroupPair.second;
        // 获取第一个 lutGroupSet 的迭代器
        auto lutGroupIt = lutGroupSet.begin();
        Instance *firstLUT;
        // 检查 lutGroupSet 是否为空，防止访问越界
        if (lutGroupIt != lutGroupSet.end())
        {
            // 获取第一个 lutGroup 的第一个元素的迭代器
            auto firstLUTIt = lutGroupIt->begin();

            // 检查 lutGroup 是否为空
            if (firstLUTIt != lutGroupIt->end())
            {
                firstLUT = *firstLUTIt;
            }
        }
        if (!firstLUT->isFixed())
        {
            continue;
        }
        auto newLocation = firstLUT->getLocation();
        Tile *tilePtr = chip.getTile(std::get<0>(newLocation), std::get<1>(newLocation));
        // 检查 Tile 资源是否足够
        if (tilePtr && lutGroupSet.size() <= tilePtr->getLUTCount())
        {
            for (const auto &lutGroup : lutGroupSet)
            {
                for (Instance *instance : lutGroup)
                {
                    if (!instance->isLUTInitial())
                    {
                        auto sitelocation = instance->getLocation();
                        int instID = instance->getInstID();
                        if (tilePtr->addInstance(instID, std::get<2>(sitelocation), instance->getModelName(), false))
                        {
                            instance->setLUTInitial(true);
                        }
                        else
                        {
                            std::cout << "Error: Failed to add fixed instance " << instance->getInstanceName()
                                      << " to tile at " << std::get<0>(newLocation) << ", "
                                      << std::get<1>(newLocation) << std::endl;
                            return false;
                        }
                    }
                }
            }
        }
    }
    //-------------------------------------------------------------------------------
    // 创建一个向量存储未固定的PLB组
    std::vector<std::set<std::set<Instance *>>> nonFixedPLBGrouptList;

    // 第二遍：遍历 plbGroups，挑选未固定的PLB组
    for (const auto &plbGroupPair : plbGroups)
    {
        int plbGroupID = plbGroupPair.first;
        const auto &lutGroupSet = plbGroupPair.second;

        // 检查第一个 LUT 组的第一个 LUT 是否未固定
        if (!lutGroupSet.empty() && !lutGroupSet.begin()->empty())
        {
            Instance *firstLUT = *lutGroupSet.begin()->begin();
            if (!firstLUT->isFixed())
            {
                nonFixedPLBGrouptList.emplace_back(lutGroupSet);
            }
        }
    }
    // sortPLBGrouptList(nonFixedPLBGrouptList); // 排序
    for (const auto &lutGroupSet : nonFixedPLBGrouptList)
    {
        Tile *tilePtr;
        std::tuple<int, int, int> newLocation;
        std::set<int> availableSites = {0, 1, 2, 3, 4, 5, 6, 7};
        if (!lutGroupSet.empty()) // 确保 lutGroupSet 非空
        {
            // 获取第一个 LUT 组
            const auto &firstLUTGroup = *lutGroupSet.begin();
            if (!firstLUTGroup.empty()) // 确保第一个 LUT 组非空
            {
                // 获取第一个 LUT
                Instance *firstLUT = *firstLUTGroup.begin();
                // 获取其坐标
                newLocation = firstLUT->getLocation();
                tilePtr = chip.getTile(std::get<0>(newLocation), std::get<1>(newLocation));
            }
        }

        const auto &oneLUTGroup = *lutGroupSet.begin();
        int lutGroupNum = oneLUTGroup.size();
        if (lutGroupNum == 2)
        {
            // tilePtr有定义且有足够的资源放下lutGroupSet
            if (tilePtr && lutGroupSet.size() <= tilePtr->getLUTCount())
            {
                // std::set<int> availableSites = {0, 1, 2, 3, 4, 5, 6, 7};
                // 获取 "LUT" 类型的 slotArr 指针
                slotArr *slots = tilePtr->getInstanceByType("LUT");
                if (slots != nullptr)
                {
                    // 移除已使用的编号
                    for (size_t i = 0; i < slots->size(); ++i)
                    {
                        if (!(*slots)[i]->getOptimizedInstances().empty())
                        {
                            availableSites.erase(i);
                        }
                    }
                }
                for (const auto &lutGroup : lutGroupSet)
                {
                    int siteIndex = *availableSites.begin();
                    for (Instance *instance : lutGroup)
                    {
                        instance->setLocation(std::make_tuple(std::get<0>(newLocation), std::get<1>(newLocation), siteIndex));
                        int instID = instance->getInstID();
                        if (tilePtr->addInstance(instID, siteIndex, instance->getModelName(), false))
                        {
                            instance->setLUTInitial(true);
                            availableSites.erase(siteIndex);
                        }
                        else
                        {
                            std::cout << "Error: Failed to add non-fixed instance " << instance->getInstanceName()
                                      << " to tile at " << std::get<0>(newLocation) << ", "
                                      << std::get<1>(newLocation) << std::endl;
                            return false;
                        }
                    }
                }
            }
            else
            {
                std::set<int> availableSitesInNewTile = {0, 1, 2, 3, 4, 5, 6, 7};
                bool isfinished = false;
                auto neighbors = getNeighborTile(std::get<0>(newLocation), std::get<1>(newLocation)); // 找下一个tile位置
                while (!isfinished)
                {
                    bool placed = false; // 标记是否成功放置

                    int neighborX = std::get<0>(neighbors);
                    int neighborY = std::get<1>(neighbors);
                    tilePtr = chip.getTile(neighborX, neighborY);

                    // 检查相邻 tile 是否有效并且有足够的资源
                    if (tilePtr && lutGroupSet.size() <= tilePtr->getLUTCount() && tilePtr->getTileTypes().count("PLB"))
                    {
                        isfinished = true;
                        // 获取 "LUT" 类型的 slotArr 指针
                        slotArr *slots = tilePtr->getInstanceByType("LUT");
                        if (slots != nullptr)
                        {
                            // 移除已使用的编号
                            for (size_t i = 0; i < slots->size(); ++i)
                            {
                                if (!(*slots)[i]->getOptimizedInstances().empty())
                                {
                                    availableSitesInNewTile.erase(i);
                                }
                            }
                        }
                        for (const auto &lutGroup : lutGroupSet)
                        {
                            for (Instance *instance : lutGroup)
                            {
                                int siteIndex = *availableSitesInNewTile.begin();
                                instance->setLocation(std::make_tuple(neighborX, neighborY, siteIndex));
                                int instID = instance->getInstID();
                                if (tilePtr->addInstance(instID, siteIndex, instance->getModelName(), false))
                                {
                                    instance->setLUTInitial(true);
                                }
                                else
                                {
                                    std::cout << "Error: Failed to add non-fixed instance " << instance->getInstanceName()
                                              << " to tile at " << std::get<0>(newLocation) << ", "
                                              << std::get<1>(newLocation) << std::endl;
                                    return false;
                                }
                            }
                        }
                    }
                    else
                    {
                        neighbors = getNeighborTile(neighborX, neighborY);
                        int dummy = 0;
                    }
                }
            }
        }
        if (lutGroupNum == 1)
        {
            Instance *oneLUT = *oneLUTGroup.begin();
            // tilePtr有定义且有足够的资源放下lutGroupSet
            int x = std::get<0>(newLocation);
            int y = std::get<1>(newLocation);
            int siteIndex = -1;
            if (tilePtr && isValid(false, x, y, siteIndex, oneLUT))
            {
                for (const auto &lutGroup : lutGroupSet)
                {
                    oneLUT->setLocation(std::make_tuple(x, y, siteIndex));
                    int instID = oneLUT->getInstID();
                    if (tilePtr->addInstance(instID, siteIndex, oneLUT->getModelName(), false))
                    {
                        oneLUT->setLUTInitial(true);
                        availableSites.erase(siteIndex);
                    }
                    else
                    {
                        std::cout << "Error: Failed to add non-fixed instance " << oneLUT->getInstanceName()
                                  << " to tile at " << std::get<0>(newLocation) << ", "
                                  << std::get<1>(newLocation) << std::endl;
                        return false;
                    }
                }
            }
            else
            {
                bool isfinished = false;
                auto neighbors = getNeighborTile(std::get<0>(newLocation), std::get<1>(newLocation)); // 找下一个tile位置
                while (!isfinished)
                {
                    bool placed = false; // 标记是否成功放置

                    int neighborX = std::get<0>(neighbors);
                    int neighborY = std::get<1>(neighbors);
                    tilePtr = chip.getTile(neighborX, neighborY);

                    // 检查相邻 tile 是否有效并且有足够的资源
                    if (tilePtr && isValid(false, neighborX, neighborY, siteIndex, oneLUT) && tilePtr->getTileTypes().count("PLB"))
                    {
                        isfinished = true;
                        oneLUT->setLocation(std::make_tuple(neighborX, neighborY, siteIndex));
                        int instID = oneLUT->getInstID();
                        if (tilePtr->addInstance(instID, siteIndex, oneLUT->getModelName(), false))
                        {
                            oneLUT->setLUTInitial(true);
                        }
                        else
                        {
                            std::cout << "Error: Failed to add non-fixed instance " << oneLUT->getInstanceName()
                                      << " to tile at " << std::get<0>(newLocation) << ", "
                                      << std::get<1>(newLocation) << std::endl;
                            return false;
                        }
                    }
                    else
                    {
                        neighbors = getNeighborTile(neighborX, neighborY);
                        int dummy = 0;
                    }
                }
            }
        }
    }
    //-------------------------------------------------------------------------------

    // 将seq放置在tile上，并进行合法化处理
    for (const auto &[bankID, bank] : seqPlacementMap)
    {
        bool isSeqPlaceFinished = false;
        // 获取 bank 的位置
        auto [x, y] = bank.getLocation(); // 假设 getLocation() 返回 (x, y)
        while (!isSeqPlaceFinished)
        {
            // 获取相应的 Tile
            Tile *tilePtr = chip.getTile(x, y);
            if (tilePtr == nullptr)
            {
                std::cout << "Error: Tile at (" << x << ", " << y << ") is invalid." << std::endl;
                continue; // 跳过无效的 Tile
            }

            // 检查 Tile 的类型是否为 PLB
            if (!tilePtr->getTileTypes().count("PLB"))
            {
                std::cout << "Error: Tile at (" << x << ", " << y << ") is not a PLB." << std::endl;

                auto neighbors = getNeighborTile(x, y); // 找下一个tile位置
                x = std::get<0>(neighbors);
                y = std::get<1>(neighbors);
                continue; // 跳过非 PLB 类型的 Tile
            }

            std::vector<int> seqBankNum = tilePtr->getSeqInstanceBankNum();

            // 根据seqBankNum来判断能不能放置新的Seq组
            if (seqBankNum.size() == 0)
            {
                // 遍历 bank 中的 SEQ 实例并进行放置
                for (Instance *seqInstance : bank.getSEQInstances()) // 假设 getInstances() 返回 SEQ 实例的集合
                {
                    auto newLocation = seqInstance->getLocation();
                    int siteIndex = std::get<2>(newLocation); // 获取坐标中的 siteIndex

                    // 尝试将 SEQ 实例放置到 Tile 中
                    int instID = seqInstance->getInstID();

                    if (!tilePtr->addInstance(instID, siteIndex, seqInstance->getModelName(), false))
                    {
                        std::cout << "Error: Failed to add SEQ instance " << seqInstance->getInstanceName()
                                  << " to tile at (" << x << ", " << y << ")." << std::endl;
                        return false; // 返回错误
                    }

                    std::tuple<int, int, int> seqLocation = {x, y, siteIndex};
                    seqInstance->setLocation(seqLocation);
                    seqInstance->setLUTInitial(true); // 标记为已放置
                }
                isSeqPlaceFinished = true;
            }
            if (seqBankNum.size() == 1)
            {
                if (seqBankNum[0] == 1)
                {
                    // 遍历 bank 中的 SEQ 实例并进行放置
                    for (Instance *seqInstance : bank.getSEQInstances()) // 假设 getInstances() 返回 SEQ 实例的集合
                    {
                        auto newLocation = seqInstance->getLocation();
                        int siteIndex = std::get<2>(newLocation); // 获取坐标中的 siteIndex

                        // 尝试将 SEQ 实例放置到 Tile 中
                        int instID = seqInstance->getInstID();

                        if (!tilePtr->addInstance(instID, siteIndex, seqInstance->getModelName(), false))
                        {
                            std::cout << "Error: Failed to add SEQ instance " << seqInstance->getInstanceName()
                                      << " to tile at (" << x << ", " << y << ")." << std::endl;
                            return false; // 返回错误
                        }

                        std::tuple<int, int, int> seqLocation = {x, y, siteIndex};
                        seqInstance->setLocation(seqLocation);
                        seqInstance->setLUTInitial(true); // 标记为已放置
                    }
                    isSeqPlaceFinished = true;
                }
                if (seqBankNum[0] == 0)
                {
                    // 遍历 bank 中的 SEQ 实例并进行放置
                    for (Instance *seqInstance : bank.getSEQInstances()) // 假设 getInstances() 返回 SEQ 实例的集合
                    {
                        auto newLocation = seqInstance->getLocation();
                        int siteIndex = std::get<2>(newLocation); // 获取坐标中的 siteIndex
                        siteIndex += 8;
                        // 尝试将 SEQ 实例放置到 Tile 中
                        int instID = seqInstance->getInstID();

                        if (!tilePtr->addInstance(instID, siteIndex, seqInstance->getModelName(), false))
                        {
                            std::cout << "Error: Failed to add SEQ instance " << seqInstance->getInstanceName()
                                      << " to tile at (" << x << ", " << y << ")." << std::endl;
                            return false; // 返回错误
                        }
                        std::tuple<int, int, int> seqLocation = {x, y, siteIndex};
                        seqInstance->setLocation(seqLocation);
                        seqInstance->setLUTInitial(true); // 标记为已放置
                    }
                    isSeqPlaceFinished = true;
                }
            }
            if (seqBankNum.size() == 2)
            {
                auto neighbors = getNeighborTile(x, y); // 找下一个tile位置
                x = std::get<0>(neighbors);
                y = std::get<1>(neighbors);
            }
        }
    }
    return true; // 返回成功
}

// 比对PLB中的LUT的信息
void printPLBInformation()
{
    if (false)
    {
        // 遍历每个 PLB 组
        // 检测数据
        int totalLUTCOUNT = 0;
        for (const auto &instpair : glbInstMap)
        {
            auto inst = instpair.second;
            if (inst->getModelName().substr(0, 3) != "LUT")
            {
                continue;
            }
            if (inst->getPLBGroupID() != -1)
            {
                totalLUTCOUNT++;
            }
        }
        std::cout << "globalinstancemap 中 plbGroups 检测到的 LUT的数目 : " << totalLUTCOUNT << std::endl;
        int totalLUTCount = 0;
        for (const auto &plbPair : plbGroups)
        {
            if (plbPair.first == 922)
            {
                int dummy = 0;
            }

            const auto &lutGroupSet = plbPair.second;
            int a = 0;
            // 遍历每个 LUT 组
            for (const auto &lutGroup : lutGroupSet)
            {
                totalLUTCount += lutGroup.size(); // 计算当前 LUT 组的大小
                a += lutGroup.size();
            }
            // std::cout << a << std::endl;
        }
        std::cout << "实际的 plbGroups 中LUT的数目 : " << totalLUTCount << std::endl;
        std::cout << "plbGroups数目 : " << plbGroups.size() << std::endl;
    }
}

// 打印instance信息
void printInstanceInformation()
{
    if (true)
    {
        int totalLUTmatchedNum = 0;
        for (auto &lutGroup : lutGroups)
        {
            
            if (lutGroup.second.size() == 2)
            {
                totalLUTmatchedNum++;
            }
        }
        std::cout << lineBreaker << std::endl;
        std::cout << "匹配的LUT组数目 : " << totalLUTmatchedNum << std::endl;
        std::cout << lineBreaker << std::endl;
    }
    
    if (false)
    {
        int totalFixedInstance = 0;
        int totalSeqMatchedInstance = 0;
        // 遍历 glbInstMap 组
        for (auto &inst : glbInstMap)
        {
            Instance *instance = inst.second;
            if (instance->isFixed())
            {
                totalFixedInstance++;
            }
            if (instance->getSEQID() != -1)
            {
                totalSeqMatchedInstance++;
            }
        }
        std::cout << lineBreaker << std::endl;
        std::cout << "固定的Instance数目 : " << totalFixedInstance << std::endl;
        std::cout << "匹配完成的Seq数目 : " << totalSeqMatchedInstance << std::endl;
        std::cout << "Seq组的数目 : " << seqPlacementMap.size() << std::endl;
        std::cout << lineBreaker << std::endl;
    }
}

bool compareOuterSets(const std::set<std::set<Instance *>> &a, const std::set<std::set<Instance *>> &b)
{
    return a.size() > b.size(); // 根据外层 set 的大小比较
}

void sortPLBGrouptList(std::vector<std::set<std::set<Instance *>>> &nonFixedPLBGrouptList)
{
    std::sort(nonFixedPLBGrouptList.begin(), nonFixedPLBGrouptList.end(), compareOuterSets);
}

// 获取上一个或下一个相邻的 Tile 位置，返回单个位置
std::tuple<int, int> getNeighborTile(int x, int y)
{
    // 保存原始输入以便必要时恢复
    int originalX = x;
    int originalY = y;

    // 尝试 y + 1
    if (y + 1 <= 299)
    {
        return std::make_tuple(x, y + 1);
    }
    else
    {
        // y 超出范围，将 y 设为 0 并尝试 x + 1
        y = 0;
        if (x + 1 <= 149)
        {
            return std::make_tuple(x + 1, y);
        }
        else
        {
            // x + 1 也超出范围，恢复到原位置并尝试 y - 1
            x = originalX;
            y = originalY;

            if (y - 1 >= 0)
            {
                return std::make_tuple(x, y - 1);
            }
            else
            {
                // y - 1 超出范围，尝试 x - 1
                y = 299;
                if (x - 1 >= 0)
                {
                    return std::make_tuple(x - 1, y);
                }
                else
                {
                    // x - 1 超出范围，返回无效位置
                    return std::make_tuple(-1, -1);
                }
            }
        }
    }
}

int calculateTwoInstanceWireLength(Instance *inst1, Instance *inst2, bool isBaseLine)
{
    int totalWireLength = 0;
    if (isBaseLine)
    {
        std::tuple<int, int, int> loc1 = inst1->getBaseLocation();
        int x1 = std::get<0>(loc1);
        int y1 = std::get<0>(loc1);
        std::tuple<int, int, int> loc2 = inst1->getBaseLocation();
        int x2 = std::get<0>(loc2);
        int y2 = std::get<0>(loc2);
        totalWireLength = abs(x1 - x2) + abs(y1 - y2);
    }
    else
    {
        std::tuple<int, int, int> loc1 = inst1->getLocation();
        int x1 = std::get<0>(loc1);
        int y1 = std::get<0>(loc1);
        std::tuple<int, int, int> loc2 = inst1->getLocation();
        int x2 = std::get<0>(loc2);
        int y2 = std::get<0>(loc2);
        totalWireLength = abs(x1 - x2) + abs(y1 - y2);
    }

    return totalWireLength;
}