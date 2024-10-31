#include <iomanip>
#include "global.h"
#include "object.h"
#include "arbsa.h"
#include <algorithm>
#include <cmath>
#include "wirelength.h"
#include <random>
// 计时
#include <chrono>

// #define DEBUG

// 全局随机数生成器
std::mt19937& get_random_engine() {
    // static std::random_device rd;  // 用于生成种子
    static std::mt19937 gen; // 使用随机设备生成种子的Mersenne Twister随机数引擎
    return gen;
}
// 设置随机种子
void set_random_seed(unsigned int seed) {
    get_random_engine().seed(seed);
}
// 生成一个 [min, max] 区间的随机整数
int generate_random_int(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(get_random_engine());
}
// 生成 [min, max] 范围内的随机浮点数
double generate_random_double(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(get_random_engine());
}

//计算标准差
double calculateStandardDeviation(const std::vector<int>& data) {
    if (data.empty()) {
        return 0.0; // 如果数据为空，返回0或处理异常
    }
    
    // 计算平均值
    double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    
    // 计算每个元素与平均值的差值的平方和
    double variance = 0.0;
    for (int value : data) {
        variance += (value - mean) * (value - mean);
    }
    
    // 方差除以数据数量并开平方得到标准差
    return std::sqrt(variance / data.size());
}

//计算rangeActulMap
int calculrangeMap(bool isBaseline, std::map<int, int>& rangeActualMap){
    for(auto iter : glbNetMap){
        Net *net = iter.second;
        // 访问net input 引脚
        Instance* instIn = net->getInpin()->getInstanceOwner();
        int x, y, z;
        if(isBaseline){
            std::tie(x, y, z) = instIn->getBaseLocation();
        }
        else{
            std::tie(x, y, z) = instIn->getLocation();
        }
        int maxX, minX, maxY, minY;
        maxX = minX = x;
        maxY = minY = y;
        // 访问net output 引脚
        std::list<Pin*> outputPins = net->getOutputPins();
        for(Pin *pin : outputPins){
            Instance* instTmp = pin->getInstanceOwner();
            if(isBaseline){
                std::tie(x, y, z) = instTmp->getBaseLocation();
            }
            else{
                std::tie(x, y, z) = instTmp->getLocation();
            }
            if(maxX < x) maxX = x;
            if(minX > x) minX = x;
            if(maxY < y) maxY = y;
            if(minY > y) minY = y;
        }
        int netDesired = std::ceil((maxX-minX+maxY-minY)/2);
        rangeActualMap[net->getId()] = netDesired;
    }
    return 0;
}

//优化，循环体中只计算相关的
int calculRelatedRangeMap(bool isBaseline, std::map<int, int>& rangeActualMap, const std::set<int>& instRelatedNetId){
    for(int i : instRelatedNetId){
        if(glbNetMap.count(i) <= 0){
            std::cout<<"calculRelatedRangeMap can not find this netId:"<<i<<std::endl;
            continue;
        }
        Net *net = glbNetMap[i];
        // 访问net input 引脚
        Instance* instIn = net->getInpin()->getInstanceOwner();
        int x, y, z;
        if(isBaseline){
            std::tie(x, y, z) = instIn->getBaseLocation();
        }
        else{
            std::tie(x, y, z) = instIn->getLocation();
        }
        int maxX, minX, maxY, minY;
        maxX = minX = x;
        maxY = minY = y;
        // 访问net output 引脚
        std::list<Pin*> outputPins = net->getOutputPins();
        for(Pin *pin : outputPins){
            Instance* instTmp = pin->getInstanceOwner();
            if(isBaseline){
                std::tie(x, y, z) = instTmp->getBaseLocation();
            }
            else{
                std::tie(x, y, z) = instTmp->getLocation();
            }
            if(maxX < x) maxX = x;
            if(minX > x) minX = x;
            if(maxY < y) maxY = y;
            if(minY > y) minY = y;
        }
        int netDesired = std::ceil((maxX-minX+maxY-minY)/2);
        rangeActualMap[net->getId()] = netDesired;
    }
    return 0;
}

//计算Fitness
int calculFitness(std::vector<std::pair<int,float>>& fitnessVec, std::map<int, int>& rangeDesiredMap, std::map<int, int>& rangeActualMap){ 
    // 计算fitness
    int n = fitnessVec.size();
    for(int i = 0; i < n; i++){
        int netId = fitnessVec[i].first; 
        float fitness;
        int rangeDesired = rangeDesiredMap[netId];
        int rangeActual = rangeActualMap[netId];
        if(rangeDesired == 0 && rangeActual == 0){
            fitness = 1; //都为0则不考虑移动了，认为为最完美的net
        }
        else if(rangeDesired >= rangeActual){
            fitness = rangeActual / rangeDesired;
        }
        else{
            fitness = rangeDesired / rangeActual;
        }
        fitnessVec[i] = std::make_pair(netId, fitness);
    }
    return 0;
}

//优化 只计算相关的fitness
int calculRelatedFitness(std::vector<std::pair<int,float>>& fitnessVec, std::map<int, int>& rangeDesiredMap, std::map<int, int>& rangeActualMap, const std::set<int>& instRelatedNetId){ 
    // 计算fitness
    int n = fitnessVec.size();
    for(auto netId : instRelatedNetId){
        if(glbNetMap.count(netId) <= 0){
            std::cout<<"calculRelatedFitness can not find this netId:"<<netId<<std::endl;
            continue;
        }
        int index = -1; 
        //找到匹配netId的下标
        for (int i = 0; i < fitnessVec.size(); ++i) {
            if (fitnessVec[i].first == netId) {
                index = i;
                break;
            }
        }
        if(index == -1){
            std::cout<<"can not find this netId: "<<netId <<", something may be wrong"<<std::endl;
            exit(1);
        }
        int rangeDesired = rangeDesiredMap[netId];
        int rangeActual = rangeActualMap[netId];
        float fitness;
        if(rangeDesired == 0 && rangeActual == 0){
            fitness = 1; //都为0则不考虑移动了，认为为最完美的net
        }
        else if(rangeDesired >= rangeActual){
            fitness = rangeActual / rangeDesired;
        }
        else{
            fitness = rangeDesired / rangeActual;
        }
        fitnessVec[index] = std::make_pair(netId, fitness);
    }
    return 0;
}

int sortedFitness(std::vector<std::pair<int,float>>& fitnessVec){ //将fitnessVec按第二个值升序排列
    std::sort(fitnessVec.begin(), fitnessVec.end(), [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
        return a.second < b.second; // 按照 float 值从小到大排序
    });
    return 0;
}

int selectNetId(std::vector<std::pair<int,float>>& fitnessVec){ //返回具有一定规律随机选取的net
    int n = fitnessVec.size();
    int a = generate_random_int(0, n - 1); // 范围在 0 到 n - 1
    int b = generate_random_int(0, n - 1);
    int index = std::min(a, b);
    int netId = fitnessVec[index].first;
    return netId;
}

Instance* selectInst(Net *net){ //返回在net中随机选取的instance指针
    std::list<Pin*> pinList = net->getOutputPins();
    pinList.emplace_back(net->getInpin());
    int n = pinList.size(); // 
    Instance* inst = nullptr;
    std::vector<int> numbers(n); // 0到n-1的数字
    std::iota(numbers.begin(), numbers.end(), 0); // 填充从0到n的数字
    while (!numbers.empty()) {
        int randomIndex = generate_random_int(0, numbers.size() - 1);
        int index = numbers[randomIndex];

        // 创建一个迭代器指向 list 的开始
        std::list<Pin*>::iterator it = pinList.begin();
        // 使用 std::advance 移动迭代器到指定下标
        std::advance(it, index);
        inst = (*it)->getInstanceOwner();
        
        // 检查是否符合规则
        if (inst->isFixed()) {
            // 移除不符合规则的数字
            numbers.erase(numbers.begin() + randomIndex);
            inst = nullptr;
        } else {
            break;
        }
    }
    return inst;
}

std::pair<int,int> getNetCenter(bool isBaseline, Net *net){ //返回net的中心位置
    int x = 0, y = 0;
    std::set<int> visitInst; // 记录访问过的inst的id，防止一个inst有多个引脚在同一net且计算多次的情况
    // 统计inpin
    Instance* inst = net->getInpin()->getInstanceOwner();
    int xt, yt, zt;
    if(isBaseline){
        std::tie(xt, yt, zt) = inst->getBaseLocation();
    }
    else{
        std::tie(xt, yt, zt) = inst->getLocation();
    }
    x += xt;
    y += yt;
    //标记为访问过
    std::string instName = inst->getInstanceName();
    size_t underscorePos = instName.find('_');
    if (underscorePos != std::string::npos) {
      std::string subStr = instName.substr(underscorePos + 1);
      // Convert the second substring to an integer
      int instId = std::stoi(subStr);
      visitInst.insert(instId);
    }
    //统计outputpin
    std::list<Pin*> pinList = net->getOutputPins();
    for(const auto& pin : pinList){
        Instance* inst = pin->getInstanceOwner();
        //判断是否重复出现
        std::string instName = inst->getInstanceName();
        size_t underscorePos = instName.find('_');
        if (underscorePos != std::string::npos) {
            std::string subStr = instName.substr(underscorePos + 1);
            // Convert the second substring to an integer
            int instId = std::stoi(subStr);
            auto it = visitInst.find(instId);
            if (it != visitInst.end()) {
                //重复的inst，跳过
                continue;
            }  
            else visitInst.insert(instId);
        }
        if(isBaseline){
            std::tie(xt, yt, zt) = inst->getBaseLocation();
        }
        else{
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

bool isValid(bool isBaseline, int x, int y, int& z, Instance* inst){ //判断这个位置是否可插入该inst，如果可插入则返回z值
    bool valid = false;
    Tile* tile = chip.getTile(x, y);
    // if(tile == NULL || tile->matchType("PLB") == false){
    //     return false;
    // }
    std::string instType = inst->getModelName();
    std::string instName = inst->getInstanceName();
    int instId = std::stoi(inst->getInstanceName().substr(5));  // inst_xxx 从第5个字符开始截取，转换为整数
    if(instType.substr(0,3) == "LUT"){
        std::vector<int> hasDRAM(2, 0); //  1 表示有DRAM
        //先判断是否有DRAM
        slotArr dramSlotArr = *(tile->getInstanceByType("DRAM"));
        slotArr lutSlotArr = *(tile->getInstanceByType("LUT"));
        int lutBegin = 0, lutEnd = 0;
        for (int idx = 0; idx < (int)dramSlotArr.size(); idx++) {
            Slot* slot = dramSlotArr[idx];
            if (slot == nullptr) {
                continue;
            }
            std::list<int> instances;
            if (isBaseline) {
                instances = slot->getBaselineInstances();
            } else {
                instances = slot->getOptimizedInstances();
            }
            if (instances.empty()) {
                continue;
            } 
            // DRAM at slot0 blocks lut slot 0~3
            // DRAM at slot1 blocks lut slot 4~7
            hasDRAM[idx] = 1;
        }
        if(hasDRAM[0] && hasDRAM[1]){
            //两个都是DRAM，不可放置
            return false;
        }
        // 0-3不可放
        else if(hasDRAM[0]){
            lutBegin = 4;
            lutEnd = 8;
        }
        // 4-7不可放
        else if(hasDRAM[1]){
            lutBegin = 0;
            lutEnd = 4;
        }
        // 都可以放
        else{
            lutBegin = 0;
            lutEnd = 8;
        }
        std::vector<std::pair<int, int> > record;  //记录每个引脚小于6的 lut  [idx, pinNum]
        for (int idx = lutBegin; idx < lutEnd; idx++) {
            Slot* slot = lutSlotArr[idx];
            if (slot == nullptr) {
                continue;
            }
            std::list<int> instances;
            if (isBaseline) {
                instances = slot->getBaselineInstances();
            } else {
                instances = slot->getOptimizedInstances();
            }
            if(instances.size() <= 1){
                //大于1个lut，不可放入了
                instances.push_back(instId); //添加当前instId
                std::set<int> totalInputs;
                for (auto instID : instances) {
                    Instance* instPtr = glbInstMap.find(instID)->second;
                    std::vector<Pin*> inpins = instPtr->getInpins();
                    for (auto pin : inpins) {
                        if (pin->getNetID() != -1) {
                            totalInputs.insert(pin->getNetID());
                        }
                    }
                }
                if (totalInputs.size() <= 6) {
                    //符合条件 记录
                    record.emplace_back(std::make_pair(idx, totalInputs.size()));
                }
            }
        }
        if(record.size() != 0){
            // 按第二个元素降序排序
            std::sort(record.begin(), record.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                return a.second > b.second;  // 使用 > 实现降序
            });
            z = record[0].first;
            return true;
        }
    }
    else if(inst->getModelName() == "SEQ"){
        slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
        for (int bank = 0; bank < 2; bank++) {
            // bank0 0-8   bank1 8-16
            int start = bank * 8;
            int end = (bank + 1) * 8;
            std::set<int> instIdSet;
            instIdSet.insert(instId); // 添加当前Id
            for(int i = start; i < end; i++){
                Slot* slot = seqSlotArr[i];
                std::list<int> instList;
                if(isBaseline) instList = slot->getBaselineInstances();
                else instList = slot->getOptimizedInstances();
                for(int id : instList){
                    instIdSet.insert(id);
                }
            }
            //判断每个inst的引脚
            std::set<int> clkNets; //不超过1
            std::set<int> ceNets; //不超过2
            std::set<int> srNets; //不超过1
            for(int i : instIdSet){
                Instance* instPtr = glbInstMap[i];
                int numInpins = instPtr->getNumInpins();
                for (int i = 0; i < numInpins; i++) {
                    //只判断input是否是这几个量
                    Pin *pin = instPtr->getInpin(i);
                    int netID = pin->getNetID();
                    if (netID >= 0) {
                        PinProp prop = pin->getProp();
                        if (prop == PIN_PROP_CE){
                            ceNets.insert(netID);
                        } else if (prop == PIN_PROP_CLOCK) {
                            clkNets.insert(netID);
                        } else if (prop == PIN_PROP_RESET) {
                            srNets.insert(netID);
                        }
                    }
                }
            }

            int numClk = clkNets.size();      
            int numReset = srNets.size();    
            int numCe = ceNets.size();
            
            if (numClk > MAX_TILE_CLOCK_PER_PLB_BANK || numCe > MAX_TILE_CE_PER_PLB_BANK || numReset > MAX_TILE_RESET_PER_PLB_BANK) {
                //该bank放入该inst时违反约束
                valid = false;
            }
            else{
                // SEQ只要返回一个空位子即可
                // slotArr seqSlotArr = *(tile->getInstanceByType("SEQ"));
                for(int i = start; i < end; i++){
                    Slot *slot = seqSlotArr[i];
                    std::list<int> listTmp;
                    if(isBaseline)  listTmp = slot->getBaselineInstances();
                    else listTmp = slot->getOptimizedInstances();
                    if(listTmp.size() == 0){
                        z = i;
                        return true;
                    }
                }
            }
        }
    }

    return valid;
}

std::tuple<int,int,int> findSuitableLoc(bool isBaseline, int x, int y, int rangeDesired, Instance* inst){
  
    // 定义矩形框的左下角和右上角
    int xl = x - rangeDesired, yl = y - rangeDesired, xr = x + rangeDesired, yr = y + rangeDesired;
    int numCol = chip.getNumCol()-1;
    int numRow = chip.getNumRow()-1;
    // 判断不超出范围
    if(xl < 0) xl = 0;
    if(yl < 0) yl = 0;
    if(xr > numCol) xr = numCol;
    if(yr > numRow) yr = numRow;

    // 生成矩形框内的所有坐标
    std::vector<std::pair<int, int>> coordinates;
    for (int x = xl; x <= xr; ++x) {
        for (int y = yl; y <= yr; ++y) {
            if(isPLB[x][y]){ //只加入PLB块
                coordinates.emplace_back(x, y); // 将坐标 (x, y) 加入向量
            }
        }
    }
    int xx, yy, zz = -1; //目标坐标
    int xCur, yCur, zCur; //当前坐标
    //目前inst所在位置
    if(isBaseline) std::tie(xCur, yCur, zCur) = inst->getBaseLocation();
    else std::tie(xCur, yCur, zCur) = inst->getLocation();

    // 进行随机抽取
    while (!coordinates.empty()) {
        int randomIndex = generate_random_int(0, coordinates.size() - 1);;
        xx = coordinates[randomIndex].first;
        yy = coordinates[randomIndex].second;
        #ifdef DEBUG
            std::cout << "DEBUG-Selected target coordinates: (" << xx << ", " << yy << ")" << std::endl;
        #endif
        if(xCur == xx && yCur == yy){
            #ifdef DEBUG
                std::cout << "DEBUG-The coordinates (" << xx << ", " << yy << ") are consistent with the original coordinates (" << xCur << ", " << yCur << ")"<< std::endl;
            #endif
            // 移除不符合规则的坐标
            coordinates.erase(coordinates.begin() + randomIndex);
            continue;
        }
        // 检查坐标是否符合规则
        if (isValid(isBaseline, xx, yy, zz, inst)) {
            break;
        } else {
            #ifdef DEBUG
                std::cout << "DEBUG-The coordinates (" << xx << ", " << yy << ") do not conform to the constraints" << std::endl;
            #endif
            // 移除不符合规则的坐标
            coordinates.erase(coordinates.begin() + randomIndex);
        }
    }
    return {xx, yy, zz};
}

//修改slot队列
int changeTile(bool isBaseline, std::tuple<int, int, int> originLoc, std::tuple<int, int, int> loc, Instance* inst){
    int xCur, yCur, zCur, xGoal, yGoal, zGoal;
    std::tie(xCur, yCur, zCur) = originLoc;
    std::tie(xGoal, yGoal, zGoal) = loc;
    Tile* tileCur = chip.getTile(xCur, yCur);
    Tile* tileGoal = chip.getTile(xGoal, yGoal);
    int instId = std::stoi(inst->getInstanceName().substr(5));  // inst_xxx 从第5个字符开始截取，转换为整数
    //删除旧的tile插槽中的inst
    slotArr *slotArrCur = tileCur->getInstanceByType(inst->getModelName().substr(0,3)); //LUT or SEQ
    Slot* slot = slotArrCur->at(zCur);
    if (isBaseline) {
        std::list<int>& instances = slot->getBaselineInstances();
        for(int instIdTmp : instances){
            if(instId == instIdTmp){
                instances.remove(instIdTmp);
                break;
            }
        }
    } else {
        std::list<int>& instances = slot->getOptimizedInstancesRef();
        for(int instIdTmp : instances){
            if(instId == instIdTmp){
                instances.remove(instIdTmp);
                break;
            }
        }
    }
    

    //在新的插槽中插入
    tileGoal->addInstance(instId, zGoal, inst->getModelName(), isBaseline);

    return 0;
}

int arbsa(bool isBaseline){
    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 初始布局
    
    // 构造 fitness 优先级列表 初始化 rangeDesired  
    std::vector<std::pair<int,float>> fitnessVec; // 第一个是netId，第二个是适应度fitness, 适应度越小表明越需要移动。后续会按照fitness升序排列
    std::map<int, int> rangeDesiredMap; // 第一个是netId，第二个是外框矩形的平均跨度，即半周线长的一半
    calculrangeMap(isBaseline, rangeDesiredMap);  
    for(auto it : glbNetMap){
        Net *net = it.second;
        //构造fitnessVec
        fitnessVec.emplace_back(std::make_pair(net->getId(), 0));
    }
    std::map<int, int> rangeActualMap(rangeDesiredMap); // 第一个是netId，第二个是当前实际的外框矩形的平均跨度，即半周线长的一半
    
    // 计算 fitness 并 排序
    calculFitness(fitnessVec, rangeDesiredMap, rangeActualMap);
    sortedFitness(fitnessVec);

    // 初始化迭代次数Iter、初始化温度T
    int Iter = 0;
    // int InnerIter = int( pow(glbInstMap.size(),4/3) ); 
    int InnerIter = int( glbInstMap.size()*0.2 ); 
    // int InnerIter = 100;
    float T = 2, threashhold = 1e-3, alpha = 0.8; //0.8-0.99
    // 计算初始cost
    int cost = 0, costNew = 0;
    cost = getWirelength(isBaseline);
    // cost = getHPWL(isBaseline);
    //自适应参数
    int counterNet = 0;
    const int counterNetLimit = 100;
    const int seed = 999;
    set_random_seed(seed);

    std::vector<int> sigmaVecInit;
    //根据标准差设置初始温度
    for(int i = 0; i < 50; i++){
        //计算50次步骤取方差
        int netId = selectNetId(fitnessVec);
        Net* net = glbNetMap[netId];
        Instance* inst = selectInst(net);
        if(inst == nullptr){
            // 没找到可移动的inst，跳过后续部分
            continue;
        }
        int centerX, centerY;
        std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
        int x, y, z;
        std::tie(x, y, z) = findSuitableLoc(isBaseline, centerX, centerY, rangeDesiredMap[netId], inst);
        if(z == -1){
            // 没找到合适位置
            continue;
        }
        //找到这个inst附近的net
        std::set<int> instRelatedNetId;
        //访问inputpin
        for(auto& pin: inst->getInpins()){
            instRelatedNetId.insert(pin->getNetID());
        }
        //访问outputpin
        for(auto& pin: inst->getOutpins()){
            instRelatedNetId.insert(pin->getNetID());
        }
        
        // 计算移动后的newCost
        std::tuple<int, int, int> loc = std::make_tuple(x, y, z);
        std::tuple<int, int, int> originLoc;
        //保存更新前的部分net
        int beforeNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
        if(isBaseline){
            originLoc = inst->getBaseLocation();
            inst->setBaseLocation(loc);
        }
        else{
            originLoc = inst->getLocation();
            inst->setLocation(loc);
        }
        int afterNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
        int costNew = cost - beforeNetWL + afterNetWL;
        if(costNew < cost){
            sigmaVecInit.emplace_back(costNew);
        }
        //复原
        if(isBaseline){
            inst->setBaseLocation(originLoc);
        } else{
            inst->setLocation(originLoc);
        }
    }

    //计算标准差
    double standardDeviation = calculateStandardDeviation(sigmaVecInit);
    std::cout<<"------------------------------------------------\n";
    std::cout << "[INFO] Standard Deviation: " << standardDeviation << std::endl;
    if(standardDeviation != 0){
        T = 0.5 * standardDeviation;
    }
    std::cout<<"[INFO] The simulated annealing algorithm starts "<< std::endl;
    std::cout<<"[INFO] initial temperature T= "<< T <<", threshhold= "<<threashhold<<", alpha= "<<alpha<< ", InnerIter= "<<InnerIter<<", seed="<<seed<<std::endl;

    bool timeup = false;
    // 外层循环 温度大于阈值， 更新一次fitness优先级列表
    while(T > threashhold){
        //记录接受的new_cost
        std::vector<int> sigmaVec; 
        // 内层循环 小于内层迭代次数
        while(Iter < InnerIter){
            if(Iter % 100 == 0) {
                auto tmp = std::chrono::high_resolution_clock::now();
                // 计算运行时间
                std::chrono::duration<double> durationtmp = tmp - start;
                if(durationtmp.count() >= 1180){
                    timeup = true;
                    break;
                }
                std::cout<<"[INFO] T:"<< std::scientific << std::setprecision(3) <<T <<" iter:"<<std::setw(4)<<Iter<<" alpha:"<<std::fixed<<std::setprecision(2)<<alpha<<" cost:"<<std::setw(7)<<cost<<std::endl;
            }
            // std::cout<<"[INFO] T:"<< std::scientific << std::setprecision(3) <<T <<" iter:"<<std::setw(4)<<Iter<<" alpha:"<<std::fixed<<std::setprecision(2)<<alpha<<" cost:"<<std::setw(7)<<cost<<std::endl;
            Iter++;
            // 根据fitness列表选择一个net
            int netId = selectNetId(fitnessVec);
            #ifdef DEBUG
                std::cout<<"DEBUG-netId:"<<netId<<std::endl;
            #endif
            Net* net = glbNetMap[netId];
            // 随机选择net中的一个inst  
            Instance* inst = selectInst(net);
            if(inst == nullptr){
                // 没找到可移动的inst，跳过后续部分
                continue;
            }
            #ifdef DEBUG
                std::cout<<"DEBUG-instName:"<<inst->getInstanceName()<<std::endl;
            #endif
            // 确定net的中心  
            int centerX, centerY;
            std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
            // 在 rangeDesired 范围内选取一个位置去放置这个inst 
            int x, y, z;
            std::tie(x, y, z) = findSuitableLoc(isBaseline, centerX, centerY, rangeDesiredMap[netId], inst);
            if(z == -1){
                // 没找到合适位置
                continue;
            }
            //找到这个inst附近的net
            std::set<int> instRelatedNetId;
            //访问inputpin
            for(auto& pin: inst->getInpins()){
                int netId = pin->getNetID();
                //-1表示未连接
                if(netId != -1) instRelatedNetId.insert(pin->getNetID());
            }
            //访问outputpin
            for(auto& pin: inst->getOutpins()){
                int netId = pin->getNetID();
                //-1表示未连接
                if(netId != -1) instRelatedNetId.insert(pin->getNetID());
            }
            
            // 计算移动后的newCost
            std::tuple<int, int, int> loc = std::make_tuple(x, y, z);
            std::tuple<int, int, int> originLoc;
            //保存更新前的部分net
            int beforeNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
            if(isBaseline){
                originLoc = inst->getBaseLocation();
                inst->setBaseLocation(loc);
            }
            else{
                originLoc = inst->getLocation();
                inst->setLocation(loc);
            }
            int afterNetWL = getRelatedWirelength(isBaseline, instRelatedNetId);
            int costNew = cost - beforeNetWL + afterNetWL;
            // costNew = getHPWL(isBaseline);
            // deta = new_cost - cost
            int deta = costNew - cost;
            #ifdef DEBUG
                std::cout<<"DEBUG-deta:"<<deta<<std::endl;
            #endif
            // if deta < 0 更新这个操作到布局中，更新fitness列表
            if(deta < 0){
                changeTile(isBaseline, originLoc, loc, inst);
                calculRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
                cost = costNew;
                sigmaVec.emplace_back(costNew);
                // sortedFitness(fitnessVec);
            }
            else{
                // else 取(0,1)随机数，判断随机数是否小于 e^(-deta/T) 是则同样更新操作，更新fitness列表
                // 生成一个 0 到 1 之间的随机浮点数
                double randomValue = generate_random_double(0.0, 1.0);
                double eDetaT = exp(-deta/T);
                #ifdef DEBUG
                    std::cout<<"DEBUG-randomValue:"<<randomValue<<", eDetaT:"<<eDetaT<<std::endl;
                #endif
                if(randomValue < eDetaT){
                    changeTile(isBaseline, originLoc, loc, inst);
                    calculRelatedRangeMap(isBaseline, rangeActualMap, instRelatedNetId);
                    calculRelatedFitness(fitnessVec, rangeDesiredMap, rangeActualMap, instRelatedNetId);
                    cost = costNew;
                    sigmaVec.emplace_back(costNew);
                }
                else{ //复原
                    if(isBaseline){
                        inst->setBaseLocation(originLoc);
                    } else{
                        inst->setLocation(originLoc);
                    }
                }
            }
            // counterNet 计数+1
            counterNet += 1;
            // 当计数等于一个限制时，更新rangeActual 到 rangeDesired
            if(counterNet == counterNetLimit){
                rangeDesiredMap = rangeActualMap;
                counterNet = 0;
            }
        }
        if(timeup) break; //时间快到了，结束
        
        double acceptRate = sigmaVec.size() / InnerIter;
        if(0.96 <= acceptRate){
            alpha = 0.5;
        } else if(0.8 <= acceptRate && acceptRate < 0.96){
            alpha = 0.9;
        } else if(0.16 <= acceptRate && acceptRate < 0.8 ){
            alpha = 0.95;
        } else{
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
    return 0;
}

