#include <iomanip>
// #include "legal.h"
#include "global.h"
#include "object.h"
#include "method.h"

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

void FM()
{
    std::cout << "--------FM--------" << std::endl;
    for (auto &instPair : glbInstMap)
    {
        // instPair 是一个 std::pair<int, Instance*>
        Instance *inst = instPair.second; // 获取实例指针

        // 调用函数计算并更新该实例的HPWL
        bool isBaseline = true; // 可以根据需求选择是否计算 baseline 状态
        inst->calculateAllRelatedNetHPWL(isBaseline);

        // 输出更新后的HPWL值
        std::cout << "Instance ID: " << instPair.first
                  << " has updated HPWL = " << inst->getAllRelatedNetHPWL() << std::endl;
    }
    std::cout << "--------FM--------" << std::endl;
}