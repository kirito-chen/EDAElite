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
#include "sa.h"
#include "global_placement_sa.h"

#include <chrono>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <script_file>" << std::endl;
        return 1;
    }
    std::ifstream scriptFile(argv[1]);
    if (!scriptFile)
    {
        std::cout << "Failed to open script file: " << argv[1] << std::endl;
        return 1;
    }

    std::cout << "Track-8 Checker V0.6" << std::endl;
    std::cout << std::endl;

    bool result = true;
    std::string command;
    while (std::getline(scriptFile, command))
    {
        if (command.empty() || command[0] == '#')
        {
            continue;
        }
        if (command.find_first_not_of(' ') == std::string::npos)
        {
            continue;
        }

        std::cout << command << std::endl;
        std::istringstream iss(command);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }
        // You can replace the above line with the actual execution logic
        if (tokens[0] == "read_arch")
        {
            if (tokens.size() != 4)
            {
                std::cout << "Invalid format of " << command << std::endl;
                std::cout << "Usage: read_arch <*.lib> <*.scl> <*.clk>" << std::endl;
                result = false;
            }
            else
            {
                std::string libFileName = tokens[1];
                std::string sclFileName = tokens[2];
                std::string clkFileName = tokens[3];
                if (readAndCreateLib(libFileName) == false)
                {
                    std::cout << "Failed to create library" << std::endl;
                    result = false;
                }
                if (chip.readArch(sclFileName, clkFileName) == false)
                {
                    result = false;
                }
                else
                {
                    std::cout << "  Successfully read architecture files." << std::endl;
                }
            }
        }
        else if (tokens[0] == "read_design")
        {
            if (tokens.size() == 4)
            {
                std::string inputNodeFileName = tokens[1];
                std::string inputNetFileName = tokens[2];
                std::string inputTimingFileName = tokens[3];
                if (!readInputNodes(inputNodeFileName))
                {
                    result = false;
                }
                if (!readInputNets(inputNetFileName))
                {
                    result = false;
                }
                if (!readInputTiming(inputTimingFileName))
                {
                    result = false;
                }
                if (result == true)
                {
                    std::cout << "  Successfully read design files." << std::endl;
                }
            }
            else
            {
                std::cout << "Invalid format of " << command << std::endl;
                std::cout << "Usage: read_design <input_node_file> <input_net_file> <timing_file>" << std::endl;
                result = false;
            }
        }
        else if (tokens[0] == "read_output")
        {
            if (tokens.size() != 2)
            {
                std::cout << "Invalid format of " << command << std::endl;
                std::cout << "Usage: read_output <output_node_file>" << std::endl;
                result = false;
            }
            else
            {
                std::string outputNodeFileName = tokens[1];
                if (!readOutputNetlist(outputNodeFileName))
                {
                    result = false;
                }
                if (result == true)
                {
                    std::cout << "  Successfully read output file." << std::endl;
                }
            }
        }
        else if (tokens[0] == "report_arch")
        {
            chip.reportArch();
        }
        else if (tokens[0] == "report_design")
        {
            reportDesignStatistics();
        }
        else if (tokens[0] == "legal_check")
        {
            result = legalCheck();
        }
        else if (tokens[0] == "report_wirelength")
        {
            reportWirelength();
        }
        else if (tokens[0] == "report_pin_density")
        {
            reportPinDensity();
        }
        else if (tokens[0] == "report_clock_region")
        {
            if (tokens.size() != 3)
            {
                std::cout << "Invalid format of " << command << std::endl;
                std::cout << "Usage: print_tile <col> <row>" << std::endl;
                result = false;
            }
            else
            {
                int col = std::stoi(tokens[1]);
                int row = std::stoi(tokens[2]);
                if (col > chip.getNumClockCol() || row > chip.getNumClockRow())
                {
                    std::cout << "Invalid clock region coordinate: " << col << " " << row << std::endl;
                    result = false;
                }
                reportClockRegion(col, row);
            }
        }
        else if (tokens[0] == "report_tile")
        {
            if (tokens.size() != 3)
            {
                std::cout << "Invalid format of " << command << std::endl;
                std::cout << "Usage: print_tile <col> <row>" << std::endl;
                result = false;
            }
            else
            {
                int col = std::stoi(tokens[1]);
                int row = std::stoi(tokens[2]);
                if (col > chip.getNumCol() || row > chip.getNumRow())
                {
                    std::cout << "Invalid tile coordinate: " << col << " " << row << std::endl;
                    result = false;
                }
                Tile *tile = chip.getTile(col, row);
                tile->reportTile();
            }
        }
        else if (tokens[0] == "report_net")
        {
            if (tokens.size() != 2)
            {
                std::cout << "Invalid format of " << command << std::endl;
                std::cout << "Usage: print_net <net_name>" << std::endl;
            }
            else
            {
                std::string netName = tokens[1];
                size_t underscorePos = netName.find('_');
                if (underscorePos != std::string::npos)
                {
                    std::string subStr = netName.substr(underscorePos + 1);
                    // Convert the second substring to an integer
                    int netID = std::stoi(subStr);
                    if (glbNetMap.find(netID) == glbNetMap.end())
                    {
                        std::cout << "Error: Net ID " << netID << " not found" << std::endl;
                    }
                    else
                    {
                        Net *net = glbNetMap[netID];
                        net->reportNet();
                    }
                }
            }
        }
        else if (tokens[0] == "SA")
        {
            // SA(true);
        }
        else if (tokens[0] == "Initial_Placement")
        {
        }
        else if (tokens[0] == "exit")
        {
            break;
        }
        else if (tokens[0] == "FM")
        {
            calculateTileRemain();
            reportWirelength();
            for (size_t i = 0; i < 4; i++)
            {
                std::cout << "第" << i + 1 << "次迭代";

                // 获取开始时间
                auto start = std::chrono::high_resolution_clock::now();

                FM();

                // 获取结束时间
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> duration = end - start;
                std::cout << "第" << i + 1 << "次迭代FM程序运行时间: " << duration.count() << " 秒" << std::endl;

                reportWirelength();
            }
        }
        else if (tokens[0] == "generateResult")
        {
            if (tokens.size() != 2)
            {
                std::cout << "Need generate path " << std::endl;
                result = false;
            }
            std::string outputFilePath = tokens[1];
            generateOutputFile(outputFilePath);
        }
        else
        {
            std::cout << "Invalid command: " << command << std::endl;
            result = false;
        }
        if (!result)
        {
            break;
        }
        std::cout << std::endl;
    }

    matchLUTPairs(glbInstMap);      // 打包代码

    // 打印一些数据
    size_t mapSize = lutGroups.size();
    std::cout << "Map size:             " << mapSize << std::endl;
    std::cout << "plbPlacementMap size: " << plbPlacementMap.size() << std::endl;
    std::cout << "seqPlacementMap size: " << seqPlacementMap.size() << std::endl;

    // initializePLBGroupLocations(plbPlacementMap);
    // initializeSEQGroupLocations(seqPlacementMap);
    // global_placement_sa(false);
    // 检测数据
    // for (auto &inst : glbInstMap)
    // {
    //     if (inst.first == 43907)
    //     {
    //         std::cout << lineBreaker << std::endl;
    //     }
    // }

    for (auto &glbInst : glbInstMap)
    {
        if (glbInst.first == 1416)
        {
            int dummy = 0;
        }
    }

    //-------------------测试---------------
    // testHPWL();
    // reportWirelength();
    // calculateTileRemain();
    // generateOutputFile("wubaixuantest.node.out");
    // for (size_t i = 0; i < 10; i++)
    // {
    //     std::cout << "第" << i << "次迭代";
    //     FM();
    //     reportWirelength();
    // }
    reportWirelength();
    result = legalCheck();
    generateOutputFile("test.txt");

    // init fixed tile
    for (int i = 0; i < chip.getNumCol(); i++)
    {
        for (int j = 0; j < chip.getNumRow(); j++)
        {
            if (i == 31 && j == 133)
            {
                std::cout << " ";
            }
            if (i == 14 && j == 111)
            {
                std::cout << " ";
            }
            if (i == 99 && j == 152)
            {
                std::cout << " ";
            }
            Tile *tile = chip.getTile(i, j);
            int dummy = 0;
        }
    }
    // TEST FOR INSTANCE
    for (auto &inst : glbInstMap)
    {
        if(inst.second->getInstanceName() == "inst_492" || inst.second->getInstanceName() == "inst_499"|| inst.second->getInstanceName() == "inst_500"|| inst.second->getInstanceName() == "inst_511"|| inst.second->getInstanceName() == "inst_512")
        {
            int dummy = 0;
        }
    }
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
    std::cout << "Main program result: " << std::boolalpha << result << std::endl;
    return result ? 0 : 1;
}
