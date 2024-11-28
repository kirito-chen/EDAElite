#include <iomanip>
// #include "legal.h"
#include "object.h"
#include "method.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <future>
#include "wirelength.h"

#include <thread>
#include <mutex>

bool isLUTType(const std::string &modelName);

// 参数设置，线长限制和共享网络限制
#define WIRELIMIT 2
#define SHARENETCOUNT 1

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
std::mutex glbPackNetMapMutex;       // 添加 glbPackNetMapMutex 变量，用于保护 glbPackNetMap 的访问
std::mutex oldNetIDMutex;            // 用于保护 oldNetID2newNetID 的互斥锁

// 锁保护共享数据
std::mutex unassignedMutex;
std::mutex plbGroupsMutex;

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
        if (inst->isOriginalFixed())
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
    // int wireLimit = 2; // 线长限制，默认为2
    while (start < end)
    {
        int bestMatchedLUTID = -1;
        int maxSharedNets = -1;

        int currentLUTID = unmatchedLUTs[start];
        Instance *currentLUT = glbInstMap[currentLUTID];

        int inputPinNum = currentLUT->getUsedNumInpins();
        {
            std::lock_guard<std::mutex> lock(lutMutex);
            if (matchedLUTs.find(currentLUTID) != matchedLUTs.end() && currentLUT->getMatchedLUTID() != -1)
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
                if (twoinstwirelength >= WIRELIMIT)
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

                // // 完全匹配模式
                // if (sharedNetCount > maxSharedNets && totalInpins == inputPinNum && currentLUTNets.size() == otherLUTNets.size())
                // {
                //     maxSharedNets = sharedNetCount;
                //     bestMatchedLUTID = otherLUTID;
                // }

                // 最大匹配模式
                if (sharedNetCount > maxSharedNets && totalInpins <= 6 && sharedNetCount > 0)
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

                // 添加matchedID
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
        populateLUTGroups(glbInstMap); // LUT组匹配,将剩余未加入LUT组的LUT单独加入新的LUT组——已确定正确
        refreshLUTGroups(lutGroups);   // 这里会根据LUT组其中一个固定的位置修改另一个未固定的LUT位置并且将其固定
        // matchFixedLUTGroupsToPLB(lutGroups, plbGroups); // PLB打包，将LUT组打包成PLB组
        matchLUTGroupsToPLB(lutGroups, plbGroups); // PLB打包，将LUT组打包成PLB组
        updatePLBLocations(plbGroups);             // 更新分配PLB组内部LUT的位置和编号
    }
    // buildHPLB(); // 建立HPLB
    packLUTtoHPLB(); // 建立只有LUT的HPLB
    // legalCheck();
    // reportWirelength();
    // printInstanceInformation();
    // if (isSeqPack)
    // {
    //     initializeSEQPlacementMap(glbInstMap);
    //     updateSEQLocations(seqPlacementMap);
    // }
    // updateInstancesToTiles(isSeqPack); // 根据打包情况生成新的初始布局
    // reportWirelength();

    // initialGlbPackInstMap(isSeqPack); // 初始化 glbPackInstMap

    // 获取起始时间点
    auto start = std::chrono::high_resolution_clock::now();

    // 执行要测量的函数
    // initialGlbPackNetMap();
    // initialGlbPackNetMap_Jiu();

    // 获取结束时间点
    auto end = std::chrono::high_resolution_clock::now();
    // 计算耗时（以毫秒为单位）
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Execution time: " << elapsed.count() << " ms" << std::endl;
    std::cout << lineBreaker << std::endl;
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
    std::cout << "未分配PLBGroupID的组的数目 : " << unassignedLUTGroups.size() << std::endl;

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

        // 如果当前的 LUT 组不是固定的，跳过。这是为了先处理固定的 LUT 组
        if (!currentFirstInstance->isFixed())
        {
            unmatchedLUTGroups.erase(currentGroupID);
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
    std::cout << "未分配PLBGroupID的组的数目 : " << unassignedLUTGroups.size() << std::endl;

    // 为未分配的LUT组创建新的PLB组，根据最大共享网络进行PLB组的匹配
    while (!unassignedLUTGroups.empty())
    {
        int newPLBID = plbGroups.size();           // 新的 PLB ID
        std::set<std::set<Instance *>> currentPLB; // 当前 PLB 内的 LUT 组集合
        std::unordered_set<int> currentPLBNets;    // 当前 PLB 的网络集合
        int currentGroupCount = 0;                 // 当前 PLB 中的 LUT 组数量

        // 从未分配的LUT组中选择初始组
        int initialGroupID = *unassignedLUTGroups.begin();
        Instance *current_first_inst = *(lutGroups[initialGroupID]).begin();
        auto current_loc = current_first_inst->getLocation();
        int current_inst_x = std::get<0>(current_loc);
        int current_inst_y = std::get<1>(current_loc);
        int current_inst_z = std::get<2>(current_loc);
        currentPLB.insert(lutGroups[initialGroupID]);    // 将初始组添加到当前 PLB
        currentPLBNets = lutGroupNetMap[initialGroupID]; // 初始组的网络集合作为当前 PLB 的网络集合
        unassignedLUTGroups.erase(initialGroupID);       // 从未分配集合中移除
        ++currentGroupCount;

        // 按最大共享网络扩展当前PLB
        while (currentGroupCount < maxGroupCount && !unassignedLUTGroups.empty())
        {
            int bestGroupID = -1;   // 最佳组的ID
            int maxSharedNets = -1; // 最大共享网络数

            // 查找当前PLB网络集合相关联的LUT组
            std::unordered_set<int> candidateGroups;
            for (int netID : currentPLBNets)
            {
                if (netLUTGroupMap.count(netID))
                {
                    const auto &relatedGroups = netLUTGroupMap[netID];
                    for (int groupID : relatedGroups)
                    {
                        // 只考虑未分配的组
                        if (unassignedLUTGroups.count(groupID))
                        {
                            candidateGroups.insert(groupID);
                        }
                    }
                }
            }

            // 遍历候选的LUT组，选择共享网络最多的组
            for (int groupID : candidateGroups)
            {
                const auto &groupNets = lutGroupNetMap[groupID];
                int sharedNetCount = 0;

                // 计算与当前 PLB 的共享网络数
                for (int net : groupNets)
                {
                    if (currentPLBNets.count(net))
                    {
                        ++sharedNetCount;
                    }
                }

                // 更新最大共享网络数和最佳组ID
                if (sharedNetCount > maxSharedNets)
                {
                    maxSharedNets = sharedNetCount;
                    bestGroupID = groupID;
                }
            }

            // 如果找到最佳组，将其添加到当前 PLB
            if (bestGroupID != -1)
            {
                currentPLB.insert(lutGroups[bestGroupID]); // 将最佳组加入当前 PLB
                currentPLBNets.insert(lutGroupNetMap[bestGroupID].begin(),
                                      lutGroupNetMap[bestGroupID].end()); // 更新当前 PLB 的网络集合
                unassignedLUTGroups.erase(bestGroupID);                   // 从未分配集合中移除
                ++currentGroupCount;
            }
            else
            {
                // 没有更多可以添加的组
                break;
            }
        }

        // 添加同 PLB 内部的未被分配的 LUT_pair
        auto neighbor_tile_list = getNeighborTiles(current_inst_x, current_inst_y, 1);
        for (auto list_xy : neighbor_tile_list)
        {
            Tile *tile_current = chip.getTile(std::get<0>(list_xy), std::get<1>(list_xy));
            int add_inst_count = 0;
            slotArr slots_current = *(tile_current->getInstanceByType("LUT"));
            while (currentPLB.size() < 8 && add_inst_count < slots_current.size())
            {
                std::list<int> instances;
                Slot *slot_current = slots_current[add_inst_count];
                instances = slot_current->getBaselineInstances();
                if (!instances.empty())
                {
                    for (int instance : instances)
                    {
                        Instance *instance_temp = glbInstMap[instance];
                        if (unassignedLUTGroups.find(instance_temp->getLUTSetID()) != unassignedLUTGroups.end())
                        {
                            unassignedLUTGroups.erase(instance_temp->getLUTSetID());

                            currentPLB.insert(lutGroups[instance_temp->getLUTSetID()]);
                            if (currentPLB.size() >= 8)
                            {
                                break;
                            }
                        }
                    }
                }
                add_inst_count++;
            }
        }

        // 将当前 PLB 添加到 PLB 组集合
        plbGroups[newPLBID] = currentPLB;
        // 更新当前 PLB 的 LUT 组 ID
        for (const auto &lutGroup : currentPLB)
        {
            for (Instance *lut : lutGroup)
            {
                lut->setPLBGroupID(newPLBID); // 设置新的 PLB 组 ID
            }
        }
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
        else
        {
            bool oneFlag = true;
            std::tuple<int, int, int> tempLocation;
            int x = -1;
            int y = -1;
            int z = 0;
            for (auto &lutGroup : lutGroupSet)
            {
                for (Instance *lut : lutGroup)
                {

                    if (oneFlag)
                    {
                        tempLocation = lut->getLocation();
                        x = std::get<0>(tempLocation);
                        y = std::get<1>(tempLocation);
                        oneFlag = false;
                    }
                    lut->setLocation(std::make_tuple(x, y, z));
                }
                z++;
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
    int wireLimit = 1; // 默认为1

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

            int x_inst = std::get<0>(inst->getLocation());
            int y_inst = std::get<1>(inst->getLocation());
            int x_bank = std::get<0>(bank.getLocation());
            int y_bank = std::get<1>(bank.getLocation());
            int twoInstWireLength = abs(x_inst - x_bank) + abs(y_inst - y_bank);

            if (twoInstWireLength >= wireLimit)
            {
                continue;
            }

            // 判断是否符合 bank 的约束：检查连接情况并确保数量限制在 8 个以内
            if (bank.getSEQCount() < 8 && bank.addInstance(inst))
            {
                placed = true;
                inst->setSEQID(bank.getBankID());

                int xtmp = std::get<0>(bank.getLocation());
                if (xtmp == -1)
                {
                    bank.setLocation(std::get<0>(inst->getLocation()), std::get<1>(inst->getLocation()));
                }
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
            int xtmp = std::get<0>(newBank.getLocation());
            if (xtmp == -1)
            {
                newBank.setLocation(std::get<0>(inst->getLocation()), std::get<1>(inst->getLocation()));
            }

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
    }
}

bool updateInstancesToTiles(bool isSeqPack)
{
    // 清除所有 tile 中的 LUT 和 SEQ 实例
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {
            Tile *tile = chip.getTile(i, j);
            tile->clearLUTOptimizedInstances(); // 清理 LUT 类型的实例
            if (isSeqPack)
            {
                tile->clearSEQOptimizedInstances(); // 清理 LUT 类型的实例
            }
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
            bool isLeft = true;
            if (x < 75)
            {
                isLeft = false;
            }

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
                auto neighbors = getNeighborTile(std::get<0>(newLocation), std::get<1>(newLocation), isLeft); // 找下一个tile位置
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
                    }
                }
            }
        }
    }
    //-------------------------------------------------------------------------------

    if (isSeqPack)
    {
        // 将seq放置在tile上，并进行合法化处理
        for (const auto &[bankID, bank] : seqPlacementMap)
        {
            bool isSeqPlaceFinished = false;
            // 获取 bank 的位置
            auto [x, y] = bank.getLocation(); // 假设 getLocation() 返回 (x, y)

            // 获取相应的 Tile
            Tile *tilePtr = chip.getTile(x, y);
            std::vector<std::tuple<int, int>> neighbors;
            // 如果tilePtr不是PLB，则生成邻域位置
            if (!tilePtr->getTileTypes().count("PLB"))
            {
                neighbors = getNeighborTiles(x, y, 1); // 找下一个tile位置
            }
            if (tilePtr->addSeqBank(bank))
            {
                isSeqPlaceFinished = true;
            }
            else
            {
                neighbors = getNeighborTiles(x, y, 1); // 找下一个tile位置
            }
            int rangeSide = 2;
            while (!isSeqPlaceFinished)
            {
                if (!neighbors.empty())
                {
                    x = std::get<0>(neighbors[0]);
                    y = std::get<1>(neighbors[0]);
                    tilePtr = chip.getTile(x, y);
                    if (tilePtr == nullptr) // 跳过无效的 Tile
                    {
                        std::cout << "Error: Tile at (" << x << ", " << y << ") is invalid." << std::endl;
                        continue;
                    }

                    if (!tilePtr->addSeqBank(bank))
                    {
                        neighbors.erase(neighbors.begin());
                    }
                    else
                    {
                        isSeqPlaceFinished = true;
                        neighbors.clear();
                    }
                }
                else
                {
                    neighbors = getNeighborTiles(x, y, rangeSide);
                    rangeSide++;
                }
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
        int total_LUT_HPLBinstNum = 0;
        int total_SEQ_HPLBinstNum = 0;
        int totalFixedHPLBinstNum = 0;
        for (auto hplb_pair : globalHPLBMap)
        {
            HPLB *hplb = hplb_pair.second;
            if (hplb->getIsFixed())
            {
                totalFixedHPLBinstNum++;
            }
            for (Instance *instance : hplb->getInstances())
            {
                if (instance->getModelName().substr(0, 3) == "LUT")
                {
                    total_LUT_HPLBinstNum++;
                }
                if (instance->getModelName().substr(0, 3) == "SEQ")
                {
                    total_SEQ_HPLBinstNum++;
                }
            }
        }
        int yuanshi_lut_pair_count = 0;
        int yuanshi_HPLB_count = 0;
        int yuanshi_PLB_fixed_count = 0;
        int yuanshi_HPLB_fixed_count = 0;
        for (int i = 0; i < chip.getNumCol(); i++)
        {
            for (int j = 0; j < chip.getNumRow(); j++)
            {
                Tile *tile = chip.getTile(i, j);
                if (!isPLB[i][j])
                {
                    continue;
                }
                bool hplb_high = false;
                bool hplb_low = false;
                bool hplb_fix_high = false;
                bool hplb_fix_low = false;
                bool is_PLB_fixed = false;

                slotArr lutSlotArr = *(tile->getInstanceByType("LUT"));
                int lutBegin = 0, lutEnd = 0;
                for (int idx = 0; idx < (int)lutSlotArr.size(); idx++)
                {
                    Slot *slot = lutSlotArr[idx];
                    if (slot == nullptr)
                    {
                        continue;
                    }
                    std::list<int> instances;
                    instances = slot->getBaselineInstances();
                    if (instances.size() == 2)
                    {
                        yuanshi_lut_pair_count++;
                    }
                    if (instances.size() > 0 && idx <= 3)
                    {
                        hplb_low = true;
                    }
                    if (instances.size() > 0 && idx > 3)
                    {
                        hplb_high = true;
                    }
                    if (instances.size() > 0 && idx <= 3)
                    {
                        for (auto idx_list : instances)
                        {
                            if (glbInstMap[idx_list]->isFixed())
                            {
                                hplb_fix_low = true;
                            }
                        }
                    }
                    if (instances.size() > 0 && idx > 3)
                    {
                        for (auto idx_list : instances)
                        {
                            if (glbInstMap[idx_list]->isFixed())
                            {
                                hplb_fix_high = true;
                            }
                        }
                    }
                    for (auto idx_list : instances)
                    {
                        if (glbInstMap[idx_list]->isFixed())
                        {
                            is_PLB_fixed = true;
                        }
                    }
                }
                if (hplb_high)
                {
                    yuanshi_HPLB_count++;
                }
                if (hplb_low)
                {
                    yuanshi_HPLB_count++;
                }
                if (hplb_fix_high)
                {
                    yuanshi_HPLB_fixed_count++;
                }
                if (hplb_fix_low)
                {
                    yuanshi_HPLB_fixed_count++;
                }
                if (is_PLB_fixed)
                {
                    yuanshi_PLB_fixed_count++;
                }
            }
        }

        int increaseFixedCount = 0;
        // 遍历 glbInstMap 组
        for (auto &inst : glbInstMap)
        {
            Instance *instance = inst.second;
            if (instance->isFixed() && !instance->isOriginalFixed())
            {
                increaseFixedCount++;
            }
        }
        int totalLUTmatchedNum = 0;
        for (auto &lutGroup : lutGroups)
        {

            if (lutGroup.second.size() == 2)
            {
                totalLUTmatchedNum++;
            }
        }
        std::cout << lineBreaker << std::endl;
        std::cout << "匹配的LUT组数目 :            " << totalLUTmatchedNum << std::endl;
        std::cout << "LUT组数目 :                 " << lutGroups.size() << std::endl;
        std::cout << "seq组的数目 :               " << seqPlacementMap.size() << std::endl;
        std::cout << "glbPackInstMap 数目 :       " << glbPackInstMap.size() << std::endl;
        std::cout << "glbPackNetMap 数目 :        " << glbPackNetMap.size() << std::endl;
        std::cout << "glbNetMap 数目 :            " << glbNetMap.size() << std::endl;
        std::cout << "plbGroups 数目 :            " << plbGroups.size() << std::endl;
        std::cout << "新增 fixed instance 数目 :   " << increaseFixedCount << std::endl;
        std::cout << "globalHPLBMap 数目        :   " << globalHPLBMap.size() << std::endl;
        std::cout << "globalHPLBMap 固定的数目  :   " << totalFixedHPLBinstNum << std::endl;
        std::cout << "HPLB中LUT instance 的数目 :   " << total_LUT_HPLBinstNum << std::endl;
        std::cout << "HPLB中SEQ instance 的数目 :   " << total_SEQ_HPLBinstNum << std::endl;
        std::cout << "plbGroups 的数目          :   " << plbGroups.size() << std::endl;
        std::cout << "初始布局的 LUT_pair 情况   :   " << yuanshi_lut_pair_count << std::endl;
        std::cout << "初始布局的 HPLB 情况   :   " << yuanshi_HPLB_count << std::endl;
        std::cout << "初始布局的 固定的 PLB 情况   :   " << yuanshi_PLB_fixed_count << std::endl;
        std::cout << "初始布局的 固定的 HPLB 情况   :   " << yuanshi_HPLB_fixed_count << std::endl;

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

bool compareOuterSets(const HPLB &a, const HPLB &b)
{
    return a.getInstanceCount() > b.getInstanceCount(); // 根据外层 set 的大小比较
}

void sortPLBGrouptList(std::vector<HPLB *> &nonFixedPLBGrouptList)
{
    // std::sort(nonFixedPLBGrouptList.begin(), nonFixedPLBGrouptList.end(), compareOuterSets);
}

// 获取上一个或下一个相邻的 Tile 位置，返回单个位置
std::tuple<int, int> getNeighborTile(int x, int y, bool isLeft)
{
    // 假设 isPLB 是一个二维数组，表示每个位置是否是 PLB
    while (true)
    {
        if (isLeft)
        {
            // 尝试向左移动
            if (y - 1 >= 0) // 向左移动
            {
                y = y - 1;
                if (isPLB[x][y]) // 如果当前位置是 PLB，返回
                {
                    return std::make_tuple(x, y);
                }
            }
            else // y 超出范围，尝试上一行的最后一个位置
            {
                if (x - 1 >= 0)
                {
                    x = x - 1;
                    y = 299; // 重置到最后一列
                    if (isPLB[x][y])
                    {
                        return std::make_tuple(x, y);
                    }
                }
                else
                {
                    break; // 超出范围
                }
            }
        }
        else
        {
            // 尝试向右移动
            if (y + 1 <= 299) // 向右移动
            {
                y = y + 1;
                if (isPLB[x][y]) // 如果当前位置是 PLB，返回
                {
                    return std::make_tuple(x, y);
                }
            }
            else // y 超出范围，尝试下一行的第一个位置
            {
                if (x + 1 <= 149)
                {
                    x = x + 1;
                    y = 0; // 重置到第一列
                    if (isPLB[x][y])
                    {
                        return std::make_tuple(x, y);
                    }
                }
                else
                {
                    break; // 超出范围
                }
            }
        }
    }

    // 超出范围，返回无效位置
    return std::make_tuple(-1, -1);
}

std::tuple<int, int> getNeighborTile_Jiu(int x, int y, bool isLeft)
{
    if (isLeft)
    {
        // 尝试向左移动
        if (y + 1 <= 299)
        {
            return std::make_tuple(x, y + 1);
        }
        else
        {
            // y 超出范围，重置 y 为最大值，并尝试 x - 1
            if (x - 1 >= 0)
            {
                return std::make_tuple(x - 1, 0);
            }
        }
    }
    else
    {
        // 尝试向右移动
        if (y + 1 <= 299)
        {
            return std::make_tuple(x, y + 1);
        }
        else
        {
            // y 超出范围，重置 y 为 0 并尝试 x + 1
            if (x + 1 <= 149)
            {
                return std::make_tuple(x + 1, y);
            }
        }
    }

    // 超出范围，返回无效位置
    return std::make_tuple(-1, -1);
}

// 获取上一个或下一个相邻的 Tile 位置，返回单个位置
std::vector<std::tuple<int, int>> getNeighborTiles(int x, int y, int rangeDesired)
{
    // 定义矩形框的左下角和右上角
    int xl = x - rangeDesired, yl = y - rangeDesired, xr = x + rangeDesired, yr = y + rangeDesired;
    int numCol = chip.getNumCol() - 1;
    int numRow = chip.getNumRow() - 1;
    // 判断不超出范围
    if (xl < 0)
        xl = 0;
    if (yl < 0)
        yl = 0;
    if (xr > numCol)
        xr = numCol;
    if (yr > numRow)
        yr = numRow;

    // 生成矩形框内的所有坐标
    std::vector<std::tuple<int, int>> coordinates;
    for (int x = xl; x <= xr; ++x)
    {
        for (int y = yl; y <= yr; ++y)
        {
            if (isPLB[x][y])
            {                                                    // 只加入PLB块
                coordinates.emplace_back(std::make_tuple(x, y)); // 将坐标 (x, y) 加入向量
            }
        }
    }
    return coordinates;
}

int calculateTwoInstanceWireLength(Instance *inst1, Instance *inst2, bool isBaseLine)
{
    int totalWireLength = 0;
    if (isBaseLine)
    {
        std::tuple<int, int, int> loc1 = inst1->getBaseLocation();
        int x1 = std::get<0>(loc1);
        int y1 = std::get<1>(loc1);
        std::tuple<int, int, int> loc2 = inst2->getBaseLocation();
        int x2 = std::get<0>(loc2);
        int y2 = std::get<1>(loc2);
        totalWireLength = abs(x1 - x2) + abs(y1 - y2);
    }
    else
    {
        std::tuple<int, int, int> loc1 = inst1->getLocation();
        int x1 = std::get<0>(loc1);
        int y1 = std::get<1>(loc1);
        std::tuple<int, int, int> loc2 = inst2->getLocation();
        int x2 = std::get<0>(loc2);
        int y2 = std::get<1>(loc2);
        totalWireLength = abs(x1 - x2) + abs(y1 - y2);
    }

    return totalWireLength;
}

// 初始化 glbPackInstMap , 在完成LUT与SEQ的打包之后
void initialGlbPackInstMap(bool isSeqPack)
{
    for (auto &entry : glbInstMap)
    {
        int instID = entry.first;
        Instance *instance = entry.second;
        // Instance *newInstance = new Instance();

        // 对 LUT 类型的 instance 处理
        if (instance->getModelName().substr(0, 3) == "LUT" && !instance->isMapMatched())
        {
            glbPackInstMap.insert(std::make_pair(glbPackInstMap.size(), instance));
            instance->setMapMatched(true);
            instance->addMapInstID(instID);
            int matchedID = instance->getMatchedLUTID();
            if (matchedID != -1)
            {
                instance->addMapInstID(matchedID);
                glbInstMap[matchedID]->setMapMatched(true);
                Instance *tmp = glbInstMap[matchedID];
                auto otherInputInstPinVec = tmp->getInpins();
                instance->unionInputPins(otherInputInstPinVec);
                auto otherOutputInstPinVec = tmp->getOutpins();
                instance->unionOutputPins(otherOutputInstPinVec);
            }
            continue;
        }
        if (isSeqPack)
        {
            if (instance->getModelName().substr(0, 3) == "SEQ" && !instance->isMapMatched())
            {
                glbPackInstMap.insert(std::make_pair(glbPackInstMap.size(), instance));

                int seqGroupID = instance->getSEQID();
                if (!seqPlacementMap.empty())
                {
                    std::vector<Instance *> seqVec = seqPlacementMap[seqGroupID].getSEQInstances();
                    for (auto &seq_instance : seqVec)
                    {
                        // 检查指针是否为空，以防止空指针访问
                        if (seq_instance)
                        {
                            // 对 instance 进行所需的修改
                            seq_instance->setMapMatched(true);
                            instance->addMapInstID(seq_instance->getInstID());
                            instance->unionInputPins(seq_instance->getInpins());
                            instance->unionOutputPins(seq_instance->getOutpins());
                        }
                    }
                    continue;
                }
                else
                {
                    instance->setMapMatched(true);
                    instance->addMapInstID(instance->getInstID());
                }
            }
        }

        if (!instance->isMapMatched())
        {

            glbPackInstMap.insert(std::make_pair(glbPackInstMap.size(), instance));
            instance->setMapMatched(true);
            instance->addMapInstID(instID);
        }
    }
}

void initialGlbPackNetMap()
{
    std::cout << " --- 生成新的glbPackNetMap ---" << std::endl;
    int newNetPackID = 0;
    for (auto iter : glbNetMap)
    {
        auto netID = iter.first;
        auto net = iter.second;
        oldNetID2newNetID.insert(std::make_pair(netID, newNetPackID)); // 建立旧netID到新netID的映射
        // 处理InPin
        auto currentInPin = net->getInpin();
        if (currentInPin->getInstanceOwner()->getMapInstID().size() == 0)
        {
            continue;
        }

        Pin *newInPin = new Pin(newNetPackID, currentInPin->getProp(), currentInPin->getTimingCritical(), nullptr);
        newInPin->setInstanceOwner(currentInPin->getInstanceOwner()->getPackInstance());

        Net *newNet = new Net(newNetPackID);
        newNet->setClock(net->isClock());
        newNet->setInpin(newInPin);
        for (auto currentOutPin : net->getOutputPins())
        {
            Pin *newOutPin = new Pin(newNetPackID, currentOutPin->getProp(), currentOutPin->getTimingCritical(), nullptr);
            newOutPin->setInstanceOwner(currentOutPin->getInstanceOwner()->getPackInstance());
            newNet->addPinIfUnique(newOutPin);
            // auto inst = pin->getInstanceOwner();
        }

        glbPackNetMap.insert(std::make_pair(newNetPackID, newNet));
        newNetPackID++;
    }
}

void initialGlbPackNetMap_Jiu()
{
    std::cout << " --- 生成新的glbPackNetMap ---" << std::endl;
    int newNetPackID = 0;
    for (auto iter : glbNetMap)
    {
        auto netID = iter.first;
        auto net = iter.second;
        oldNetID2newNetID.insert(std::make_pair(netID, newNetPackID)); // 建立旧netID到新netID的映射
        // 处理InPin
        auto currentInPin = net->getInpin();
        if (currentInPin->getInstanceOwner()->getMapInstID().size() == 0)
        {
            continue;
        }

        Pin *newInPin = new Pin(newNetPackID, currentInPin->getProp(), currentInPin->getTimingCritical(), nullptr);
        newInPin->setInstanceOwner(currentInPin->getInstanceOwner()->getPackInstance());

        Net *newNet = new Net(newNetPackID);
        newNet->setClock(net->isClock());
        newNet->setInpin(newInPin);
        for (auto currentOutPin : net->getOutputPins())
        {
            Pin *newOutPin = new Pin(newNetPackID, currentOutPin->getProp(), currentOutPin->getTimingCritical(), nullptr);
            newOutPin->setInstanceOwner(currentOutPin->getInstanceOwner()->getPackInstance());
            newNet->addPinIfUnique_Jiu(newOutPin);
            // auto inst = pin->getInstanceOwner();
            int a = 0;
        }

        glbPackNetMap.insert(std::make_pair(newNetPackID, newNet));
        newNetPackID++;
        int a = 0;
    }
}

// 还原最终结果映射
void recoverAllMap(bool isSeqPack)
{
    std::cout << "还原所有映射" << std::endl;
    for (auto inst : glbPackInstMap)
    {
        auto instance = inst.second;
        auto location = instance->getLocation();
        int x = std::get<0>(location);
        int y = std::get<1>(location);
        int z = std::get<2>(location);

        if (isSeqPack)
        {
            if (instance->getModelName().substr(0, 3) == "SEQ")
            {
                auto instVec = instance->getMapInstID();
                if (z == 0)
                {
                    for (int i = 0; i < instVec.size(); i++)
                    {
                        glbInstMap[instVec[i]]->setLocation(std::make_tuple(x, y, i));
                    }
                }
                if (z == 1)
                {
                    for (int i = 0; i < instVec.size(); i++)
                    {
                        glbInstMap[instVec[i]]->setLocation(std::make_tuple(x, y, i + 8));
                    }
                }
            }
            else
            {
                auto instVec = instance->getMapInstID();
                for (int i = 0; i < instVec.size(); i++)
                {
                    glbInstMap[instVec[i]]->setLocation(location);
                }
            }
        }
        else
        {
            auto instVec = instance->getMapInstID();
            for (int i = 0; i < instVec.size(); i++)
            {
                glbInstMap[instVec[i]]->setLocation(location);
            }
        }
    }
}

// 提取node文件名
std::string extractFileName(const std::string &filePath)
{
    size_t pos = filePath.find_last_of("/\\"); // 查找最后一个路径分隔符的位置
    if (pos != std::string::npos)
    {
        return filePath.substr(pos + 1); // 返回文件名
    }
    return filePath; // 若没有分隔符，返回整个路径
}
bool fileExists(const std::string &filePath)
{
    std::ifstream file(filePath);
    return file.good();
}
// 从 JSON 文件读取并解析数据到 NestedMap
NestedMap readJsonFile(const std::string &filename)
{
    NestedMap data;
    std::ifstream inputFile(filename);
    if (!inputFile.is_open())
    {
        std::cerr << "Error opening file for reading." << filename << std::endl;
        exit(1);
    }
    std::string line;
    std::string outerKey, innerKey;
    int value;
    while (std::getline(inputFile, line))
    {
        // 查找外层键（例如："1"）
        if (line.find("\"") != std::string::npos && line.find(": {") != std::string::npos)
        {
            outerKey = line.substr(line.find("\"") + 1, line.rfind("\"") - line.find("\"") - 1);
            data[outerKey] = {};
        }
        // 查找内层键值对（例如："60": 11）
        if (line.find(":") != std::string::npos && line.find("{") == std::string::npos)
        {
            size_t keyStart = line.find("\"") + 1;
            size_t keyEnd = line.find("\"", keyStart);
            innerKey = line.substr(keyStart, keyEnd - keyStart);
            size_t valueStart = line.find(": ", keyEnd) + 2;
            value = std::stoi(line.substr(valueStart));
            data[outerKey][innerKey] = value;
        }
    }
    inputFile.close();
    return data;
}
// 将 NestedMap 写入到 JSON 文件
void writeJsonFile(const std::string &filename, const NestedMap &data)
{
    std::ofstream outputFile(filename);
    if (!outputFile.is_open())
    {
        std::cerr << "Error opening file for writing." << filename << std::endl;
        exit(1);
    }
    outputFile << "{\n";
    for (auto outerIt = data.begin(); outerIt != data.end(); ++outerIt)
    {
        outputFile << "    \"" << outerIt->first << "\": {\n";
        for (auto innerIt = outerIt->second.begin(); innerIt != outerIt->second.end(); ++innerIt)
        {
            outputFile << "        \"" << innerIt->first << "\": " << innerIt->second;
            if (std::next(innerIt) != outerIt->second.end())
                outputFile << ",";
            outputFile << "\n";
        }
        outputFile << "    }";
        if (std::next(outerIt) != data.end())
            outputFile << ",";
        outputFile << "\n";
    }
    outputFile << "}\n";
    outputFile.close();
}

// 如果存在 引脚数 > pinNum的netId 的net则返回true，id存储在 glbBigNet 中
bool findBigNetId(int pinNumLimit)
{
    bool hasBigNet = false;
    glbBigNetPinNum = 0;
    for (const auto &it : glbNetMap)
    {
        int netId = it.first;
        Net *net = it.second;
        if (net->isClock())
        { // 跳过clock 不参与线长计算
            continue;
        }
        int pinNum = 1 + (net->getOutputPins()).size(); //+1是唯一的O_x 也就是这里唯一的inpin
        if (pinNum > pinNumLimit)
        {
            glbBigNet.insert(netId);
            glbBigNetPinNum += pinNum;
        }
    }
    if (glbBigNetPinNum > 0)
        hasBigNet = true;
    return hasBigNet;
}

// 如果存在 引脚数 > pinNum的netId 的net则返回true，id存储在 glbBigNet 中
bool findPackBigNetId(int pinNumLimit)
{
    bool hasBigNet = false;
    glbBigNetPinNum = 0;
    for (const auto &it : glbPackNetMap)
    {
        int netId = it.first;
        Net *net = it.second;
        if (net->isClock())
        { // 跳过clock 不参与线长计算
            continue;
        }
        int pinNum = 1 + (net->getOutputPins()).size(); //+1是唯一的O_x 也就是这里唯一的inpin
        if (pinNum > pinNumLimit)
        {
            glbBigNet.insert(netId);
            glbBigNetPinNum += pinNum;
        }
    }
    if (glbBigNetPinNum > 0)
        hasBigNet = true;
    return hasBigNet;
}

// 建立HPLB映射数据
void buildHPLB()
{
    std::cout << " 建立HPLB映射数据 " << std::endl;
    // 根据 plbGroups 建立全局的HPLB
    for (auto plbgroup : plbGroups)
    {
        int plbID = plbgroup.first;
        auto lutGroups = plbgroup.second;
        auto firstLUTGroup = *lutGroups.begin();
        Instance *firstInstance = *firstLUTGroup.begin();
        bool isFixed = firstInstance->isFixed(); // 根据第一个instance获取固定信息

        std::tuple<int, int, int> location = firstInstance->getLocation(); // 默认位置
        int hplb_X = std::get<0>(location);
        int hplb_Y = std::get<1>(location);
        int hplb_Z = std::get<2>(location);
        // 创建两个 HPLB：一个用于 z <= 3，另一个用于 z > 3
        HPLB *hplbLow = new HPLB(plbID, isFixed, std::make_tuple(-1, -1, 0));  // hplb_Z = 0
        HPLB *hplbHigh = new HPLB(plbID, isFixed, std::make_tuple(-1, -1, 1)); // hplb_Z = 1
        for (const auto &lutGroup : lutGroups)
        {
            for (Instance *instance : lutGroup)
            {
                std::tuple<int, int, int> location = instance->getLocation();
                int hplb_X = std::get<0>(location);
                int hplb_Y = std::get<1>(location);
                int hplb_Z = std::get<2>(location);

                // 根据 z 值将实例添加到对应的 HPLB
                if (hplb_Z > 3)
                {
                    hplbHigh->setLocation(hplb_X, hplb_Y, 1); // 设置位置
                    hplbHigh->addInstance(instance);          // 添加实例
                    instance->setHplbID(plbID * 2 + 1);
                }
                else
                {
                    hplbLow->setLocation(hplb_X, hplb_Y, 0); // 设置位置
                    hplbLow->addInstance(instance);          // 添加实例
                    instance->setHplbID(plbID * 2);
                }
            }
        }
        // 如果 HPLB 有实例，加入到全局 HPLB 集合
        if (hplbLow->getInstanceCount() != 0)
        {
            globalHPLBMap[plbID * 2] = hplbLow; // 为低区设置唯一 ID（例如 plbID * 2）
        }
        if (hplbHigh->getInstanceCount() != 0)
        {
            globalHPLBMap[plbID * 2 + 1] = hplbHigh; // 为高区设置唯一 ID（例如 plbID * 2 + 1）
        }
    }

    // -------将LUT整合到Tile内---------------------------------------------------------
    // 清除所有 tile 中的 LUT 和 SEQ 实例
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {
            Tile *tile = chip.getTile(i, j);
            tile->clearLUTOptimizedInstances(); // 清理 LUT 类型的实例
            // tile->clearSEQOptimizedInstances(); // 清理 SEQ 类型的实例
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
                        }
                    }
                }
            }
        }
    }

    // 创建一个向量存储未固定的HPLB组
    std::vector<HPLB *> nonFixedHPLBGrouptList;

    // 第二遍：遍历 globalHPLBMap ，挑选未固定的HPLB组
    for (const auto &hplbGroupPair : globalHPLBMap)
    {
        int plbGroupID = hplbGroupPair.first;
        const auto &hplbGroupSet = hplbGroupPair.second;
        if (!hplbGroupSet->getIsFixed())
        {
            nonFixedHPLBGrouptList.emplace_back(hplbGroupSet);
        }
    }
    // sortPLBGrouptList(nonFixedHPLBGrouptList); // 排序
    for (const auto &hplbGroupSet : nonFixedHPLBGrouptList)
    {
        Tile *tilePtr;
        std::tuple<int, int, int> newLocation;
        newLocation = hplbGroupSet->getLocation();
        int x_tile = std::get<0>(newLocation);
        int y_tile = std::get<1>(newLocation);
        int z_tile = std::get<2>(newLocation);
        bool isOneFlag = true;
        bool isLeft = true;
        if (x_tile < 75)
        {
            isLeft = false;
        }
        while (isOneFlag)
        {
            tilePtr = chip.getTile(x_tile, y_tile);
            if (tilePtr->isLUTempty(z_tile, false))
            {
                std::set<Instance *> instance_hplb = hplbGroupSet->getInstances();
                for (Instance *instance : instance_hplb)
                {
                    tilePtr->addInstance(instance->getInstID(), std::get<2>(instance->getLocation()), instance->getModelName(), false);
                }
                hplbGroupSet->setLocation(x_tile, y_tile, z_tile);
                isOneFlag = false;
                neighbor_PLB_xy.clear();
            }
            else
            {
                if (z_tile == 0)
                {
                    z_tile = 1;
                }
                else
                {
                    if (!neighbor_PLB_xy.empty())
                    {
                        x_tile = std::get<0>(neighbor_PLB_xy[0]);
                        y_tile = std::get<1>(neighbor_PLB_xy[0]);
                        neighbor_PLB_xy.erase(neighbor_PLB_xy.begin());
                        z_tile = 0;
                    }
                    else
                    {
                        neighbor_PLB_xy = getNeighborTiles(x_tile, y_tile, 2);
                    }
                }
            }
        }
    }
    // ------------------------------------------------

    // 进行SEQ的匹配
    std::unordered_map<int, std::unordered_set<int>> netToSEQMap; // Net -> SEQ 实例映射

    // 遍历所有 instance， 收集SEQ 信息
    for (auto inst_pair : glbInstMap)
    {
        Instance *instance = inst_pair.second;
        if (instance->getModelName().substr(0, 3) == "SEQ") // 假设 Instance 类有 isSEQ 方法
        {
            // 遍历 SEQ 的连接网络，建立映射
            for (int i = 0; i < instance->getNumInpins(); ++i)
            {
                Pin *pin = instance->getInpin(i);
                int netID = pin->getNetID();
                if (netID != -1) // 跳过未连接的网络
                {
                    netToSEQMap[netID].insert(instance->getInstID());
                }
            }
            for (int i = 0; i < instance->getNumOutpins(); ++i)
            {
                Pin *pin = instance->getOutpin(i);
                int netID = pin->getNetID();
                if (netID != -1) // 跳过未连接的网络
                {
                    netToSEQMap[netID].insert(instance->getInstID());
                }
            }
        }
    }
    std::unordered_map<int, std::vector<int>> hplbNetMap; // HPLB_ID -> Net 映射

    // 遍历所有 HPLB
    for (const auto &[hplbID, hplb] : globalHPLBMap)
    {
        std::vector<int> netSet; // 存储当前 HPLB 相关的网络

        // 遍历 HPLB 内部的 instances
        for (Instance *instance : hplb->getInstances())
        {
            // 遍历 instance 的所有 pins
            for (int i = 0; i < instance->getNumInpins(); ++i)
            {
                Pin *pin = instance->getInpin(i);
                int netID = pin->getNetID();
                if (netID != -1) // 跳过未连接的 pin
                {
                    netSet.push_back(netID); // 将网络添加到集合
                }
            }
            for (int i = 0; i < instance->getNumOutpins(); ++i)
            {
                Pin *pin = instance->getOutpin(i);
                int netID = pin->getNetID();
                if (netID != -1) // 跳过未连接的 pin
                {
                    netSet.push_back(netID); // 将网络添加到集合
                }
            }
        }

        // 将 HPLB_ID 和对应的网络集合插入映射
        hplbNetMap[hplbID] = std::move(netSet);
    }

    std::unordered_set<int> assignedSEQ; // 用于记录已分配的 SEQ
    // 遍历每个 HPLB
    for (auto &[hplbID, netSet] : hplbNetMap)
    {
        HPLB *hplb = globalHPLBMap[hplbID];
        std::unordered_map<int, int> seqFrequency; // SEQ -> 出现次数的映射

        // 获取当前tile下的seq集合，添加进入 seqFrequency
        Tile *base_tile = chip.getTile(std::get<0>(hplb->getLocation()), std::get<1>(hplb->getLocation()));
        slotArr temp_slotarr = *(base_tile->getInstanceByType("SEQ"));
        std::list<int> instances_base;
        for (int idx = 0; idx < (int)temp_slotarr.size(); idx++)
        {
            Slot *slot = temp_slotarr[idx];
            instances_base = slot->getBaselineInstances();
        }

        // 遍历 HPLB 的网络集合
        for (int netID : netSet)
        {
            // 查找网络对应的 SEQ
            if (netToSEQMap.count(netID))
            {
                for (int seqID : netToSEQMap[netID])
                {
                    // 如果 SEQ 已经分配，跳过
                    if (assignedSEQ.count(seqID))
                        continue;
                    seqFrequency[seqID]++; // 记录 SEQ 的出现次数
                }
            }
        }

        // 将 SEQ 按出现次数排序
        std::vector<std::pair<int, int>> sortedSEQ(seqFrequency.begin(), seqFrequency.end());
        std::sort(sortedSEQ.begin(), sortedSEQ.end(),
                  [](const auto &a, const auto &b)
                  { return a.second > b.second; });

        // 选取前 8 个 SEQ 并添加到 HPLB
        int addedSEQCount = 0;
        int z_seq = 0;
        for (const auto &[seqID, freq] : sortedSEQ)
        {
            if (addedSEQCount >= 8)
                break;

            Instance *seqInstance = glbInstMap[seqID];
            // 将 SEQ 添加到 HPLB
            if (hplb->addSeqInstance(seqInstance))
            {
                seqInstance->setHplbID(hplbID);
                seqInstance->setLocation(std::make_tuple(std::get<0>(hplb->getLocation()), std::get<1>(hplb->getLocation()), z_seq));
                Tile *tile_temp_addInstance = chip.getTile(std::get<0>(hplb->getLocation()), std::get<1>(hplb->getLocation()));
                if (std::get<2>(hplb->getLocation()) == 0)
                {
                    tile_temp_addInstance->addInstance(seqInstance->getInstID(), z_seq, seqInstance->getModelName(), false);
                }
                else
                {
                    tile_temp_addInstance->addInstance(seqInstance->getInstID(), z_seq + 8, seqInstance->getModelName(), false);
                }

                assignedSEQ.insert(seqID); // 记录为已分配
                ++addedSEQCount;
                z_seq++;
            }
            else
            {
                int dummy = 0;
            }
        }
    }
    // > 进行SEQ的匹配

    // 未被匹配的SEQ就近放置
    std::vector<Instance *> unmatchedSEQ; // 未匹配的SEQ
    // 遍历所有 SEQ
    for (auto &[instID, instance] : glbInstMap)
    {
        if (instance->getModelName().substr(0, 3) == "SEQ" && assignedSEQ.count(instID) == 0)
        {
            unmatchedSEQ.push_back(instance);
        }
    }
    for (size_t i = 0; i < unmatchedSEQ.size(); i++)
    {
        Instance *seqInstance = unmatchedSEQ[i];
        auto unmatchedSeqLoc = unmatchedSEQ[i]->getLocation();
        int temp_x = std::get<0>(unmatchedSeqLoc);
        int temp_y = std::get<1>(unmatchedSeqLoc);
        int temp_z = std::get<2>(unmatchedSeqLoc);
        bool temp_one_flag = true;
        while (temp_one_flag)
        {
            Tile *tile_ptr = chip.getTile(temp_x, temp_y);
            // std::set<std::string> tileTypes = tile_ptr->getTileTypes();
            if (tile_ptr == nullptr || !isPLB[temp_x][temp_y])
            {
                auto tempXY = getNeighborTile(temp_x, temp_y);
                temp_x = std::get<0>(tempXY);
                temp_y = std::get<1>(tempXY);
                continue; // 跳过无效的 Tile
            }

            int z_offset = tile_ptr->findOffset(seqInstance->getModelName(), seqInstance, false);
            if (z_offset != -1)
            {
                tile_ptr->addInstance(seqInstance->getInstID(), z_offset, seqInstance->getModelName(), false);
                seqInstance->setLocation(std::make_tuple(temp_x, temp_y, z_offset));
                temp_one_flag = false;
                neighbor_PLB_xy.clear();
            }
            else
            {
                if (!neighbor_PLB_xy.empty())
                {
                    temp_x = std::get<0>(neighbor_PLB_xy[0]);
                    temp_y = std::get<1>(neighbor_PLB_xy[0]);
                    neighbor_PLB_xy.erase(neighbor_PLB_xy.begin());
                }
                else
                {
                    neighbor_PLB_xy = getNeighborTiles(temp_x, temp_y, 3);
                }
            }
        }
    }

    // 根据Tile重新建立HPLB的全局映射-----------------------------------
    globalHPLBMap.clear();
    int pid_count = 0;
    int total_count = 0;
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {

            Tile *tile = chip.getTile(i, j);

            // std::set<std::string> tileTypes = tile->getTileTypes();
            if (tile == nullptr || !isPLB[i][j])
                continue; // 跳过无效的 Tile

            // 遍历 Tile 中的所有实例
            std::set<Instance *> tileInstances;
            slotArr slotArr = *tile->getInstanceByType("LUT");
            for (Slot *slot : slotArr) // slotArrInstance 是 slotArr 类型的实例
            {
                if (!slot->getOptimizedInstances().empty())
                {
                    auto optimizedInstArr = slot->getOptimizedInstances();
                    for (int value : optimizedInstArr)
                    {
                        tileInstances.insert(glbInstMap[value]);
                    }
                }
            }
            slotArr = *tile->getInstanceByType("SEQ");
            for (Slot *slot : slotArr) // slotArrInstance 是 slotArr 类型的实例
            {
                if (!slot->getOptimizedInstances().empty())
                {
                    auto optimizedInstArr = slot->getOptimizedInstances();
                    for (int value : optimizedInstArr)
                    {
                        tileInstances.insert(glbInstMap[value]);
                    }
                }
            }
            total_count += tileInstances.size();

            if (tileInstances.empty())
                continue; // 如果 Tile 没有实例，跳过

            // 创建低区 HPLB (Z=0)
            HPLB *hplbLow = new HPLB(pid_count, false, std::make_tuple(i, j, 0));
            HPLB *hplbHigh = new HPLB(pid_count + 1, false, std::make_tuple(i, j, 1));
            bool isfixed_hplb = false;
            // 遍历实例，分配到 HPLB 的低区或高区
            for (Instance *instance : tileInstances)
            {
                std::tuple<int, int, int> location = instance->getLocation();
                int z = std::get<2>(location);
                if (instance->isFixed())
                {
                    isfixed_hplb = true;
                }

                if (instance->getModelName().substr(0, 3) == "LUT")
                {
                    if (z > 3) // 判断实例的 Z 坐标
                    {
                        hplbHigh->addInstance(instance);
                        hplbHigh->setIsFixed(isfixed_hplb);
                    }
                    else
                    {
                        hplbLow->addInstance(instance);
                        hplbLow->setIsFixed(isfixed_hplb);
                    }
                }
                if (instance->getModelName().substr(0, 3) == "SEQ")
                {
                    if (z > 7) // 判断实例的 Z 坐标
                    {
                        hplbHigh->addInstance(instance);
                        hplbHigh->setIsFixed(isfixed_hplb);
                    }
                    else
                    {
                        hplbLow->addInstance(instance);
                        hplbLow->setIsFixed(isfixed_hplb);
                    }
                }
            }
            // 如果 HPLB 非空，添加到 globalHPLBMap
            if (hplbLow->getInstanceCount() > 0)
            {
                globalHPLBMap[pid_count * 2] = hplbLow;
            }
            if (hplbHigh->getInstanceCount() > 0)
            {
                globalHPLBMap[pid_count * 2 + 1] = hplbHigh;
            }
            pid_count++;
            int dummy = 0;
        }
    }

    // ----------------------------------------------

    std::cout << netToSEQMap.size() << std::endl;
    std::cout << hplbNetMap.size() << std::endl;
    std::cout << assignedSEQ.size() << std::endl;
    std::cout << unmatchedSEQ.size() << std::endl;
}

void processPLBGroups(
    std::set<int> &unassignedLUTGroups,
    std::unordered_map<int, std::unordered_set<int>> &lutGroupNetMap,
    int maxGroupCount)
{
    while (true)
    {
        int initialGroupID = -1;

        // 获取一个未分配的LUT组
        {
            std::lock_guard<std::mutex> lock(unassignedMutex);
            if (unassignedLUTGroups.empty())
            {
                break; // 没有更多的未分配组
            }
            initialGroupID = *unassignedLUTGroups.begin();
            unassignedLUTGroups.erase(initialGroupID);
        }

        // 创建新的 PLB
        int newPLBID = -1;
        std::set<std::set<Instance *>> currentPLB;
        std::unordered_set<int> currentPLBNets = lutGroupNetMap[initialGroupID];
        currentPLB.insert(lutGroups[initialGroupID]);
        int currentGroupCount = 1;

        // 按最大共享网络扩展当前PLB
        while (currentGroupCount < maxGroupCount)
        {
            int bestGroupID = -1;
            int maxSharedNets = -1;

            {
                std::lock_guard<std::mutex> lock(unassignedMutex);
                for (int groupID : unassignedLUTGroups)
                {
                    const auto &groupNets = lutGroupNetMap[groupID];
                    int sharedNetCount = 0;

                    // 计算共享网络数
                    for (int net : groupNets)
                    {
                        if (currentPLBNets.count(net))
                        {
                            ++sharedNetCount;
                        }
                    }

                    // 更新最大共享网络数和最佳组ID
                    if (sharedNetCount > maxSharedNets)
                    {
                        maxSharedNets = sharedNetCount;
                        bestGroupID = groupID;
                    }
                }

                // 如果找到最佳组，从未分配集合中移除
                if (bestGroupID != -1)
                {
                    unassignedLUTGroups.erase(bestGroupID);
                }
            }

            // 如果没有找到匹配组，结束扩展
            if (bestGroupID == -1)
            {
                break;
            }

            // 将最佳组添加到当前PLB
            currentPLB.insert(lutGroups[bestGroupID]);
            currentPLBNets.insert(lutGroupNetMap[bestGroupID].begin(),
                                  lutGroupNetMap[bestGroupID].end());
            ++currentGroupCount;
        }

        // 更新PLB组集合
        {
            std::lock_guard<std::mutex> lock(plbGroupsMutex);
            newPLBID = plbGroups.size();
            plbGroups[newPLBID] = currentPLB;
        }

        // 更新实例的PLB组ID
        for (const auto &lutGroup : currentPLB)
        {
            for (Instance *lut : lutGroup)
            {
                lut->setPLBGroupID(newPLBID);
            }
        }
    }
}

// 将LUT_pair 打包成为只有LUT的HPLB，SEQ交给SA进行位置调整
void packLUTtoHPLB()
{
    std::cout << " packing LUT_pair into HPLB and update LUTs' location " << std::endl;
    // 根据 plbGroups 建立全局的HPLB
    for (auto plbgroup : plbGroups)
    {
        int plbID = plbgroup.first;
        auto lutGroups = plbgroup.second;
        auto firstLUTGroup = *lutGroups.begin();
        Instance *firstInstance = *firstLUTGroup.begin();
        bool isFixed = firstInstance->isFixed(); // 根据第一个instance获取固定信息

        std::tuple<int, int, int> location = firstInstance->getLocation(); // 默认位置
        int hplb_X = std::get<0>(location);
        int hplb_Y = std::get<1>(location);
        int hplb_Z = std::get<2>(location);
        // 创建两个 HPLB：一个用于 z <= 3，另一个用于 z > 3
        HPLB *hplbLow = new HPLB(plbID, isFixed, std::make_tuple(-1, -1, 0));  // hplb_Z = 0
        HPLB *hplbHigh = new HPLB(plbID, isFixed, std::make_tuple(-1, -1, 1)); // hplb_Z = 1
        for (const auto &lutGroup : lutGroups)
        {
            for (Instance *instance : lutGroup)
            {
                std::tuple<int, int, int> location_inst = instance->getLocation();
                int hplb_X = std::get<0>(location_inst);
                int hplb_Y = std::get<1>(location_inst);
                int hplb_Z = std::get<2>(location_inst);

                // 根据 z 值将实例添加到对应的 HPLB
                if (hplb_Z > 3)
                {
                    hplbHigh->setLocation(hplb_X, hplb_Y, 1); // 设置位置
                    hplbHigh->addInstance(instance);          // 添加实例
                    // instance->setLocation(std::make_tuple(hplb_X, hplb_Y, hplb_Z));
                    instance->setHplbID(plbID * 2 + 1);
                }
                else
                {
                    hplbLow->setLocation(hplb_X, hplb_Y, 0); // 设置位置
                    hplbLow->addInstance(instance);          // 添加实例
                    // instance->setLocation(std::make_tuple(hplb_X, hplb_Y, hplb_Z));
                    instance->setHplbID(plbID * 2);
                }
            }
        }
        // 如果 HPLB 有实例，加入到全局 HPLB 集合
        if (hplbLow->getInstanceCount() != 0)
        {
            globalHPLBMap[plbID * 2] = hplbLow; // 为低区设置唯一 ID（例如 plbID * 2）
        }
        if (hplbHigh->getInstanceCount() != 0)
        {
            globalHPLBMap[plbID * 2 + 1] = hplbHigh; // 为高区设置唯一 ID（例如 plbID * 2 + 1）
        }
    }

    // ------- 将LUT整合到Tile内 ---------------------------------------------------------
    // 清除所有 tile 中的 LUT 实例
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
                        }
                    }
                }
            }
        }
    }

    // 创建一个向量存储未固定的HPLB组
    std::vector<HPLB *> nonFixedHPLBGrouptList;

    // 第二遍：遍历 globalHPLBMap ，挑选未固定的 HPLB 组
    for (const auto &hplbGroupPair : globalHPLBMap)
    {
        int plbGroupID = hplbGroupPair.first;
        const auto &hplbGroupSet = hplbGroupPair.second;
        if (!hplbGroupSet->getIsFixed())
        {
            nonFixedHPLBGrouptList.emplace_back(hplbGroupSet);
        }
    }
    // sortPLBGrouptList(nonFixedHPLBGrouptList); // 排序
    for (const auto &hplbGroupSet : nonFixedHPLBGrouptList)
    {
        Tile *tilePtr;
        std::tuple<int, int, int> newLocation;
        newLocation = hplbGroupSet->getLocation();

        int initial_loc_x = std::get<0>(newLocation);
        int initial_loc_y = std::get<1>(newLocation);
        std::set<std::tuple<int, int>> skip_set_neighbor;
        int range_serch = 1;

        int x_tile = std::get<0>(newLocation);
        int y_tile = std::get<1>(newLocation);
        int z_tile = std::get<2>(newLocation);
        bool isOneFlag = true;
        while (isOneFlag)
        {
            tilePtr = chip.getTile(x_tile, y_tile);
            int z_HPLB = tilePtr->findWhichPartForHPLB();
            if (z_HPLB != -1)
            {
                std::set<Instance *> instance_hplb = hplbGroupSet->getInstances();
                if (z_HPLB == 0 && z_tile == 0)
                {
                    for (Instance *instance : instance_hplb)
                    {
                        tilePtr->addInstance(instance->getInstID(), std::get<2>(instance->getLocation()), instance->getModelName(), false);
                        instance->setLocation(std::make_tuple(x_tile, y_tile, std::get<2>(instance->getLocation())));
                    }
                }
                else if (z_HPLB == 0 && z_tile == 1)
                {
                    for (Instance *instance : instance_hplb)
                    {
                        tilePtr->addInstance(instance->getInstID(), std::get<2>(instance->getLocation()) - 4, instance->getModelName(), false);
                        instance->setLocation(std::make_tuple(x_tile, y_tile, std::get<2>(instance->getLocation()) - 4));
                    }
                }
                else if (z_HPLB == 1 && z_tile == 0)
                {
                    for (Instance *instance : instance_hplb)
                    {
                        tilePtr->addInstance(instance->getInstID(), std::get<2>(instance->getLocation()) + 4, instance->getModelName(), false);
                        instance->setLocation(std::make_tuple(x_tile, y_tile, std::get<2>(instance->getLocation()) + 4));
                    }
                }
                else
                {
                    for (Instance *instance : instance_hplb)
                    {
                        tilePtr->addInstance(instance->getInstID(), std::get<2>(instance->getLocation()), instance->getModelName(), false);
                        instance->setLocation(std::make_tuple(x_tile, y_tile, std::get<2>(instance->getLocation())));
                    }
                }

                hplbGroupSet->setLocation(x_tile, y_tile, z_HPLB);
                isOneFlag = false;
                neighbor_PLB_xy.clear();
            }
            else
            {
                if (!neighbor_PLB_xy.empty() && !(skip_set_neighbor.find(neighbor_PLB_xy[0]) != skip_set_neighbor.end()))
                {
                    x_tile = std::get<0>(neighbor_PLB_xy[0]);
                    y_tile = std::get<1>(neighbor_PLB_xy[0]);
                    skip_set_neighbor.insert(neighbor_PLB_xy[0]);
                    neighbor_PLB_xy.erase(neighbor_PLB_xy.begin());
                }
                else
                {
                    neighbor_PLB_xy = getNeighborTiles(initial_loc_x, initial_loc_y, range_serch);
                    x_tile = std::get<0>(neighbor_PLB_xy[0]);
                    y_tile = std::get<1>(neighbor_PLB_xy[0]);
                    skip_set_neighbor.insert(neighbor_PLB_xy[0]);
                    neighbor_PLB_xy.erase(neighbor_PLB_xy.begin());
                    range_serch++;
                }
            }
        }
    }
    // legalCheck();
    int dummy = 0;
}