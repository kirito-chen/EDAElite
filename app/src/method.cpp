#include <iomanip>
// #include "legal.h"
#include "global.h"
#include "object.h"
#include "method.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

// 计算PLB余量的函数——吴白轩——2024年10月18日
void calculateTileRemain()
{
    std::cout << "计算PLB类型的Tile中LUT与DFF的余量" << std::endl;
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {
            Tile *tile = chip.getTile(i, j);
            tile->getRemainingPLBResources(true);
        }
    }
}

int calculateGain(Instance *inst, Tile *newTile, bool isBaseline) {
    int originalHPWL = inst->getAllRelatedNetHPWL();  // 获取移动前的HPWL

    // 临时将inst移动到newTile，计算新的HPWL
    auto originalLocation = inst->getBaseLocation();
    inst->setBaseLocation(std::make_tuple(newTile->getCol(), newTile->getRow(), 0));
    inst->updateInstRelatedNet(isBaseline);
    int newHPWL = inst->getAllRelatedNetHPWL();

    // 恢复原始位置
    inst->setBaseLocation(originalLocation);

    // 计算增益：HPWL减少的量
    return originalHPWL - newHPWL;
}

bool moveInstance(Instance *inst, Tile *newTile, bool isBaseline) {
    int instID = std::stoi(inst->getInstanceName().substr(5));  // 从第5个字符开始截取，转换为整数
    int offset = newTile->findOffset(inst->getModelName(), inst, isBaseline); //cjq modify 获取合并引脚数不超过6的最大引脚数的插槽位置
    if(offset == -1){
        // std::cout<<"Unable to find any available space to place\n";
        return false;
    }
    //可以进行移动
    Tile *oldTile = chip.getTile(std::get<0>(inst->getBaseLocation()), std::get<1>(inst->getBaseLocation()));
    if (oldTile != nullptr) {
        oldTile->removeInstance(inst);  // 从旧Tile中移除inst
    }
    newTile->addInstance(instID, offset, inst->getModelName(), isBaseline);  // 将inst添加到新Tile
    inst->setBaseLocation(std::make_tuple(newTile->getCol(), newTile->getRow(), 0));  // 更新inst的位置
    return true;
}

void FM()
{
    std::cout << "--------FM--------" << std::endl;
    bool isBaseline = true; // 可以根据需求选择是否计算 baseline 状态
    
    // 统计每个net线长
    for (auto iter : glbNetMap)
    {
        Net *net = iter.second;
        net->setNetHPWL(isBaseline);  // 计算并设置net的半周线长
    }

    // 统计每个inst的所有相关net的线长之和以及平均线长
    for (auto &instPair : glbInstMap)
    {
        Instance *inst = instPair.second;
        inst->calculateAllRelatedNetHPWL(isBaseline);  // 计算与该inst相关的net的总HPWL
        inst->generateMovableRegion();  // 生成可移动区域
    }

    // 根据每个实例的HPWL平均值进行排序
    std::vector<std::pair<int, int>> instHPWL;  // {instID, HPWLaver}
    for (auto &instPair : glbInstMap)
    {
        int instanceID = instPair.first;
        Instance *inst = instPair.second;
        int HPWLAver = inst->getAllRelatedNetHPWLAver();  // 获取HPWL平均值
        instHPWL.emplace_back(instanceID, HPWLAver);  // 将instID与HPWLAver存入
    }

    // 对实例按HPWL降序排序
    std::sort(instHPWL.begin(), instHPWL.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return a.second > b.second;  // 降序
    });

    // 遍历排序后的实例，尝试移动
    for (const auto &instPair : instHPWL)
    {
        Instance *inst = glbInstMap[instPair.first];  // 获取实例
        if (inst->isFixed()) continue;  // 跳过固定的实例

        Tile *bestTile = nullptr;  // 最佳的Tile位置
        int bestGain = 0;  // 最佳增益

        // 遍历inst的可移动区域
        int x_min = inst->getMovableRegion()[0];
        int y_min = inst->getMovableRegion()[1];
        int x_max = inst->getMovableRegion()[2];
        int y_max = inst->getMovableRegion()[3];

        for (int x = x_min; x <= x_max; ++x) {
            for (int y = y_min; y <= y_max; ++y) {
                Tile *tile = chip.getTile(x, y);  // 获取目标Tile
                if (tile == nullptr || !tile->matchType(inst->getModelName())) {
                    continue;  // 如果Tile不存在或类型不匹配，跳过
                }

                // 检查Tile是否有足够资源容纳inst
                tile->getRemainingPLBResources(false);
                if (!tile->hasEnoughResources(inst)) {
                    continue;  // 如果资源不足，跳过
                }

                // 计算当前移动的增益
                int gain = calculateGain(inst, tile, isBaseline);
                if (gain > bestGain) {
                    bestGain = gain;
                    bestTile = tile;  // 更新最佳Tile
                }
            }
        }

        // 如果找到最佳位置并且增益为正，则移动inst
        if (bestTile != nullptr && bestGain > 0) {
            moveInstance(inst, bestTile, isBaseline);
            // if(moveInstance(inst, bestTile))
            //     std::cout << "Moved instance " << inst->getInstanceName() << " to tile " << bestTile->getLocStr() << std::endl;
        }
    }

    std::cout << "--------FM完成--------" << std::endl;
}


// 统计每种类型的数量
void generateOutputFile(bool isBaseline, const std::string &filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        std::cerr << "fail to open file " << filename << std::endl;
        return;
    }

    // 预定义表头中涉及的类型名称和顺序
    std::vector<std::string> modelOrder = {
        "CARRY4", "F7MUX", "LUT1", "LUT2", "LUT3", "LUT4", "LUT5", "LUT6", 
        "LUT6X", "SEQ", "DRAM", "DSP", "RAMA", "IOA", "IOB", "GCLK"
    };

    // 用于统计每种类型实例的数量
    std::map<std::string, int> typeCountMap;

    // 遍历 glbInstMap 统计每种类型的数量
    for (const auto &instPair : glbInstMap) {
        Instance *inst = instPair.second;
        std::string type = inst->getModelName();
        typeCountMap[type]++;
    }

    // 输出统计表头，按照固定顺序
    outFile << "# List of Models in This Case: \n";
    for (const auto &model : modelOrder) {
        outFile << "#   " << std::setw(25) << std::left << model << ": " << typeCountMap[model] << "\n";
    }
    outFile << "\n";

    // 遍历 glbInstMap 输出实例的位置信息和类型
    for (const auto &instPair : glbInstMap) {
        Instance *inst = instPair.second;
        int instID = instPair.first;

        // 获取实例的基础位置 (x, y, z)
        std::tuple<int,int,int> loc;
        if(isBaseline) loc = inst->getBaseLocation();
        else loc = inst->getLocation();
        int x = std::get<0>(loc);
        int y = std::get<1>(loc);
        int z = std::get<2>(loc);

        // 获取实例的类型
        std::string type = inst->getModelName();

        // 输出实例位置信息
        outFile << "X" << x << "Y" << y << "Z" << z << " " << type << " inst_" << instID;

        // 如果实例是固定的，输出 FIXED
        if (inst->isFixed()) {
            outFile << " FIXED";
        }

        outFile << "\n";
    }

    outFile.close();
    std::cout << "Result file generate in " << filename << std::endl;
}

//获取json文件信息
std::string getValue(const std::string& jsonContent, const std::string& key) {
    // 构造查找键的格式，例如："libFile":
    std::string searchKey = "\"" + key + "\":";

    // 找到键的位置
    size_t startPos = jsonContent.find(searchKey);
    if (startPos == std::string::npos) {
        return "";
    }

    // 跳过键和引号，定位到值的起始位置
    startPos = jsonContent.find("\"", startPos + searchKey.length()) + 1;
    size_t endPos = jsonContent.find("\"", startPos);

    // 返回提取到的值
    return jsonContent.substr(startPos, endPos - startPos);
}

//赋值isPLB数组
int setIsPLB(){
    int numCol = chip.getNumCol(); //150
    int numRow = chip.getNumRow(); //300
    isPLB = new int*[numCol];
    for (int i = 0; i < numCol; i++) {
        isPLB[i] = new int[numRow];
    }
    Tile*** tileArray = chip.getTileArray();
    for(int i = 0; i < numCol; i++){
        for(int j = 0; j < numRow; j++){
            if(tileArray[i][j] != nullptr && tileArray[i][j]->matchType("PLB")){
                isPLB[i][j] = 1;
            }
            else{
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

//如果存在 引脚数 > pinNum的netId 的net则返回true，id存储在 glbBigNet 中
bool findBigNetId(int pinNumLimit){
    bool hasBigNet = false;
    glbBigNetPinNum = 0;
    for(const auto& it : glbNetMap){
        int netId = it.first;
        Net* net = it.second;
        if(net->isClock()){ //跳过clock 不参与线长计算
            continue;
        }
        int pinNum = 1 + (net->getOutputPins()).size(); //+1是唯一的O_x 也就是这里唯一的inpin
        if(pinNum > pinNumLimit){
            glbBigNet.insert(netId);
            glbBigNetPinNum += pinNum;
        }
    }
    if(glbBigNetPinNum > 0) hasBigNet = true;
    return hasBigNet;
}