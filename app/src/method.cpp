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
            // if (i == 94 && j == 128)
            // {
            //     std::cout << "-" << std::endl;
            //     Tile *tile = chip.getTile(i, j);
            //     tile->getRemainingPLBResources(true);
            //     // 输出剩余的 LUT 和 DFF 资源数量
            //     std::cout << "Remaining LUTs: " << std::endl;
            //     // std::cout << "Remaining DFFs: " << remainingResources.second << std::endl;
            // }
        }
    }
}
