#include <iomanip>
#include "global.h"
#include "method.h"
#include "object.h"
#include "arbsa.h"
#include <algorithm>
#include <cmath>
#include "wirelength.h"
#include <random>
#include "pindensity.h"
// 计时
#include <chrono>

// #define DEBUG
// #define EXTERITER  //是否固定外部循环次数
#define INFO  //是否输出每次的迭代信息
#define TIME_LIMIT 1180

// 全局随机数生成器
std::mt19937 &get_random_engine()
{
    // static std::random_device rd;  // 用于生成种子
    static std::mt19937 gen; // 使用随机设备生成种子的Mersenne Twister随机数引擎
    return gen;
}
// 设置随机种子
void set_random_seed(unsigned int seed)
{
    get_random_engine().seed(seed);
}
// 生成一个 [min, max] 区间的随机整数
int generate_random_int(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(get_random_engine());
}
// 生成 [min, max] 范围内的随机浮点数
double generate_random_double(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(get_random_engine());
}

// 计算标准差
double calculateStandardDeviation(const std::vector<int> &data)
{
    if (data.empty())
    {
        return 0.0; // 如果数据为空，返回0或处理异常
    }

    // 计算平均值
    double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();

    // 计算每个元素与平均值的差值的平方和
    double variance = 0.0;
    for (int value : data)
    {
        variance += (value - mean) * (value - mean);
    }

    // 方差除以数据数量并开平方得到标准差
    return std::sqrt(variance / data.size());
}

// 计算rangeActulMap
int calculrangeMap(bool isBaseline, std::map<int, int> &rangeActualMap)
{
    for (auto iter : glbNetMap)
    {
        Net *net = iter.second;
        // 访问net input 引脚
        Instance *instIn = net->getInpin()->getInstanceOwner();
        int x, y, z;
        if (isBaseline)
        {
            std::tie(x, y, z) = instIn->getBaseLocation();
        }
        else
        {
            std::tie(x, y, z) = instIn->getLocation();
        }
        int maxX, minX, maxY, minY;
        maxX = minX = x;
        maxY = minY = y;
        // 访问net output 引脚
        std::list<Pin *> outputPins = net->getOutputPins();
        for (Pin *pin : outputPins)
        {
            Instance *instTmp = pin->getInstanceOwner();
            if (isBaseline)
            {
                std::tie(x, y, z) = instTmp->getBaseLocation();
            }
            else
            {
                std::tie(x, y, z) = instTmp->getLocation();
            }
            if (maxX < x)
                maxX = x;
            if (minX > x)
                minX = x;
            if (maxY < y)
                maxY = y;
            if (minY > y)
                minY = y;
        }
        int netDesired = std::ceil((maxX - minX + maxY - minY) / 2);
        // int netDesired = std::ceil((maxX-minX+maxY-minY));
        rangeActualMap[net->getId()] = netDesired;
    }
    return 0;
}

//优化，循环体中只计算相关的
int calculRelatedRangeMap(bool isBaseline, std::map<int, int>& rangeActualMap, const std::set<int>& instRelatedNetId){
    for(int i : instRelatedNetId){
        Net *net = glbNetMap[i];
        // 访问net input 引脚
        Instance *instIn = net->getInpin()->getInstanceOwner();
        int x, y, z;
        if (isBaseline)
        {
            std::tie(x, y, z) = instIn->getBaseLocation();
        }
        else
        {
            std::tie(x, y, z) = instIn->getLocation();
        }
        int maxX, minX, maxY, minY;
        maxX = minX = x;
        maxY = minY = y;
        // 访问net output 引脚
        std::list<Pin *> outputPins = net->getOutputPins();
        for (Pin *pin : outputPins)
        {
            Instance *instTmp = pin->getInstanceOwner();
            if (isBaseline)
            {
                std::tie(x, y, z) = instTmp->getBaseLocation();
            }
            else
            {
                std::tie(x, y, z) = instTmp->getLocation();
            }
            if (maxX < x)
                maxX = x;
            if (minX > x)
                minX = x;
            if (maxY < y)
                maxY = y;
            if (minY > y)
                minY = y;
        }
        int netDesired = std::ceil((maxX - minX + maxY - minY) / 2);
        // int netDesired = std::ceil((maxX-minX+maxY-minY));
        rangeActualMap[net->getId()] = netDesired;
    }
    return 0;
}

// 计算Fitness
int calculFitness(std::vector<std::pair<int, float>> &fitnessVec, std::map<int, int> &rangeDesiredMap, std::map<int, int> &rangeActualMap)
{
    // 计算fitness
    int n = fitnessVec.size();
    for (int i = 0; i < n; i++)
    {
        int netId = fitnessVec[i].first;
        float fitness;
        int rangeDesired = rangeDesiredMap[netId];
        int rangeActual = rangeActualMap[netId];
        if (rangeDesired == 0 && rangeActual == 0)
        {
            fitness = 1; // 都为0则不考虑移动了，认为为最完美的net
        }
        else if (rangeDesired >= rangeActual)
        {
            fitness = rangeActual / rangeDesired;
        }
        else
        {
            fitness = rangeDesired / rangeActual;
        }
        fitnessVec[i] = std::make_pair(netId, fitness);
    }
    return 0;
}

// 优化 只计算相关的fitness
int calculRelatedFitness(std::vector<std::pair<int, float>> &fitnessVec, std::map<int, int> &rangeDesiredMap, std::map<int, int> &rangeActualMap, const std::set<int> &instRelatedNetId)
{
    // 计算fitness
    int n = fitnessVec.size();
    for(auto netId : instRelatedNetId){
        int index = -1; 
        //找到匹配netId的下标
        for (int i = 0; i < fitnessVec.size(); ++i) {
            if (fitnessVec[i].first == netId) {
                index = i;
                break;
            }
        }
        #ifdef DEBUG
        if(index == -1){
            std::cout<<"can not find this netId: "<<netId <<", something may be wrong"<<std::endl;
            exit(1);
        }
        #endif
        int rangeDesired = rangeDesiredMap[netId];
        int rangeActual = rangeActualMap[netId];
        float fitness;
        if (rangeDesired == 0 && rangeActual == 0)
        {
            fitness = 1; // 都为0则不考虑移动了，认为为最完美的net
        }
        else if (rangeDesired >= rangeActual)
        {
            fitness = rangeActual / rangeDesired;
        }
        else
        {
            fitness = rangeDesired / rangeActual;
        }
        fitnessVec[index] = std::make_pair(netId, fitness);
    }
    return 0;
}

int sortedFitness(std::vector<std::pair<int, float>> &fitnessVec)
{ // 将fitnessVec按第二个值升序排列
    std::sort(fitnessVec.begin(), fitnessVec.end(), [](const std::pair<int, float> &a, const std::pair<int, float> &b)
              {
                  return a.second < b.second; // 按照 float 值从小到大排序
              });
    return 0;
}

int selectNetId(std::vector<std::pair<int, float>> &fitnessVec)
{ // 返回具有一定规律随机选取的net
    int n = fitnessVec.size();
    int a = generate_random_int(0, n - 1); // 范围在 0 到 n - 1
    int b = generate_random_int(0, n - 1);
    int index = std::min(a, b);
    int netId = fitnessVec[index].first;

    //跳过bigNet
    if(glbBigNetPinNum > 0){
        while(glbBigNet.find(netId) != glbBigNet.end()){
            a = generate_random_int(0, n - 1); // 范围在 0 到 n - 1
            b = generate_random_int(0, n - 1);
            index = std::min(a, b);
            netId = fitnessVec[index].first;
        }
    }

    return netId;
}

Instance* selectInst(Net *net){ //返回在net中随机选取的instance指针
    std::list<Pin*>& pinList = net->getOutputPins();
    pinList.emplace_back(net->getInpin());
    int n = pinList.size(); //
    Instance *inst = nullptr;
    std::vector<int> numbers(n);                  // 0到n-1的数字
    std::iota(numbers.begin(), numbers.end(), 0); // 填充从0到n的数字
    while (!numbers.empty())
    {
        int randomIndex = generate_random_int(0, numbers.size() - 1);
        int index = numbers[randomIndex];

        // 创建一个迭代器指向 list 的开始
        std::list<Pin *>::iterator it = pinList.begin();
        // 使用 std::advance 移动迭代器到指定下标
        std::advance(it, index);
        inst = (*it)->getInstanceOwner();

        // 检查是否符合规则
        if (inst->isFixed())
        {
            // 移除不符合规则的数字
            numbers.erase(numbers.begin() + randomIndex);
            inst = nullptr;
        }
        else
        {
            break;
        }
    }
    return inst;
}

std::pair<int, int> getNetCenter(bool isBaseline, Net *net)
{ // 返回net的中心位置
    int x = 0, y = 0;
    std::set<int> visitInst; // 记录访问过的inst的id，防止一个inst有多个引脚在同一net且计算多次的情况
    // 统计inpin
    Instance *inst = net->getInpin()->getInstanceOwner();
    int xt, yt, zt;
    if (isBaseline)
    {
        std::tie(xt, yt, zt) = inst->getBaseLocation();
    }
    else
    {
        std::tie(xt, yt, zt) = inst->getLocation();
    }
    x += xt;
    y += yt;
    // 标记为访问过
    std::string instName = inst->getInstanceName();
    size_t underscorePos = instName.find('_');
    if (underscorePos != std::string::npos)
    {
        std::string subStr = instName.substr(underscorePos + 1);
        // Convert the second substring to an integer
        int instId = std::stoi(subStr);
        visitInst.insert(instId);
    }
    // 统计outputpin
    std::list<Pin *> pinList = net->getOutputPins();
    for (const auto &pin : pinList)
    {
        Instance *inst = pin->getInstanceOwner();
        // 判断是否重复出现
        std::string instName = inst->getInstanceName();
        size_t underscorePos = instName.find('_');
        if (underscorePos != std::string::npos)
        {
            std::string subStr = instName.substr(underscorePos + 1);
            // Convert the second substring to an integer
            int instId = std::stoi(subStr);
            auto it = visitInst.find(instId);
            if (it != visitInst.end())
            {
                // 重复的inst，跳过
                continue;
            }
            else
                visitInst.insert(instId);
        }
        if (isBaseline)
        {
            std::tie(xt, yt, zt) = inst->getBaseLocation();
        }
        else
        {
            std::tie(xt, yt, zt) = inst->getLocation();
        }
        x += xt;
        y += yt;
    }
    // 取平均值
    x = x / visitInst.size();
    y = y / visitInst.size();
    return std::make_pair(x, y);
}

bool isValid(bool isBaseline, int x, int y, int &z, Instance *inst)
{ // 判断这个位置是否可插入该inst，如果可插入则返回z值
    bool valid = false;
    Tile *tile = chip.getTile(x, y);
    // if(tile == NULL || tile->matchType("PLB") == false){
    //     return false;
    // }
    std::string instType = inst->getModelName();
    std::string instName = inst->getInstanceName();
    int instId = std::stoi(inst->getInstanceName().substr(5)); // inst_xxx 从第5个字符开始截取，转换为整数
    if (instType.substr(0, 3) == "LUT")
    {
        std::vector<int> hasDRAM(2, 0); //  1 表示有DRAM
        // 先判断是否有DRAM
        slotArr dramSlotArr = *(tile->getInstanceByType("DRAM"));
        slotArr lutSlotArr = *(tile->getInstanceByType("LUT"));
        int lutBegin = 0, lutEnd = 0;
        for (int idx = 0; idx < (int)dramSlotArr.size(); idx++)
        {
            Slot *slot = dramSlotArr[idx];
            if (slot == nullptr)
            {
                continue;
            }
            std::list<int> instances;
            if (isBaseline)
            {
                instances = slot->getBaselineInstances();
            }
            else
            {
                instances = slot->getOptimizedInstances();
            }
            if (instances.empty())
            {
                continue;
            }
            // DRAM at slot0 blocks lut slot 0~3
            // DRAM at slot1 blocks lut slot 4~7
            hasDRAM[idx] = 1;
        }
        if (hasDRAM[0] && hasDRAM[1])
        {
            // 两个都是DRAM，不可放置
            return false;
        }
        // 0-3不可放
        else if (hasDRAM[0])
        {
            lutBegin = 4;
            lutEnd = 8;
        }
        // 4-7不可放
        else if (hasDRAM[1])
        {
            lutBegin = 0;
            lutEnd = 4;
        }
        // 都可以放
        else
        {
            lutBegin = 0;
            lutEnd = 8;
        }
        // 新加的 cjq 1104
        if (inst->getMatchedLUTID() != -1 && (inst->getModelName()).substr(0, 3) == "LUT")
        {
            for (int idx = lutBegin; idx < lutEnd; idx++)
            {
                Slot *slot = lutSlotArr[idx];
                if (slot == nullptr)
                {
                    continue;
                }
                std::list<int> instances;
                if (isBaseline)
                {
                    instances = slot->getBaselineInstances();
                }
                else
                {
                    instances = slot->getOptimizedInstances();
                }
                if (instances.size() == 0)
                {
                    // 找到整个位置都是空的
                    z = idx;
                    return true;
                }
            }
            return false;
        }
        std::vector<std::pair<int, int>> record; // 记录每个引脚小于6的 lut  [idx, pinNum]
        for (int idx = lutBegin; idx < lutEnd; idx++)
        {
            Slot *slot = lutSlotArr[idx];
            if (slot == nullptr)
            {
                continue;
            }
            std::list<int> instances;
            if (isBaseline)
            {
                instances = slot->getBaselineInstances();
            }
            else
            {
                instances = slot->getOptimizedInstances();
            }
            if (instances.size() <= 1)
            {
                // 大于1个lut，不可放入了
                instances.push_back(instId); // 添加当前instId
                std::set<int> totalInputs;
                for (auto instID : instances)
                {
                    Instance *instPtr = glbInstMap.find(instID)->second;
                    std::vector<Pin *> inpins = instPtr->getInpins();
                    for (auto pin : inpins)
                    {
                        if (pin->getNetID() != -1)
                        {
                            totalInputs.insert(pin->getNetID());
                        }
                    }
                }
                if (totalInputs.size() <= 6)
                {
                    // 符合条件 记录
                    record.emplace_back(std::make_pair(idx, totalInputs.size()));
                }
            }
        }
        if (record.size() != 0)
        {
            // 按第二个元素降序排序
            std::sort(record.begin(), record.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b)
                      {
                          return a.second > b.second; // 使用 > 实现降序
                      });
            z = record[0].first;
            return true;
        }
    }
    else if (inst->getModelName() == "SEQ")
    {
        slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
        for (int bank = 0; bank < 2; bank++)
        {
            // bank0 0-8   bank1 8-16
            int start = bank * 8;
            int end = (bank + 1) * 8;
            std::set<int> instIdSet;
            instIdSet.insert(instId); // 添加当前Id
            for (int i = start; i < end; i++)
            {
                Slot *slot = seqSlotArr[i];
                std::list<int> instList;
                if (isBaseline)
                    instList = slot->getBaselineInstances();
                else
                    instList = slot->getOptimizedInstances();
                for (int id : instList)
                {
                    instIdSet.insert(id);
                }
            }
            // 判断每个inst的引脚
            std::set<int> clkNets; // 不超过1
            std::set<int> ceNets;  // 不超过2
            std::set<int> srNets;  // 不超过1
            for (int i : instIdSet)
            {
                Instance *instPtr = glbInstMap[i];
                int numInpins = instPtr->getNumInpins();
                for (int i = 0; i < numInpins; i++)
                {
                    // 只判断input是否是这几个量
                    Pin *pin = instPtr->getInpin(i);
                    int netID = pin->getNetID();
                    if (netID >= 0)
                    {
                        PinProp prop = pin->getProp();
                        if (prop == PIN_PROP_CE)
                        {
                            ceNets.insert(netID);
                        }
                        else if (prop == PIN_PROP_CLOCK)
                        {
                            clkNets.insert(netID);
                        }
                        else if (prop == PIN_PROP_RESET)
                        {
                            srNets.insert(netID);
                        }
                    }
                }
            }

            int numClk = clkNets.size();
            int numReset = srNets.size();
            int numCe = ceNets.size();

            if (numClk > MAX_TILE_CLOCK_PER_PLB_BANK || numCe > MAX_TILE_CE_PER_PLB_BANK || numReset > MAX_TILE_RESET_PER_PLB_BANK)
            {
                // 该bank放入该inst时违反约束
                valid = false;
            }
            else
            {
                // SEQ只要返回一个空位子即可
                // slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
                for (int i = start; i < end; i++)
                {
                    Slot *slot = seqSlotArr[i];
                    std::list<int> listTmp;
                    if (isBaseline)
                        listTmp = slot->getBaselineInstances();
                    else
                        listTmp = slot->getOptimizedInstances();
                    if (listTmp.size() == 0)
                    {
                        z = i;
                        return true;
                    }
                }
            }
        }
    }

    return valid;
}

std::tuple<int, int, int> findSuitableLoc(bool isBaseline, int x, int y, int rangeDesired, Instance *inst)
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
    std::vector<std::pair<int, int>> coordinates;
    for (int x = xl; x <= xr; ++x)
    {
        for (int y = yl; y <= yr; ++y)
        {
            if (isPLB[x][y])
            {                                   // 只加入PLB块
                coordinates.emplace_back(x, y); // 将坐标 (x, y) 加入向量
            }
        }
    }
    int xx, yy, zz = -1;  // 目标坐标
    int xCur, yCur, zCur; // 当前坐标
    // 目前inst所在位置
    if (isBaseline)
        std::tie(xCur, yCur, zCur) = inst->getBaseLocation();
    else
        std::tie(xCur, yCur, zCur) = inst->getLocation();

    // 进行随机抽取
    while (!coordinates.empty())
    {
        int randomIndex = generate_random_int(0, coordinates.size() - 1);
        ;
        xx = coordinates[randomIndex].first;
        yy = coordinates[randomIndex].second;
#ifdef DEBUG
        std::cout << "DEBUG-Selected target coordinates: (" << xx << ", " << yy << ")" << std::endl;
#endif
        if (xCur == xx && yCur == yy)
        {
#ifdef DEBUG
            std::cout << "DEBUG-The coordinates (" << xx << ", " << yy << ") are consistent with the original coordinates (" << xCur << ", " << yCur << ")" << std::endl;
#endif
            // 移除不符合规则的坐标
            coordinates.erase(coordinates.begin() + randomIndex);
            continue;
        }
        // 检查坐标是否符合规则
        if (isValid(isBaseline, xx, yy, zz, inst))
        {
            break;
        }
        else
        {
#ifdef DEBUG
            std::cout << "DEBUG-The coordinates (" << xx << ", " << yy << ") do not conform to the constraints" << std::endl;
#endif
            // 移除不符合规则的坐标
            coordinates.erase(coordinates.begin() + randomIndex);
        }
    }
    return {xx, yy, zz};
}

// 修改slot队列
int changeTile(bool isBaseline, std::tuple<int, int, int> originLoc, std::tuple<int, int, int> loc, Instance *inst)
{
    int xCur, yCur, zCur, xGoal, yGoal, zGoal;
    std::tie(xCur, yCur, zCur) = originLoc;
    std::tie(xGoal, yGoal, zGoal) = loc;
    Tile *tileCur = chip.getTile(xCur, yCur);
    Tile *tileGoal = chip.getTile(xGoal, yGoal);
    int instId = std::stoi(inst->getInstanceName().substr(5)); // inst_xxx 从第5个字符开始截取，转换为整数
    // 删除旧的tile插槽中的inst
    slotArr *slotArrCur = tileCur->getInstanceByType(inst->getModelName().substr(0, 3)); // LUT or SEQ
    Slot *slot = slotArrCur->at(zCur);
    if (inst->getMatchedLUTID() != -1 && (inst->getModelName()).substr(0, 3) == "LUT")
    {
        if (isBaseline)
            slot->clearBaselineInstances();
        else
            slot->clearOptimizedInstances();
        // 添加到新的tile
        tileGoal->addInstance(instId, zGoal, inst->getModelName(), isBaseline);
        tileGoal->addInstance(inst->getMatchedLUTID(), zGoal, inst->getModelName(), isBaseline);
    }
    else
    {
        if (isBaseline)
        {
            std::list<int> &instances = slot->getBaselineInstances();
            for (int instIdTmp : instances)
            {
                if (instId == instIdTmp)
                {
                    instances.remove(instIdTmp);
                    break;
                }
            }
        }
        else
        {
            std::list<int> &instances = slot->getOptimizedInstancesRef();
            for (int instIdTmp : instances)
            {
                if (instId == instIdTmp)
                {
                    instances.remove(instIdTmp);
                    break;
                }
            }
        }
        // 在新的插槽中插入
        tileGoal->addInstance(instId, zGoal, inst->getModelName(), isBaseline);
    }

    return 0;
}


//删除旧值，插入新值

bool tryUpdatePinDensity(std::tuple<int,int,int> originLoc, std::tuple<int,int,int> loc){
    int originLocIndex = std::get<0>(originLoc) * 1000 + std::get<1>(originLoc);
    int locIndex = std::get<0>(loc) * 1000 + std::get<1>(loc);
    //获取旧的pin密度
    int oldOriginLocPD = 0;
    int oldLocPD = 0;
    auto itOrigin = std::find_if(glbPinDensity.begin(), glbPinDensity.end(), [originLocIndex](const std::pair<int, int>& p) {
        return p.first == originLocIndex;
    });
    if (itOrigin != glbPinDensity.end()) {
        oldOriginLocPD = itOrigin->second;
    } 
    auto it = std::find_if(glbPinDensity.begin(), glbPinDensity.end(), [locIndex](const std::pair<int, int>& p) {
        return p.first == locIndex;
    });
    if (it != glbPinDensity.end()) {
        oldLocPD = it->second;
    } 

    //获取新的pin密度分子
    int newOriginLocPD = getPinDensityByXY(std::get<0>(originLoc), std::get<1>(originLoc));
    int newLocPD = getPinDensityByXY(std::get<0>(loc), std::get<1>(loc));

    //修改
    if(itOrigin == glbPinDensity.end()){
        glbPinDensity.emplace_back(std::make_pair(originLocIndex, newOriginLocPD));
    }
    else{
        itOrigin->second = newOriginLocPD;
    }
    if(it == glbPinDensity.end()){
        glbPinDensity.emplace_back(std::make_pair(locIndex, newLocPD));
    }
    else{
        it->second = newLocPD;
    }
    

    //排序
    std::sort(glbPinDensity.begin(), glbPinDensity.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return a.second > b.second; // 按值降序排序
    });
    itOrigin = std::find_if(glbPinDensity.begin(), glbPinDensity.end(), [originLocIndex](const std::pair<int, int>& p) {
        return p.first == originLocIndex;
    });
    it = std::find_if(glbPinDensity.begin(), glbPinDensity.end(), [locIndex](const std::pair<int, int>& p) {
        return p.first == locIndex;
    });


    // 计算新和
    int potentialSum = 0;
    int i = 0;
    for(auto& it : glbPinDensity){
        if(i >= glbTopKNum){
            break;
        }
        potentialSum += it.second;
        i++;
    }
    
    if (potentialSum > glbInitTopSum) {
        //复原
        itOrigin->second = oldOriginLocPD;
        it->second = oldLocPD;
        return false;
    }
    else{
        //应用修改
        return true;
    }
}

int arbsa(bool isBaseline, std::string nodesFile){
    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 初始布局

    // 懒加载，算到再加进去，用空间换时间。key: instId  value: instRelatedNetId
    std::map<int, std::set<int>> instRelatedNetIdMap;

    // 构造 fitness 优先级列表 初始化 rangeDesired
    std::vector<std::pair<int, float>> fitnessVec; // 第一个是netId，第二个是适应度fitness, 适应度越小表明越需要移动。后续会按照fitness升序排列
    std::map<int, int> rangeDesiredMap;            // 第一个是netId，第二个是外框矩形的平均跨度，即半周线长的一半
    calculrangeMap(isBaseline, rangeDesiredMap);
    for (auto it : glbNetMap)
    {
        Net *net = it.second;
        // 构造fitnessVec
        fitnessVec.emplace_back(std::make_pair(net->getId(), 0));
    }
    std::map<int, int> rangeActualMap(rangeDesiredMap); // 第一个是netId，第二个是当前实际的外框矩形的平均跨度，即半周线长的一半

    // 计算 fitness 并 排序
    calculFitness(fitnessVec, rangeDesiredMap, rangeActualMap);
    sortedFitness(fitnessVec);

    // 初始化迭代次数Iter、初始化温度T
    int Iter = 0;
    int InnerIter = 2000; // int( glbInstMap.size()*0.2 );  int( pow(glbInstMap.size(),4/3) );
    float T = 2;
    float threashhold = 0; //1e-5
    float alpha = 0.8; //0.8-0.99
    const int timeLimit = TIME_LIMIT; //1180  3580
    // 计算初始cost
    int cost = 0, costNew = 0;
    cost = getWirelength(isBaseline);
    // cost = getHPWL(isBaseline);
    // 自适应参数
    int counterNet = 0;
    const int counterNetLimit = 800;
    const int seed = 999; // 999 888
    set_random_seed(seed);

    std::vector<int> sigmaVecInit;
    // 根据标准差设置初始温度
    for (int i = 0; i < 50; i++)
    {
        // 计算50次步骤取方差
        int netId = selectNetId(fitnessVec);
        Net *net = glbNetMap[netId];
        Instance *inst = selectInst(net);
        if (inst == nullptr)
        {
            // 没找到可移动的inst，跳过后续部分
            continue;
        }
        int centerX, centerY;
        std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
        int x, y, z;
        std::tie(x, y, z) = findSuitableLoc(isBaseline, centerX, centerY, rangeDesiredMap[netId], inst);
        if (z == -1)
        {
            // 没找到合适位置
            continue;
        }
        // 找到这个inst附近的net
        std::set<int> instRelatedNetId;
        // 访问inputpin
        for (auto &pin : inst->getInpins())
        {
            int netId = pin->getNetID();
            //-1表示未连接
            if(netId == -1) continue;
            if(glbBigNetPinNum > 0 && glbBigNet.find(netId) != glbBigNet.end()){
                continue;
            }
            instRelatedNetId.insert(pin->getNetID());
        }
        // 访问outputpin
        for (auto &pin : inst->getOutpins())
        {
            int netId = pin->getNetID();
            //-1表示未连接
            if(netId == -1) continue;
            if(glbBigNetPinNum > 0 && glbBigNet.find(netId) != glbBigNet.end()){
                continue;
            }
            instRelatedNetId.insert(pin->getNetID());
        }
        
        // 计算移动后的newCost
        std::tuple<int, int, int> loc = std::make_tuple(x, y, z);
        std::tuple<int, int, int> originLoc;
        // 保存更新前的部分net
        int beforeNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
        if (isBaseline)
        {
            originLoc = inst->getBaseLocation();
            inst->setBaseLocation(loc);
        }
        else
        {
            originLoc = inst->getLocation();
            inst->setLocation(loc);
        }
        int afterNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
        int costNew = cost - beforeNetWL + afterNetWL;
        if (costNew < cost)
        {
            sigmaVecInit.emplace_back(costNew);
        }
        // 复原
        if (isBaseline)
        {
            inst->setBaseLocation(originLoc);
        }
        else
        {
            inst->setLocation(originLoc);
        }
    }

    //读取次数  data.json文件
    std::string caseName = extractFileName(nodesFile);
    int iterLimit = -1;
    int iterTotal = 0;
    const std::string filename = "data.json";
    // 读取 JSON 文件内容
    NestedMap jsonData;

    if(fileExists(filename)){
        jsonData = readJsonFile(filename);
        if(jsonData[caseName].find(std::to_string(timeLimit)) != jsonData[caseName].end()){
            //找到了参数
            iterLimit = jsonData[caseName][std::to_string(timeLimit)];
        }
    }

    //记录bigNet的cost
    int bigNetCostPre = 0;
    int bigNetCostCur = 0;
    //设置引脚数超过该数字的net为bigNet
    const int pinNumLimit = 5000; //5000
    //填充有超过次数的bigNet
    if(findBigNetId(pinNumLimit)){
        //记录bigNet的cost
        bigNetCostPre = getRelatedWirelength(isBaseline, glbBigNet);
    }
    int hitBigNet = 0; //统计修改影响bigNet的点数，用于更新bigNet的线长
    int hitBigNetLimit = glbBigNetPinNum * 0.05; //引脚数的百分之二十
    //计算标准差
    double standardDeviation = calculateStandardDeviation(sigmaVecInit);
    std::cout << "------------------------------------------------\n";
    std::cout << "[INFO] Standard Deviation: " << standardDeviation << std::endl;
    if (standardDeviation != 0)
    {
        T = 0.5 * standardDeviation;
    }
    std::cout<<"[INFO] The simulated annealing algorithm starts "<< std::endl;
    std::cout<<"[INFO] initial temperature T = "<< T <<", threshhold = "<<threashhold<<", alpha = "<<alpha<<
             ", InnerIter = "<<InnerIter<<", seed ="<<seed << ", iterLimit ="<< iterLimit <<std::endl;
#ifdef EXTERITER
    int exterIter = 0;
    int exterIterLimit = 10;
#endif
    bool timeup = false;
    int epsilon = 1; // 设置收敛阈值
    int exterIter = 0;
    int costPre = cost;
    // 外层循环 温度大于阈值， 更新一次fitness优先级列表
    while (T > threashhold)
    {
        // 记录接受的new_cost
        std::vector<int> sigmaVec;
        // 截断循环 判断是否收敛
        /*
        if(exterIter >= 10){
            exterIter = 0;
            int detaCost = std::abs(costPre - cost);
            // std::cout<<cost <<'-'<<costPre<<'='<<detaCost<<std::endl;
            if(detaCost < epsilon){
                break;
            }
            else{
                costPre = cost;
            }
        }
        exterIter++;  // exterIter + 1 = Iter + 2000 */
        // 内层循环 小于内层迭代次数
#ifdef EXTERITER
        if(exterIter >= exterIterLimit){
            break;
        }
        exterIter ++;
#endif
        // updatePinDensityMapAndTopValues(); //更新全局密度
        while(Iter < InnerIter){
            /*********** 更新 bigNet cost **************/
            if(glbBigNetPinNum > 0 && hitBigNet >= hitBigNetLimit){
                //更新bigNet
                bigNetCostCur = getRelatedWirelength(isBaseline, glbBigNet);
                cost = cost - bigNetCostPre + bigNetCostCur;
                bigNetCostPre = bigNetCostCur;
                hitBigNet = 0;
                //顺带更新range与fitness
                // calculRelatedRangeMap(isBaseline, rangeActualMap, glbBigNet);
                // calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, glbBigNet);
            }

            if(Iter % 100 == 0) {
                if(iterLimit > 0){
                    if(iterTotal >= iterLimit){
                        timeup = true;
                        break;
                    }
                }
                else{
                    auto tmp = std::chrono::high_resolution_clock::now();
                    // 计算运行时间
                    std::chrono::duration<double> durationtmp = tmp - start;
                    if(durationtmp.count() >= timeLimit){  //1180
                        timeup = true;
                        if(!jsonData.count(caseName)){
                            std::map<std::string, int> t;
                            jsonData[caseName] = t;
                        }
                        jsonData[caseName][std::to_string(timeLimit)] = iterTotal;
                        break;
                    }
                }
                #ifdef INFO
                std::cout<<"[INFO] T:"<< std::scientific << std::setprecision(3) <<T <<" iter:"<<std::setw(4)<<Iter<<" alpha:"<<std::fixed<<std::setprecision(2)<<alpha<<" cost:"<<std::setw(7)<<cost<<std::endl;
                #endif
            }
            // std::cout<<"[INFO] T:"<< std::scientific << std::setprecision(3) <<T <<" iter:"<<std::setw(4)<<Iter<<" alpha:"<<std::fixed<<std::setprecision(2)<<alpha<<" cost:"<<std::setw(7)<<cost<<std::endl;
            Iter++;
            iterTotal++;
            // 根据fitness列表选择一个net
            int netId = selectNetId(fitnessVec);
#ifdef DEBUG
            std::cout << "DEBUG-netId:" << netId << std::endl;
#endif
            Net *net = glbNetMap[netId];
            // 随机选择net中的一个inst
            Instance *inst = selectInst(net);
            if (inst == nullptr)
            {
                // 没找到可移动的inst，跳过后续部分
                continue;
            }
            if (inst->getInstanceName() == "inst_1613")
            {
                int a = 0;
            }
            if (inst->getInstanceName() == "inst_1614")
            {
                int a = 0;
            }
#ifdef DEBUG
            std::cout << "DEBUG-instName:" << inst->getInstanceName() << std::endl;
#endif
            // 确定net的中心
            int centerX, centerY;
            std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
            // 在 rangeDesired 范围内选取一个位置去放置这个inst
            int x, y, z;
            std::tie(x, y, z) = findSuitableLoc(isBaseline, centerX, centerY, rangeDesiredMap[netId], inst);
            if (z == -1)
            {
                // 没找到合适位置
                continue;
            }
            // 空间换时间
            // 找到这个inst附近的net
            std::set<int> instRelatedNetId;
            bool instHasBigNet = false; //判断这个inst是否连接到bigNet
            //访问inputpin
            for(auto& pin: inst->getInpins()){
                int netId = pin->getNetID();
                //-1表示未连接
                if(netId == -1) continue;
                if(glbBigNetPinNum > 0 && glbBigNet.find(netId) != glbBigNet.end()){
                    instHasBigNet = true;
                    continue;
                }
                instRelatedNetId.insert(pin->getNetID());
            }
            //访问outputpin
            for(auto& pin: inst->getOutpins()){
                int netId = pin->getNetID();
                //-1表示未连接
                if(netId == -1) continue;
                if(glbBigNetPinNum > 0 && glbBigNet.find(netId) != glbBigNet.end()){
                    instHasBigNet = true;
                    continue;
                }
                instRelatedNetId.insert(pin->getNetID());
            }

            // 计算移动后的newCost
            std::tuple<int, int, int> loc = std::make_tuple(x, y, z);
            std::tuple<int, int, int> originLoc;
            // 保存更新前的部分net
            int beforeNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
            if (isBaseline)
            {
                originLoc = inst->getBaseLocation();
                inst->setBaseLocation(loc);
                if (inst->getMatchedLUTID() != -1)
                {
                    Instance *matchedInst = glbInstMap[inst->getMatchedLUTID()];
                    matchedInst->setBaseLocation(loc);
                }
            }
            else
            {
                originLoc = inst->getLocation();
                inst->setLocation(loc);
                if (inst->getMatchedLUTID() != -1)
                {
                    Instance *matchedInst = glbInstMap[inst->getMatchedLUTID()];
                    matchedInst->setLocation(loc);
                }
            }
            int afterNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
            int costNew = cost - beforeNetWL + afterNetWL;
            // costNew = getHPWL(isBaseline);
            // deta = new_cost - cost
            int deta = costNew - cost;
            #ifdef DEBUG
                std::cout<<"DEBUG-deta:"<<deta<<std::endl;
            #endif

            //计算pin密度变化 tryUpdateAndCheck
            //改变tile
            changeTile(isBaseline, originLoc, loc, inst);

            // 生成一个 0 到 1 之间的随机浮点数
            double randomValue = generate_random_double(0.0, 1.0);
            double eDetaT = exp(-deta/T);
            // if deta < 0 更新这个操作到布局中，更新fitness列表
            if((deta < 0 || randomValue < eDetaT) && tryUpdatePinDensity(originLoc, loc)){
                // 间隔次数多了再更新这两
                // calculRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                // calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
                cost = costNew;
                sigmaVec.emplace_back(costNew);
                // sortedFitness(fitnessVec);
            }
            else{
                //复原
                changeTile(isBaseline, loc, originLoc, inst);
                if(isBaseline){
                    inst->setBaseLocation(originLoc);
                } else{
                    inst->setLocation(originLoc);
                }
            }
            // counterNet 计数+1
            counterNet += 1;
            if (counterNet % 100 == 0)
            {
                calculRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
            }
            // 当计数等于一个限制时，更新rangeActual 到 rangeDesired
            if (counterNet == counterNetLimit)
            {
                rangeDesiredMap = rangeActualMap;
                counterNet = 0;
            }
        }
        if (timeup)
            break; // 时间快到了，结束

        double acceptRate = sigmaVec.size() / InnerIter;
        if (0.96 <= acceptRate)
        {
            alpha = 0.5;
        }
        else if (0.8 <= acceptRate && acceptRate < 0.96)
        {
            alpha = 0.9;
        }
        else if (0.16 <= acceptRate && acceptRate < 0.8)
        {
            alpha = 0.95;
        }
        else
        {
            alpha = 0.8;
        }

        // T = alpha * T
        T = alpha * T;
        // Iter = 0
        Iter = 0;
        // 排序fitness列表
        sortedFitness(fitnessVec);
    }
    //记录截止次数
    writeJsonFile(filename, jsonData);
    
    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算运行时间
    std::chrono::duration<double> duration = end - start;

    // 输出运行时间（单位为秒）
    std::cout << "runtime: " << duration.count() << " s" << std::endl;
#ifdef EXTERITER
    std::cout << "runtime/exterIterLimit:" << duration.count() / (exterIterLimit) <<" runtime/iter:" << duration.count() / ((exterIterLimit)*2000)<< std::endl;
#endif
    return 0;
}

// 粗化使用
int newArbsa(bool isBaseline, bool isSeqPack)
{

    // 获取当前时间
    std::time_t now = std::time(nullptr);
    // 转换为本地时间
    std::tm* localTime = std::localtime(&now);

    // 输出当前时间
    std::cout << "当前时间: "
              << std::put_time(localTime, "%Y-%m-%d %H:%M:%S") << std::endl;

    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 初始布局

    // 懒加载，算到再加进去，用空间换时间。key: instId  value: instRelatedNetId
    std::map<int, std::set<int>> instRelatedNetIdMap;

    // 构造 fitness 优先级列表 初始化 rangeDesired
    std::vector<std::pair<int, float>> fitnessVec; // 第一个是netId，第二个是适应度fitness, 适应度越小表明越需要移动。后续会按照fitness升序排列
    std::map<int, int> rangeDesiredMap;            // 第一个是netId，第二个是外框矩形的平均跨度，即半周线长的一半
    calculrangeMap(isBaseline, rangeDesiredMap);
    for (auto it : glbPackNetMap)
    {
        Net *net = it.second;
        // 构造fitnessVec
        fitnessVec.emplace_back(std::make_pair(net->getId(), 0));
    }
    std::map<int, int> rangeActualMap(rangeDesiredMap); // 第一个是netId，第二个是当前实际的外框矩形的平均跨度，即半周线长的一半

    // 计算 fitness 并 排序
    calculFitness(fitnessVec, rangeDesiredMap, rangeActualMap);
    sortedFitness(fitnessVec);

    // 初始化迭代次数Iter、初始化温度T
    int Iter = 0;
    int InnerIter = 2000; // int( glbInstMap.size()*0.2 );  int( pow(glbInstMap.size(),4/3) );
    float T = 2;
    float threashhold = 0;    // 1e-5
    float alpha = 0.8;        // 0.8-0.99
    const int timeLimit = 1180; // 1180  3580
    // 计算初始cost
    int cost = 0, costNew = 0;
    // int cost1 = getWirelength(isBaseline);
    cost = getPackWirelength(isBaseline); // wbx 计算pack后的线长
    // cost = getHPWL(isBaseline);
    // 自适应参数
    int counterNet = 0;
    const int counterNetLimit = 800;
    const int seed = 999; // 999 888
    set_random_seed(seed);

    std::vector<int> sigmaVecInit;
    // 根据标准差设置初始温度
    for (int i = 0; i < 50; i++)
    {
        // 计算50次步骤取方差
        int netId = selectNetId(fitnessVec);
        Net *net = glbPackNetMap[netId]; // 修改为新的Netmap
        Instance *inst = selectInst(net);
        if (inst == nullptr)
        {
            // 没找到可移动的inst，跳过后续部分
            continue;
        }
        int centerX, centerY;
        std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
        int x, y, z;
        std::tie(x, y, z) = findPackSuitableLoc(isBaseline, centerX, centerY, rangeDesiredMap[netId], inst, isSeqPack);
        if (z == -1)
        {
            // 没找到合适位置
            continue;
        }
        // 找到这个inst附近的net
        std::set<int> instRelatedNetId;
        // 访问inputpin
        for (auto &pin : inst->getInpins())
        {
            int netId = pin->getNetID();
            //-1表示未连接
            if (netId != -1)
                instRelatedNetId.insert(oldNetID2newNetID[pin->getNetID()]);
        }
        // 访问outputpin
        for (auto &pin : inst->getOutpins())
        {
            int netId = pin->getNetID();
            //-1表示未连接
            if (netId != -1)
                instRelatedNetId.insert(oldNetID2newNetID[pin->getNetID()]);
        }
        // 计算移动后的newCost
        std::tuple<int, int, int> loc = std::make_tuple(x, y, z);
        std::tuple<int, int, int> originLoc;
        // 保存更新前的部分net
        int beforeNetWL = getPackRelatedWirelength(isBaseline, instRelatedNetId);
        if (isBaseline)
        {
            originLoc = inst->getBaseLocation();
            inst->setBaseLocation(loc);
        }
        else
        {
            originLoc = inst->getLocation();
            inst->setLocation(loc);
        }
        int afterNetWL = getPackRelatedWirelength(isBaseline, instRelatedNetId);
        int costNew = cost - beforeNetWL + afterNetWL;
        if (costNew < cost)
        {
            sigmaVecInit.emplace_back(costNew);
        }
        // 复原
        if (isBaseline)
        {
            inst->setBaseLocation(originLoc);
        }
        else
        {
            inst->setLocation(originLoc);
        }
    }

    // 计算标准差
    double standardDeviation = calculateStandardDeviation(sigmaVecInit);
    std::cout << "------------------------------------------------\n";
    std::cout << "[INFO] Standard Deviation: " << standardDeviation << std::endl;
    if (standardDeviation != 0)
    {
        T = 0.5 * standardDeviation;
    }
    std::cout << "[INFO] The simulated annealing algorithm starts " << std::endl;
    std::cout << "[INFO] initial temperature T= " << T << ", threshhold= " << threashhold << ", alpha= " << alpha << ", InnerIter= " << InnerIter << ", seed=" << seed << std::endl;

    bool timeup = false;
    int epsilon = 1; // 设置收敛阈值
    int exterIter = 0;
    int costPre = cost;
    // 外层循环 温度大于阈值， 更新一次fitness优先级列表
    while (T > threashhold)
    {
        // 记录接受的new_cost
        std::vector<int> sigmaVec;
        // 截断循环 判断是否收敛
        /*
        if(exterIter >= 10){
            exterIter = 0;
            int detaCost = std::abs(costPre - cost);
            // std::cout<<cost <<'-'<<costPre<<'='<<detaCost<<std::endl;
            if(detaCost < epsilon){
                break;
            }
            else{
                costPre = cost;
            }
        }
        exterIter++;  // exterIter + 1 = Iter + 2000 */
        // 内层循环 小于内层迭代次数
        while (Iter < InnerIter)
        {
            if (Iter % 100 == 0)
            {
                auto tmp = std::chrono::high_resolution_clock::now();
                // 计算运行时间
                std::chrono::duration<double> durationtmp = tmp - start;
                if (durationtmp.count() >= timeLimit)
                { // 1180
                    timeup = true;
                    break;
                }
                std::cout << "[INFO] T:" << std::scientific << std::setprecision(3) << T << " iter:" << std::setw(4) << Iter << " alpha:" << std::fixed << std::setprecision(2) << alpha << " cost:" << std::setw(7) << cost << std::endl;
            }
            // std::cout<<"[INFO] T:"<< std::scientific << std::setprecision(3) <<T <<" iter:"<<std::setw(4)<<Iter<<" alpha:"<<std::fixed<<std::setprecision(2)<<alpha<<" cost:"<<std::setw(7)<<cost<<std::endl;
            Iter++;
            // 根据fitness列表选择一个net
            int netId = selectNetId(fitnessVec);
#ifdef DEBUG
            std::cout << "DEBUG-netId:" << netId << std::endl;
#endif
            Net *net = glbPackNetMap[netId];
            // 随机选择net中的一个inst
            Instance *inst = selectInst(net);
            if (inst == nullptr)
            {
                // 没找到可移动的inst，跳过后续部分
                continue;
            }

#ifdef DEBUG
            std::cout << "DEBUG-instName:" << inst->getInstanceName() << std::endl;
#endif
            // 确定net的中心
            int centerX, centerY;
            std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
            // 在 rangeDesired 范围内选取一个位置去放置这个inst
            int x, y, z;
            std::tie(x, y, z) = findPackSuitableLoc(isBaseline, centerX, centerY, rangeDesiredMap[netId], inst, isSeqPack);
            if (z == -1)
            {
                // 没找到合适位置
                continue;
            }
            // 空间换时间
            // 找到这个inst附近的net
            std::set<int> instRelatedNetId;
            int instId = std::stoi(inst->getInstanceName().substr(5)); // inst_xxx 从第5个字符开始截取，转换为整数
            if (instRelatedNetIdMap.count(instId))
            {
                // 找得到
                instRelatedNetId = instRelatedNetIdMap[instId];
            }
            else
            {
                // 访问inputpin
                for (auto &pin : inst->getInpins())
                {
                    if (oldNetID2newNetID[pin->getNetID()] == 5925)
                    {
                        int a = 0;
                    }
                    int netId = pin->getNetID();
                    //-1表示未连接
                    if (netId != -1)
                        instRelatedNetId.insert(oldNetID2newNetID[pin->getNetID()]);
                }
                // 访问outputpin
                for (auto &pin : inst->getOutpins())
                {
                    if (oldNetID2newNetID[pin->getNetID()] == 5925)
                    {
                        int a = 0;
                    }

                    int netId = pin->getNetID();
                    //-1表示未连接
                    if (netId != -1)
                        instRelatedNetId.insert(oldNetID2newNetID[pin->getNetID()]);
                }
                if (inst->getMatchedLUTID() != -1 && (inst->getModelName()).substr(0, 3) == "LUT")
                {
                    // 将配对的net也加上
                    Instance *instMatch = glbInstMap[inst->getMatchedLUTID()];
                    // 访问inputpin
                    for (auto &pin : instMatch->getInpins())
                    {
                        int netId = pin->getNetID();
                        if (oldNetID2newNetID[pin->getNetID()] == 5925)
                        {
                            int a = 0;
                        }
                        //-1表示未连接
                        if (netId != -1)
                            instRelatedNetId.insert(oldNetID2newNetID[pin->getNetID()]);
                    }
                    // 访问outputpin
                    for (auto &pin : instMatch->getOutpins())
                    {
                        int netId = pin->getNetID();
                        if (oldNetID2newNetID[pin->getNetID()] == 5925)
                        {
                            int a = 0;
                        }
                        //-1表示未连接
                        if (netId != -1)
                            instRelatedNetId.insert(oldNetID2newNetID[pin->getNetID()]);
                    }
                    instRelatedNetIdMap[inst->getMatchedLUTID()] = instRelatedNetId; // 记录下来。下次这个instId就不需要运算了
                }
                instRelatedNetIdMap[instId] = instRelatedNetId; // 记录下来。下次这个instId就不需要运算了
            }

            // 计算移动后的newCost
            std::tuple<int, int, int> loc = std::make_tuple(x, y, z);
            std::tuple<int, int, int> originLoc;
            // 保存更新前的部分net
            int beforeNetWL = getPackRelatedWirelength(isBaseline, instRelatedNetId);
            if (isBaseline)
            {
                originLoc = inst->getBaseLocation();
                inst->setBaseLocation(loc);
                // if (inst->getMatchedLUTID() != -1)
                // {
                //     Instance *matchedInst = glbInstMap[inst->getMatchedLUTID()];
                //     matchedInst->setBaseLocation(loc);
                // }
            }
            else
            {
                originLoc = inst->getLocation();
                inst->setLocation(loc);
                if (inst->getMatchedLUTID() != -1)
                {
                    int a = 0;
                    // Instance *matchedInst = glbInstMap[inst->getMatchedLUTID()];
                    // matchedInst->setLocation(loc);
                }
            }
            int afterNetWL = getPackRelatedWirelength(isBaseline, instRelatedNetId);
            int costNew = cost - beforeNetWL + afterNetWL;
            // costNew = getHPWL(isBaseline);
            // deta = new_cost - cost
            int deta = costNew - cost;
#ifdef DEBUG
            std::cout << "DEBUG-deta:" << deta << std::endl;
#endif
            // if deta < 0 更新这个操作到布局中，更新fitness列表
            if (deta < 0)
            {
                changePackTile(isBaseline, originLoc, loc, inst, isSeqPack);
                // 间隔次数多了再更新这两
                // calculRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                // calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
                cost = costNew;
                sigmaVec.emplace_back(costNew);
                // sortedFitness(fitnessVec);
            }
            else
            {
                // else 取(0,1)随机数，判断随机数是否小于 e^(-deta/T) 是则同样更新操作，更新fitness列表
                // 生成一个 0 到 1 之间的随机浮点数
                double randomValue = generate_random_double(0.0, 1.0);
                double eDetaT = exp(-deta / T);
#ifdef DEBUG
                std::cout << "DEBUG-randomValue:" << randomValue << ", eDetaT:" << eDetaT << std::endl;
#endif
                if (randomValue < eDetaT)
                {
                    changePackTile(isBaseline, originLoc, loc, inst, isSeqPack);
                    // 间隔次数多了再更新这两
                    // calculRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                    // calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
                    cost = costNew;
                    sigmaVec.emplace_back(costNew);
                }
                else
                { // 复原
                    if (isBaseline)
                    {
                        inst->setBaseLocation(originLoc);
                        // if (inst->getMatchedLUTID() != -1)
                        // {
                        //     Instance *matchedInst = glbInstMap[inst->getMatchedLUTID()];
                        //     matchedInst->setBaseLocation(originLoc);
                        // }
                    }
                    else
                    {
                        inst->setLocation(originLoc);
                        // if (inst->getMatchedLUTID() != -1)
                        // {
                        //     Instance *matchedInst = glbInstMap[inst->getMatchedLUTID()];
                        //     matchedInst->setLocation(originLoc);
                        // }
                    }
                }
            }
            // counterNet 计数+1
            counterNet += 1;
            if (counterNet % 100 == 0)
            {
                calculPackRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                calculPackRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
            }
            // 当计数等于一个限制时，更新rangeActual 到 rangeDesired
            if (counterNet == counterNetLimit)
            {
                rangeDesiredMap = rangeActualMap;
                counterNet = 0;
            }
        }
        if (timeup)
            break; // 时间快到了，结束

        double acceptRate = sigmaVec.size() / InnerIter;
        if (0.96 <= acceptRate)
        {
            alpha = 0.5;
        }
        else if (0.8 <= acceptRate && acceptRate < 0.96)
        {
            alpha = 0.9;
        }
        else if (0.16 <= acceptRate && acceptRate < 0.8)
        {
            alpha = 0.95;
        }
        else
        {
            alpha = 0.8;
        }

        // T = alpha * T
        T = alpha * T;
        // Iter = 0
        Iter = 0;
        // 排序fitness列表
        sortedFitness(fitnessVec);
    }

    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算运行时间
    std::chrono::duration<double> duration = end - start;

    // 输出运行时间（单位为秒）
    std::cout << "runtime: " << duration.count() << " s" << std::endl;

    // 还原最终结果映射
    recoverAllMap(isSeqPack);

    return 0;
}

bool isPackValid(bool isBaseline, int x, int y, int &z, Instance *inst, bool isSeqPack)
{ // 判断这个位置是否可插入该inst，如果可插入则返回z值
    bool valid = false;
    Tile *tile = chip.getTile(x, y);
    // if(tile == NULL || tile->matchType("PLB") == false){
    //     return false;
    // }
    std::string instType = inst->getModelName();
    std::string instName = inst->getInstanceName();
    int instId = std::stoi(inst->getInstanceName().substr(5)); // inst_xxx 从第5个字符开始截取，转换为整数
    if (instType.substr(0, 3) == "LUT")
    {
        std::vector<int> hasDRAM(2, 0); //  1 表示有DRAM
        // 先判断是否有DRAM
        slotArr dramSlotArr = *(tile->getInstanceByType("DRAM"));
        slotArr lutSlotArr = *(tile->getInstanceByType("LUT"));
        int lutBegin = 0, lutEnd = 0;
        for (int idx = 0; idx < (int)dramSlotArr.size(); idx++)
        {
            Slot *slot = dramSlotArr[idx];
            if (slot == nullptr)
            {
                continue;
            }
            std::list<int> instances;
            if (isBaseline)
            {
                instances = slot->getBaselineInstances();
            }
            else
            {
                instances = slot->getOptimizedInstances();
            }
            if (instances.empty())
            {
                continue;
            }
            // DRAM at slot0 blocks lut slot 0~3
            // DRAM at slot1 blocks lut slot 4~7
            hasDRAM[idx] = 1;
        }
        if (hasDRAM[0] && hasDRAM[1])
        {
            // 两个都是DRAM，不可放置
            return false;
        }
        // 0-3不可放
        else if (hasDRAM[0])
        {
            lutBegin = 4;
            lutEnd = 8;
        }
        // 4-7不可放
        else if (hasDRAM[1])
        {
            lutBegin = 0;
            lutEnd = 4;
        }
        // 都可以放
        else
        {
            lutBegin = 0;
            lutEnd = 8;
        }
        // 新加的 cjq 1104
        if (inst->getMatchedLUTID() != -1 && (inst->getModelName()).substr(0, 3) == "LUT")
        {
            for (int idx = lutBegin; idx < lutEnd; idx++)
            {
                Slot *slot = lutSlotArr[idx];
                if (slot == nullptr)
                {
                    continue;
                }
                std::list<int> instances;
                if (isBaseline)
                {
                    instances = slot->getBaselineInstances();
                }
                else
                {
                    instances = slot->getOptimizedInstances();
                }
                if (instances.size() == 0)
                {
                    // 找到整个位置都是空的
                    z = idx;
                    return true;
                }
            }
            return false;
        }
        std::vector<std::pair<int, int>> record; // 记录每个引脚小于6的 lut  [idx, pinNum]
        for (int idx = lutBegin; idx < lutEnd; idx++)
        {
            Slot *slot = lutSlotArr[idx];
            if (slot == nullptr)
            {
                continue;
            }
            std::list<int> instances;
            if (isBaseline)
            {
                instances = slot->getBaselineInstances();
            }
            else
            {
                instances = slot->getOptimizedInstances();
            }

            if (instances.size() == 1)
            {
                Instance *instanceTmp = glbInstMap[instances.front()];
                int inttmp = instanceTmp->getMapInstID().size();

                if (inttmp == 2)
                {
                    int a = 0;
                    continue;
                }                

                // 大于1个lut，不可放入了
                instances.push_back(instId); // 添加当前instId
                std::set<int> totalInputs;
                for (auto instID : instances)
                {
                    Instance *instPtr = glbInstMap.find(instID)->second;
                    std::vector<Pin *> inpins = instPtr->getInpins();
                    for (auto pin : inpins)
                    {
                        if (pin->getNetID() != -1)
                        {
                            totalInputs.insert(pin->getNetID());
                        }
                    }
                }
                if (totalInputs.size() <= 6)
                {
                    // 符合条件 记录
                    record.emplace_back(std::make_pair(idx, totalInputs.size()));
                }
            }
            if (instances.size() == 0)
            {
                // 大于1个lut，不可放入了
                instances.push_back(instId); // 添加当前instId
                std::set<int> totalInputs;
                for (auto instID : instances)
                {
                    Instance *instPtr = glbInstMap.find(instID)->second;
                    std::vector<Pin *> inpins = instPtr->getInpins();
                    for (auto pin : inpins)
                    {
                        if (pin->getNetID() != -1)
                        {
                            totalInputs.insert(pin->getNetID());
                        }
                    }
                }
                if (totalInputs.size() <= 6)
                {
                    // 符合条件 记录
                    record.emplace_back(std::make_pair(idx, totalInputs.size()));
                }
            }
        }
        if (record.size() != 0)
        {
            // 按第二个元素降序排序
            std::sort(record.begin(), record.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b)
                      {
                          return a.second > b.second; // 使用 > 实现降序
                      });
            if (z > 8)
            {
                int a = 0;
            }

            z = record[0].first;
            return true;
        }
    }
    if (isSeqPack)
    {
        if (inst->getModelName() == "SEQ")
        { // LUT可以不用改，SEQ需要大改，现在的SEQ为一个bank组，所以返回的应该是bank的位置，0或1，只需要检查是否有空余 bank 就行
            slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
            bool zeroFlag = true;
            bool oneFlag = true;
            for (int i = 0; i < 15; i++)
            {
                Slot *slot = seqSlotArr[i];
                if (slot->getOptimizedInstances().size() != 0 && i < 8)
                {
                    zeroFlag = false;
                }
                if (slot->getOptimizedInstances().size() != 0 && i >= 8)
                {
                    oneFlag = false;
                }
            }
            if (zeroFlag)
            {
                z = 0;
                valid = true;
            }
            if (!zeroFlag && oneFlag)
            {
                z = 1;
                valid = true;
            }
            if (!zeroFlag && !oneFlag)
            {
                valid = false;
            }
        }
    }
    else
    {
        if (inst->getModelName() == "SEQ")
        {
            slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
            for (int bank = 0; bank < 2; bank++)
            {
                // bank0 0-8   bank1 8-16
                int start = bank * 8;
                int end = (bank + 1) * 8;
                std::set<int> instIdSet;
                instIdSet.insert(instId); // 添加当前Id
                for (int i = start; i < end; i++)
                {
                    Slot *slot = seqSlotArr[i];
                    std::list<int> instList;
                    if (isBaseline)
                        instList = slot->getBaselineInstances();
                    else
                        instList = slot->getOptimizedInstances();
                    for (int id : instList)
                    {
                        instIdSet.insert(id);
                    }
                }
                // 判断每个inst的引脚
                std::set<int> clkNets; // 不超过1
                std::set<int> ceNets;  // 不超过2
                std::set<int> srNets;  // 不超过1
                for (int i : instIdSet)
                {
                    Instance *instPtr = glbInstMap[i];
                    int numInpins = instPtr->getNumInpins();
                    for (int i = 0; i < numInpins; i++)
                    {
                        // 只判断input是否是这几个量
                        Pin *pin = instPtr->getInpin(i);
                        int netID = pin->getNetID();
                        if (netID >= 0)
                        {
                            PinProp prop = pin->getProp();
                            if (prop == PIN_PROP_CE)
                            {
                                ceNets.insert(netID);
                            }
                            else if (prop == PIN_PROP_CLOCK)
                            {
                                clkNets.insert(netID);
                            }
                            else if (prop == PIN_PROP_RESET)
                            {
                                srNets.insert(netID);
                            }
                        }
                    }
                }

                int numClk = clkNets.size();
                int numReset = srNets.size();
                int numCe = ceNets.size();

                if (numClk > MAX_TILE_CLOCK_PER_PLB_BANK || numCe > MAX_TILE_CE_PER_PLB_BANK || numReset > MAX_TILE_RESET_PER_PLB_BANK)
                {
                    // 该bank放入该inst时违反约束
                    valid = false;
                }
                else
                {
                    // SEQ只要返回一个空位子即可
                    // slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
                    for (int i = start; i < end; i++)
                    {
                        Slot *slot = seqSlotArr[i];
                        std::list<int> listTmp;
                        if (isBaseline)
                            listTmp = slot->getBaselineInstances();
                        else
                            listTmp = slot->getOptimizedInstances();
                        if (listTmp.size() == 0)
                        {
                            z = i;
                            return true;
                        }
                    }
                }
            }
        }
    }

    return valid;
}

std::tuple<int, int, int> findPackSuitableLoc(bool isBaseline, int x, int y, int rangeDesired, Instance *inst, bool isSeqPack)
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
    std::vector<std::pair<int, int>> coordinates;
    for (int x = xl; x <= xr; ++x)
    {
        for (int y = yl; y <= yr; ++y)
        {
            if (isPLB[x][y])
            {                                   // 只加入PLB块
                coordinates.emplace_back(x, y); // 将坐标 (x, y) 加入向量
            }
        }
    }
    int xx, yy, zz = -1;  // 目标坐标
    int xCur, yCur, zCur; // 当前坐标
    // 目前inst所在位置
    if (isBaseline)
        std::tie(xCur, yCur, zCur) = inst->getBaseLocation();
    else
        std::tie(xCur, yCur, zCur) = inst->getLocation();

    // 进行随机抽取
    while (!coordinates.empty())
    {
        int randomIndex = generate_random_int(0, coordinates.size() - 1);
        ;
        xx = coordinates[randomIndex].first;
        yy = coordinates[randomIndex].second;
#ifdef DEBUG
        std::cout << "DEBUG-Selected target coordinates: (" << xx << ", " << yy << ")" << std::endl;
#endif
        if (xCur == xx && yCur == yy)
        {
#ifdef DEBUG
            std::cout << "DEBUG-The coordinates (" << xx << ", " << yy << ") are consistent with the original coordinates (" << xCur << ", " << yCur << ")" << std::endl;
#endif
            // 移除不符合规则的坐标
            coordinates.erase(coordinates.begin() + randomIndex);
            continue;
        }
        // 检查坐标是否符合规则
        if (isPackValid(isBaseline, xx, yy, zz, inst, isSeqPack))
        {
            break;
        }
        else
        {
#ifdef DEBUG
            std::cout << "DEBUG-The coordinates (" << xx << ", " << yy << ") do not conform to the constraints" << std::endl;
#endif
            // 移除不符合规则的坐标
            coordinates.erase(coordinates.begin() + randomIndex);
        }
    }
    return {xx, yy, zz};
}

// 修改slot队列
int changePackTile(bool isBaseline, std::tuple<int, int, int> originLoc, std::tuple<int, int, int> loc, Instance *inst, bool isSeqPack)
{
    int xCur, yCur, zCur, xGoal, yGoal, zGoal;
    std::tie(xCur, yCur, zCur) = originLoc;
    std::tie(xGoal, yGoal, zGoal) = loc;
    Tile *tileCur = chip.getTile(xCur, yCur);
    Tile *tileGoal = chip.getTile(xGoal, yGoal);
    int instId = std::stoi(inst->getInstanceName().substr(5)); // inst_xxx 从第5个字符开始截取，转换为整数
    // 删除旧的tile插槽中的inst
    slotArr *slotArrCur = tileCur->getInstanceByType(inst->getModelName().substr(0, 3)); // LUT or SEQ
    Slot *slot = slotArrCur->at(zCur);
    if (inst->getMatchedLUTID() != -1 && (inst->getModelName()).substr(0, 3) == "LUT")
    {
        if (isBaseline)
            slot->clearBaselineInstances();
        else
            slot->clearOptimizedInstances();
        // 添加到新的tile
        tileGoal->addInstance(instId, zGoal, inst->getModelName(), isBaseline);
        // tileGoal->addInstance(inst->getMatchedLUTID(), zGoal, inst->getModelName(), isBaseline);
    }
    if (inst->getMatchedLUTID() == -1 && (inst->getModelName()).substr(0, 3) == "LUT")
    {
        if (isBaseline)
        {
            std::list<int> &instances = slot->getBaselineInstances();
            for (int instIdTmp : instances)
            {
                if (instId == instIdTmp)
                {
                    instances.remove(instIdTmp);
                    break;
                }
            }
        }
        else
        {
            std::list<int> &instances = slot->getOptimizedInstancesRef();
            for (int instIdTmp : instances)
            {
                if (instId == instIdTmp)
                {
                    instances.remove(instIdTmp);
                    break;
                }
            }
        }
        // 在新的插槽中插入
        tileGoal->addInstance(instId, zGoal, inst->getModelName(), isBaseline);
    }

    if ((inst->getModelName()).substr(0, 3) == "SEQ")
    {
        if (isSeqPack)
        {
            if (isBaseline)
            {
                std::list<int> &instances = slot->getBaselineInstances();
                for (int instIdTmp : instances)
                {
                    if (instId == instIdTmp)
                    {
                        instances.remove(instIdTmp);
                        break;
                    }
                }
            }
            else
            {
                std::list<int> &instances = slot->getOptimizedInstancesRef();
                for (int instIdTmp : instances)
                {
                    if (instId == instIdTmp)
                    {
                        instances.remove(instIdTmp);
                        break;
                    }
                }
            }
            // 在新的插槽中插入
            if (zGoal == 0)
            {
                for (size_t i = 0; i < inst->getMapInstID().size(); i++)
                {
                    tileGoal->addInstance(inst->getMapInstID()[i], i, inst->getModelName(), isBaseline);
                }
            }
            if (zGoal == 1)
            {
                for (size_t i = 0; i < inst->getMapInstID().size(); i++)
                {
                    tileGoal->addInstance(inst->getMapInstID()[i], i + 8, inst->getModelName(), isBaseline);
                }
            }
        }
        else
        {
            if (isBaseline)
            {
                std::list<int> &instances = slot->getBaselineInstances();
                for (int instIdTmp : instances)
                {
                    if (instId == instIdTmp)
                    {
                        instances.remove(instIdTmp);
                        break;
                    }
                }
            }
            else
            {
                std::list<int> &instances = slot->getOptimizedInstancesRef();
                for (int instIdTmp : instances)
                {
                    if (instId == instIdTmp)
                    {
                        instances.remove(instIdTmp);
                        break;
                    }
                }
            }
            // 在新的插槽中插入
            tileGoal->addInstance(instId, zGoal, inst->getModelName(), isBaseline);
        }
    }

    return 0;
}

// 优化 只计算相关的fitness
int calculPackRelatedFitness(std::vector<std::pair<int, float>> &fitnessVec, std::map<int, int> &rangeDesiredMap, std::map<int, int> &rangeActualMap, const std::set<int> &instRelatedNetId)
{
    // 计算fitness
    int n = fitnessVec.size();
    for (auto netId : instRelatedNetId)
    {
        if (glbPackNetMap.count(netId) <= 0)
        {
            std::cout << "calculPackRelatedFitness can not find this netId:" << netId << std::endl;
            continue;
        }
        int index = -1;
        // 找到匹配netId的下标
        for (int i = 0; i < fitnessVec.size(); ++i)
        {
            if (fitnessVec[i].first == netId)
            {
                index = i;
                break;
            }
        }
        if (index == -1)
        {
            std::cout << "can not find this netId: " << netId << ", something may be wrong" << std::endl;
            exit(1);
        }
        int rangeDesired = rangeDesiredMap[netId];
        int rangeActual = rangeActualMap[netId];
        float fitness;
        if (rangeDesired == 0 && rangeActual == 0)
        {
            fitness = 1; // 都为0则不考虑移动了，认为为最完美的net
        }
        else if (rangeDesired >= rangeActual)
        {
            fitness = rangeActual / rangeDesired;
        }
        else
        {
            fitness = rangeDesired / rangeActual;
        }
        fitnessVec[index] = std::make_pair(netId, fitness);
    }
    return 0;
}

// 优化，循环体中只计算相关的
int calculPackRelatedRangeMap(bool isBaseline, std::map<int, int> &rangeActualMap, const std::set<int> &instRelatedNetId)
{
    for (int i : instRelatedNetId)
    {
        if (glbNetMap.count(i) <= 0)
        {
            std::cout << "calculPackRelatedRangeMap can not find this netId:" << i << std::endl;
            continue;
        }
        Net *net = glbNetMap[i];
        // 访问net input 引脚
        Instance *instIn = net->getInpin()->getInstanceOwner();
        int x, y, z;
        if (isBaseline)
        {
            std::tie(x, y, z) = instIn->getBaseLocation();
        }
        else
        {
            std::tie(x, y, z) = instIn->getLocation();
        }
        int maxX, minX, maxY, minY;
        maxX = minX = x;
        maxY = minY = y;
        // 访问net output 引脚
        std::list<Pin *> outputPins = net->getOutputPins();
        for (Pin *pin : outputPins)
        {
            Instance *instTmp = pin->getInstanceOwner();
            if (isBaseline)
            {
                std::tie(x, y, z) = instTmp->getBaseLocation();
            }
            else
            {
                std::tie(x, y, z) = instTmp->getLocation();
            }
            if (maxX < x)
                maxX = x;
            if (minX > x)
                minX = x;
            if (maxY < y)
                maxY = y;
            if (minY > y)
                minY = y;
        }
        int netDesired = std::ceil((maxX - minX + maxY - minY) / 2);
        // int netDesired = std::ceil((maxX-minX+maxY-minY));
        rangeActualMap[net->getId()] = netDesired;
    }
    return 0;
}

// 计算rangeActulMap
int calculPackrangeMap(bool isBaseline, std::map<int, int> &rangeActualMap)
{
    for (auto iter : glbPackNetMap)
    {
        Net *net = iter.second;
        // 访问net input 引脚
        Instance *instIn = net->getInpin()->getInstanceOwner();
        int x, y, z;
        if (isBaseline)
        {
            std::tie(x, y, z) = instIn->getBaseLocation();
        }
        else
        {
            std::tie(x, y, z) = instIn->getLocation();
        }
        int maxX, minX, maxY, minY;
        maxX = minX = x;
        maxY = minY = y;
        // 访问net output 引脚
        std::list<Pin *> outputPins = net->getOutputPins();
        for (Pin *pin : outputPins)
        {
            Instance *instTmp = pin->getInstanceOwner();
            if (isBaseline)
            {
                std::tie(x, y, z) = instTmp->getBaseLocation();
            }
            else
            {
                std::tie(x, y, z) = instTmp->getLocation();
            }
            if (maxX < x)
                maxX = x;
            if (minX > x)
                minX = x;
            if (maxY < y)
                maxY = y;
            if (minY > y)
                minY = y;
        }
        int netDesired = std::ceil((maxX - minX + maxY - minY) / 2);
        // int netDesired = std::ceil((maxX-minX+maxY-minY));
        rangeActualMap[net->getId()] = netDesired;
    }
    return 0;
}