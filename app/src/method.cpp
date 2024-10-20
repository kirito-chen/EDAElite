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


void FM()
{
    std::cout << "--------FM--------" << std::endl;
    bool isBaseline = true; // 可以根据需求选择是否计算 baseline 状态
    // cjq modify
    // 统计每个net线长
    for (auto iter : glbNetMap)
    {
        Net *net = iter.second;
        // if(net->getId() == 3141){
        //     std::cout<<" ";
        // }
        net->setNetHPWL(isBaseline);
    }
    // 统计inst所有线长以及平均
    for (auto &instPair : glbInstMap)
    {
        // instPair 是一个 std::pair<int, Instance*>
        Instance *inst = instPair.second; // 获取实例指针
        // 调用函数计算并更新该实例的HPWL      
        inst->calculateAllRelatedNetHPWL(isBaseline);
        inst->generateMovableRegion();
        // 输出更新后的HPWL值
        // std::cout << "Instance ID: " << instPair.first
        //           << " has updated HPWL = " << inst->getAllRelatedNetHPWL() 
        //           << " Aver HPWL = " << inst->getAllRelatedNetHPWLAver()<< std::endl;
    }

    // 根据InstHPWLAver排序  
    std::vector<std::pair<int, int>> instHPWL; // {instID, HPWLaver}
    for (auto &instPair : glbInstMap)
    {
        int instanceID = instPair.first;
        Instance *inst = instPair.second;
        int HPWLAver = inst->getAllRelatedNetHPWLAver();
        instHPWL.emplace_back(instanceID, HPWLAver);
    }
    //排序
    std::sort(instHPWL.begin(), instHPWL.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return a.second > b.second;  // 降序排序
    });
    // 输出排序后的结果
    // for (const auto& p : instHPWL) {
    //     std::cout << "(" << p.first << ", " << p.second << ") ";
    // }
    std::cout << std::endl;
    // std::cout<< glbInstMap[0]->getInstanceName() <<std::endl;




    std::cout << "--------FM--------" << std::endl;
}