#include <iomanip>
#include "global.h"
#include "object.h"
#include "sa.h"
#include <algorithm>
#include <cmath>
#include "wirelength.h"
#include <random>

int calculFitness(std::vector<std::pair<int,float>> fitnessVec, std::map<int, int> rangeDesiredMap, std::map<int, int> rangeActualMap){ 
    // 计算fitness
    int n = fitnessVec.size();
    for(int i = 0; i < n; i++){
        int netId; 
        float fitness;
        std::tie(netId, fitness) = fitnessVec[i];
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

int sortedFitness(std::vector<std::pair<int,float>> fitnessVec){ //将fitnessVec按第二个值升序排列
    std::sort(fitnessVec.begin(), fitnessVec.end(), [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
        return a.second < b.second; // 按照 float 值从小到大排序
    });
    return 0;
}

int selectNetId(std::vector<std::pair<int,float>> fitnessVec){ //返回具有一定规律随机选取的net
    int n = fitnessVec.size();
    // 使用当前时间作为种子
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    int a = std::rand() % n; // 范围在 0 到 n - 1
    int b = std::rand() % n;
    return std::min(a, b);
}

Instance* selectInst(Net *net){ //返回在net中随机选取的instance指针
    std::list<Pin*> outputPin = net->getOutputPins();
    int n = outputPin.size() + 1; // outputpin的数量加一个inputpin
    // 使用当前时间作为种子
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    Instance* inst;
    do{
        int index = std::rand() % n;  // 取最后一个的时候为inputpin
        if(index < outputPin.size()){
            // 创建一个迭代器指向 list 的开始
            std::list<Pin*>::iterator it = outputPin.begin();
            // 使用 std::advance 移动迭代器到指定下标
            std::advance(it, index);
            inst = (*it)->getInstanceOwner();
        }
        else{
            inst = net->getInpin()->getInstanceOwner();
        }
    }while(inst->isFixed()); //排除fixed
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

std::pair<int,int> findSuitableLoc(int x, int y, int rangeDesired, std::set<std::pair<int,int>>& visitLoc){
    // 定义随机数生成器
    std::random_device rd;  // 获取随机数种子
    std::mt19937 gen(rd());  // 随机数生成器
    std::uniform_int_distribution<int> disX(x - rangeDesired, x + rangeDesired); // x 坐标范围
    std::uniform_int_distribution<int> disY(y - rangeDesired, y + rangeDesired); // y 坐标范围
    std::pair<int, int> randomCoordinate;
     do {
        // 生成随机坐标
        randomCoordinate.first = disX(gen);
        randomCoordinate.second = disY(gen);
    } while (visitLoc.count(randomCoordinate) > 0); // 检查是否在集合中
    visitLoc.insert(randomCoordinate); //更新记录
    return randomCoordinate;
}

bool isPutin(Instance* inst, int x, int y){ // 判断是否可以将该instance 放入到该位置
    bool isPut = true;
    Tile* tile = chip.getTile(y, x);
    std::string instType = inst->getModelName();
    //先判断这个位置是否是PLB
    if(!tile->matchType(instType)){
        isPut = false;
    }
    else{
        //判断是否有DRAM

        //根据instType(LUT DSP)进行判断是否可插入

    }
    return isPut; 
}

int SA(bool isBaseline){
    // 初始布局
    
    // 构造 fitness 优先级列表 初始化 rangeDesired  
    std::vector<std::pair<int,float>> fitnessVec; // 第一个是netId，第二个是适应度fitness, 适应度越小表明越需要移动。后续会按照fitness升序排列
    std::map<int, int> rangeDesiredMap; // 第一个是netId，第二个是外框矩形的平均跨度，即半周线长的一半
    for(auto it : glbNetMap){
        Net *net = it.second;
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
        rangeDesiredMap[net->getId()] = netDesired;
        //构造fitnessVec
        fitnessVec.emplace_back(std::make_pair(net->getId(), 0));
    }
    std::map<int, int> rangeActualMap(rangeDesiredMap); // 第一个是netId，第二个是当前实际的外框矩形的平均跨度，即半周线长的一半
    
    // 计算 fitness 并 排序
    calculFitness(fitnessVec, rangeDesiredMap, rangeActualMap);
    sortedFitness(fitnessVec);

    // 初始化迭代次数Iter、初始化温度T
    int Iter = 0, InnerIter = glbNetMap.size()*0.8; //移动百分之八十的net
    float T = 90, threashhold = 0, alpha = 0.8;
    // 计算初始cost
    int cost = 0, costNew = 0;
    cost = getWirelength(isBaseline);
    //自适应参数
    int counterNet = 0;
    const int counterNetLimit = 100;

    // 外层循环 温度大于阈值， 更新一次fitness优先级列表
    while(T > threashhold){
        // 内层循环 小于内层迭代次数
        while(Iter < InnerIter){
            // 根据fitness列表选择一个net
            int netId = selectNetId(fitnessVec);
            Net* net = glbNetMap[netId];
            // 随机选择net中的一个inst  // 需要改进！！ 用一个数组作为备选
            Instance* inst = selectInst(net);
            // 确定net的中心  
            int centerX, centerY;
            std::tie(centerX, centerY) = getNetCenter(isBaseline, net);
            // 在 rangeDesired 范围内选取一个位置去放置这个inst  // 需要改进！！！ 1 用一个数组作为备选 2 做一个x轴映射，丢掉非PLB的坐标
            int x, y;
            std::set<std::pair<int,int>> visitLoc; //记录访问过的 location
            while(true){
                std::tie(x, y)= findSuitableLoc(centerX, centerY, rangeDesiredMap[netId], visitLoc);
                // 判断是否可放入，是否合法
                if(isPutin(inst, x, y)){
                    break;
                }
            }
            // 计算移动后的newCost
            std::tuple<int, int, int> loc = std::make_tuple(x, y, 0);
            std::tuple<int, int, int> originLoc;
            if(isBaseline){
                originLoc = inst->getBaseLocation();
                inst->setBaseLocation(loc);
            }
            else{
                originLoc = inst->getLocation();
                inst->setLocation(loc);
            }
            costNew = getWirelength(isBaseline);
            // deta = new_cost - cost
            int deta = costNew - cost;

            // if deta < 0 更新这个操作到布局中，更新fitness列表
            if(deta < 0){
                calculFitness(fitnessVec, rangeDesiredMap, rangeActualMap);
                sortedFitness(fitnessVec);
            }
            else{
                // else 取(0,1)随机数，判断随机数是否小于 e^(-deta/T) 是则同样更新操作，更新fitness列表
                // 创建随机数生成器
                std::random_device rd;  // 用于生成随机种子
                std::mt19937 gen(rd()); // 标准的随机数生成器

                // 创建浮点数分布器，范围为 [0, 1)
                std::uniform_real_distribution<> dis(0.0, 1.0);

                // 生成一个 0 到 1 之间的随机浮点数
                double randomValue = dis(gen);
                if(randomValue < exp(-deta/T)){
                    calculFitness(fitnessVec, rangeDesiredMap, rangeActualMap);
                    
                }
                else{ //复原
                    if(isBaseline){
                        inst->setBaseLocation(originLoc);
                    }
                    else{
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
        // T = alpha * T
        T = alpha * T;
        // Iter = 0
        Iter = 0;
        // 排序fitness列表
        sortedFitness(fitnessVec);
    }
    
    
    return 0;
}
