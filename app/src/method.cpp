#include <iomanip>
// #include "legal.h"
#include "global.h"
#include "object.h"
#include "method.h"
#include <algorithm>

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

int calculateGain(Instance *inst, Tile *newTile) {
    int originalHPWL = inst->getAllRelatedNetHPWL();  // 获取移动前的HPWL

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

void moveInstance(Instance *inst, Tile *newTile) {
    Tile *oldTile = chip.getTile(std::get<0>(inst->getLocation()), std::get<1>(inst->getLocation()));
    if (oldTile != nullptr) {
        oldTile->removeInstance(inst);  // 从旧Tile中移除inst
    }
    int instID = std::stoi(inst->getInstanceName().substr(5));  // 从第5个字符开始截取，转换为整数
    int offset = newTile->findOffset(inst->getModelName(), inst, false); //cjq modify 获取合并引脚数不超过6的最大引脚数的插槽位置
    if(offset == -1){
        std::cout<<"Unable to find any available space to place\n";
        exit(1);
    }
    newTile->addInstance(instID, offset, inst->getModelName(), false);  // 将inst添加到新Tile
    inst->setLocation(std::make_tuple(newTile->getCol(), newTile->getRow(), 0));  // 更新inst的位置
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
                int gain = calculateGain(inst, tile);
                if (gain > bestGain) {
                    bestGain = gain;
                    bestTile = tile;  // 更新最佳Tile
                }
            }
        }

        // 如果找到最佳位置并且增益为正，则移动inst
        if (bestTile != nullptr && bestGain > 0) {
            moveInstance(inst, bestTile);
            std::cout << "Moved instance " << inst->getInstanceName() << " to tile " << bestTile->getLocStr() << std::endl;
        }
    }

    std::cout << "--------FM完成--------" << std::endl;
}