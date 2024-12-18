/*
代码规范化处理——吴白轩
1、只保留read arch与read design，添加node.out输出函数，所有的坐标变化全在baseline上进行修改——2024年10月22日
*/

#include "object.h"
#include "lib.h"
#include "arch.h"
#include "netlist.h"
#include "legal.h"
#include "wirelength.h"
#include "pindensity.h"
#include "global.h"
#include "rsmt.h"
#include "method.h"
#include "test.h"
#include "arbsa.h"

#include "global_placement_sa.h"

#include <chrono>

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        std::cout << "Usage: " << argv[0] << " xx.nodes xx.nets xx.timing  xx_out.nodes" << std::endl;
        return 1;
    }
    std::string nodesFile = argv[1];
    std::string netsFile = argv[2];
    std::string timingFile = argv[3];
    std::string outFile = argv[4];

    // 读框架
    //  打开 JSON 文件
    std::ifstream inputFile("config.json");
    if (!inputFile.is_open())
    {
        std::cerr << "Failed to open config file." << std::endl;
        return 1;
    }
    // 读取 JSON 文件内容
    std::string jsonContent((std::istreambuf_iterator<char>(inputFile)),
                            std::istreambuf_iterator<char>());
    inputFile.close();

    // 提取指定字段的值
    std::string libFile = getValue(jsonContent, "libFile");
    std::string sclFile = getValue(jsonContent, "sclFile");
    std::string clkFile = getValue(jsonContent, "clkFile");

    // //竞赛提交版本
    // std::string libFile = "/home/public/Arch/fpga.lib";
    // std::string sclFile = "/home/public/Arch/fpga.scl";
    // std::string clkFile = "/home/public/Arch/fpga.clk";
    if (readAndCreateLib(libFile) == false)
    {
        std::cout << "Failed to create library" << std::endl;
    }
    if (chip.readArch(sclFile, clkFile) == false)
    {
        std::cout << "Failed to read sclFile and clkFile" << std::endl;
    }
    std::cout << "  Successfully read Arch files." << std::endl;

    // 读取case
    if (!readInputNodes(nodesFile))
    {
        std::cout << "Failed to read nodesFile" << std::endl;
    }
    if (!readInputNets(netsFile))
    {
        std::cout << "Failed to read netsFile" << std::endl;
    }
    if (!readInputTiming(timingFile))
    {
        std::cout << "Failed to read timingFile" << std::endl;
    }
    std::cout << "  Successfully read design files." << std::endl;

    // 设置isPLB数组
    setIsPLB();

    // 基于baseline修改
    bool isBaseline = false;
    bool isSeqPack = false;

    reportDesignStatistics();

    if (isBaseline)
    {
        setPinDensityMapAndTopValues();
        arbsa(isBaseline, nodesFile);
        
        // free memory before exit
        for (auto &lib : glbLibMap)
        {
            delete lib.second;
        }
        for (auto &inst : glbInstMap)
        {
            delete inst.second;
        }
        for (auto &net : glbNetMap)
        {
            delete net.second;
        }
    }
    else
    {
        readOutputNetlist(nodesFile);

        matchLUTPairs(glbInstMap, true, isSeqPack); // 打包代码
        printInstanceInformation();
        // 模拟退火
        newArbsa(isBaseline, isSeqPack);
    }
    // 生成结果
    generateOutputFile(isBaseline, outFile);

    if (false)
    {
        // 设置isPLB数组
        setIsPLB();
        // 设置 glbPinDensityMap 和 topValues
        setPinDensityMapAndTopValues();

        // 模拟退火
        arbsa(isBaseline, nodesFile);

        // 生成结果
        generateOutputFile(isBaseline, outFile);

        // free memory before exit
        for (auto &lib : glbLibMap)
        {
            delete lib.second;
        }
        for (auto &inst : glbInstMap)
        {
            delete inst.second;
        }
        for (auto &net : glbNetMap)
        {
            delete net.second;
        }
    }

    return 0;
}
