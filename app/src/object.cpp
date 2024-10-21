#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include "global.h"
#include "object.h"
#include "rsmt.h"
#include "util.h"

Tile::~Tile()
{
  for (auto &pair : instanceMap)
  {
    for (auto &slot : pair.second)
    {
      delete slot;
    }
  }
  instanceMap.clear();
}

bool Tile::matchType(const std::string &modelType)
{
  std::string matchType = modelType;
  if (modelType == "SEQ" ||
      modelType == "LUT6" ||
      modelType == "LUT5" ||
      modelType == "LUT4" ||
      modelType == "LUT3" ||
      modelType == "LUT2" ||
      modelType == "LUT1" ||
      modelType == "LUT6X" ||
      modelType == "DRAM" ||
      modelType == "CARRY4" ||
      modelType == "F7MUX" ||
      modelType == "F8MUX")
  {
    matchType = "PLB";
  }
  return tileTypes.find(matchType) != tileTypes.end();
}

bool Tile::addInstance(int instID, int offset, std::string modelType, const bool isBaseline)
{
  if (matchType(modelType) == false)
  {
    std::cout << "Error: " << getLocStr() << " " << modelType << " instance " << instID << ", type mismatch with tile type" << std::endl;
    return false;
  }

  std::string mtp = unifyModelType(modelType);

  auto mapIter = instanceMap.find(mtp);
  if (mapIter == instanceMap.end())
  {
    std::cout << "Error: Invalid slot type " << mtp << " @ " << getLocStr() << std::endl;
    return false;
  }

  if (offset >= (int)mapIter->second.size())
  {
    std::cout << "Error: " << mtp << " slot offset " << offset << " exceeds the capacity" << std::endl;
    return false;
  }

  if (isBaseline)
  {
    mapIter->second[offset]->addBaselineInstance(instID);
  }
  else
  {
    mapIter->second[offset]->addOptimizedInstance(instID);
  }
  return true;
}

void Tile::clearInstances()
{
  for (auto &pair : instanceMap)
  {
    for (auto &slot : pair.second)
    {
      slot->clearInstances();
    }
  }
}

void Tile::clearBaselineInstances()
{
  for (auto &pair : instanceMap)
  {
    for (auto &slot : pair.second)
    {
      slot->clearBaselineInstances();
    }
  }
}

void Tile::clearOptimizedInstances()
{
  for (auto &pair : instanceMap)
  {
    for (auto &slot : pair.second)
    {
      slot->clearOptimizedInstances();
    }
  }
}

slotArr *Tile::getInstanceByType(std::string type)
{
  auto mapIter = instanceMap.find(type);
  if (mapIter == instanceMap.end())
  {
    return nullptr;
  }
  return &(mapIter->second);
}

void Tile::reportTile()
{
  // report tile occupation
  std::string typeStr;
  for (auto type : tileTypes)
  {
    typeStr += type + " ";
  }
  std::cout << "  Tile " << getLocStr() << " type: " << typeStr << std::endl;

  std::cout << "  " << std::string(55, '-') << std::endl;
  std::cout << "  " << std::left << std::setw(15) << "Slot";
  std::cout << std::setw(32) << "| Occupied(baseline, optimized)";
  std::cout << std::setw(15) << "| Total" << std::endl;
  std::cout << "  " << std::string(55, '-') << std::endl;

  for (auto &pair : instanceMap)
  {
    std::pair<int, int> occupiedSlotCnt = std::make_pair(0, 0);
    for (unsigned int i = 0; i < pair.second.size(); i++)
    {
      if (pair.second[i]->getBaselineInstances().size() > 0)
      {
        occupiedSlotCnt.first++;
      }
      if (pair.second[i]->getOptimizedInstances().size() > 0)
      {
        occupiedSlotCnt.second++;
      }
    }
    std::cout << "  " << std::left << std::setw(15) << pair.first;
    std::string printStr = "( " + std::to_string(occupiedSlotCnt.first) + ", " + std::to_string(occupiedSlotCnt.second) + " )";
    std::cout << "| " << std::setw(30) << printStr;
    std::cout << "| " << std::setw(13) << pair.second.size() << std::endl;
  }

  std::cout << "  " << std::string(55, '-') << std::endl;
  std::cout << std::endl;

  // more detailed information w.r.t occupation
  for (auto &pair : instanceMap)
  {
    for (unsigned int i = 0; i < pair.second.size(); i++)
    {
      if (pair.second[i]->getBaselineInstances().size() == 0 &&
          pair.second[i]->getOptimizedInstances().size() == 0)
      {
        continue;
      }
      std::cout << "  " << pair.first << " #" << i << " (baseline, optimized)" << std::endl;

      std::list<int> baselineInstArr = pair.second[i]->getBaselineInstances();
      std::list<int> optimizedInstArr = pair.second[i]->getOptimizedInstances();

      // print two columns
      // left one is baseline instances
      // right one is optimized instances
      // if two list with different size, print "-" for empty item
      auto baselineIt = baselineInstArr.begin();
      auto optimizedIt = optimizedInstArr.begin();
      while (baselineIt != baselineInstArr.end() || optimizedIt != optimizedInstArr.end())
      {
        std::cout << "    ";

        // Print baseline instance
        if (baselineIt != baselineInstArr.end())
        {
          int instID = *baselineIt;
          if (glbInstMap.find(instID) != glbInstMap.end())
          {
            Instance *instPtr = glbInstMap[instID];
            std::cout << std::left << std::setw(20) << ("inst_" + std::to_string(instID) + " " + instPtr->getModelName());
          }
          else
          {
            std::cout << std::left << std::setw(20) << "Error: Instance not found";
          }
          ++baselineIt;
        }
        else
        {
          std::cout << std::setw(20) << "-";
        }

        std::cout << " | ";

        // Print optimized instance
        if (optimizedIt != optimizedInstArr.end())
        {
          int instID = *optimizedIt;
          if (glbInstMap.find(instID) != glbInstMap.end())
          {
            Instance *instPtr = glbInstMap[instID];
            std::cout << std::left << std::setw(30) << ("inst_" + std::to_string(instID) + " " + instPtr->getModelName());
          }
          else
          {
            std::cout << std::left << std::setw(30) << "Error: Instance not found";
          }
          ++optimizedIt;
        }
        else
        {
          std::cout << std::left << std::setw(20) << "-";
        }

        std::cout << std::endl;
      }
    }
  }
  std::cout << std::endl;

  // report pin utilization
  std::set<int> baselineInpinSet = getConnectedLutSeqInput(true);
  std::set<int> optimizedInpinSet = getConnectedLutSeqInput(false);

  std::cout << "  Detailed pin utilization:" << std::endl;
  std::cout << "    Input nets: Baseline " << baselineInpinSet.size() << "/" << MAX_TILE_PIN_INPUT_COUNT;
  std::cout << ", Optimized " << optimizedInpinSet.size() << "/" << MAX_TILE_PIN_INPUT_COUNT << std::endl;

  // print left baseline netID, right optimized netID, padding with "-" is not exist
  auto baselineNetIt = baselineInpinSet.begin();
  auto optimizedNetIt = optimizedInpinSet.begin();
  while (baselineNetIt != baselineInpinSet.end() || optimizedNetIt != optimizedInpinSet.end())
  {
    std::cout << "      ";
    if (baselineNetIt != baselineInpinSet.end())
    {
      std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*baselineNetIt));
      ++baselineNetIt;
    }
    else
    {
      std::cout << std::left << std::setw(15) << "-";
    }
    std::cout << " | ";
    if (optimizedNetIt != optimizedInpinSet.end())
    {
      std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*optimizedNetIt));
      ++optimizedNetIt;
    }
    else
    {
      std::cout << std::left << std::setw(15) << "-";
    }
    std::cout << std::endl;
  }

  // print output pin utilization
  std::cout << std::endl;
  std::set<int> baselineOutpinSet = getConnectedLutSeqOutput(true);
  std::set<int> optimizedOutpinSet = getConnectedLutSeqOutput(false);

  std::cout << "  Detailed output pin utilization:" << std::endl;
  std::cout << "    Output nets: Baseline " << baselineOutpinSet.size() << "/" << MAX_TILE_PIN_OUTPUT_COUNT;
  std::cout << ", Optimized " << optimizedOutpinSet.size() << "/" << MAX_TILE_PIN_OUTPUT_COUNT << std::endl;

  // print left baseline netID, right optimized netID, padding with "-" is not exist
  baselineNetIt = baselineOutpinSet.begin();
  optimizedNetIt = optimizedOutpinSet.begin();
  while (baselineNetIt != baselineOutpinSet.end() || optimizedNetIt != optimizedOutpinSet.end())
  {
    std::cout << "      ";
    if (baselineNetIt != baselineOutpinSet.end())
    {
      std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*baselineNetIt));
      ++baselineNetIt;
    }
    else
    {
      std::cout << std::left << std::setw(15) << "-";
    }
    std::cout << " | ";
    if (optimizedNetIt != optimizedOutpinSet.end())
    {
      std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*optimizedNetIt));
      ++optimizedNetIt;
    }
    else
    {
      std::cout << std::left << std::setw(15) << "-";
    }
    std::cout << std::endl;
  }

  // report control set
  std::cout << std::endl;
  std::cout << "  Detailed control set:" << std::endl;
  for (int bank = 0; bank < 2; bank++)
  {
    std::set<int> baselineClkNets;
    std::set<int> baselineCeNets;
    std::set<int> baselineSrNets;
    getControlSet(true, bank, baselineClkNets, baselineCeNets, baselineSrNets);

    std::set<int> optimizedClkNets;
    std::set<int> optimizedCeNets;
    std::set<int> optimizedSrNets;
    getControlSet(false, bank, optimizedClkNets, optimizedCeNets, optimizedSrNets);

    std::cout << "    Bank " << bank << std::endl;
    if (baselineClkNets.size() > 0 || optimizedClkNets.size() > 0)
    {
      std::cout << "      Clock nets: baseline = " << baselineClkNets.size() << ", optimized = " << optimizedClkNets.size() << std::endl;
      auto baselineClkNetIt = baselineClkNets.begin();
      auto optimizedClkNetIt = optimizedClkNets.begin();
      while (baselineClkNetIt != baselineClkNets.end() || optimizedClkNetIt != optimizedClkNets.end())
      {
        std::cout << "        ";
        if (baselineClkNetIt != baselineClkNets.end())
        {
          std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*baselineClkNetIt));
          ++baselineClkNetIt;
        }
        else
        {
          std::cout << std::left << std::setw(15) << "-";
        }
        std::cout << " | ";

        if (optimizedClkNetIt != optimizedClkNets.end())
        {
          std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*optimizedClkNetIt));
          ++optimizedClkNetIt;
        }
        else
        {
          std::cout << std::left << std::setw(15) << "-";
        }
        std::cout << std::endl;
      }
    }

    if (baselineSrNets.size() > 0 || optimizedSrNets.size() > 0)
    {
      std::cout << "      Reset nets: baseline = " << baselineSrNets.size() << ", optimized = " << optimizedSrNets.size() << std::endl;
      auto baselineSrNetIt = baselineSrNets.begin();
      auto optimizedSrNetIt = optimizedSrNets.begin();
      while (baselineSrNetIt != baselineSrNets.end() || optimizedSrNetIt != optimizedSrNets.end())
      {
        std::cout << "        ";
        if (baselineSrNetIt != baselineSrNets.end())
        {
          std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*baselineSrNetIt));
          ++baselineSrNetIt;
        }
        else
        {
          std::cout << std::left << std::setw(15) << "-";
        }
        std::cout << " | ";
        if (optimizedSrNetIt != optimizedSrNets.end())
        {
          std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*optimizedSrNetIt));
          ++optimizedSrNetIt;
        }
        else
        {
          std::cout << std::left << std::setw(15) << "-";
        }
        std::cout << std::endl;
      }
    }

    if (baselineCeNets.size() > 0 || optimizedCeNets.size() > 0)
    {
      std::cout << "      CE nets: baseline = " << baselineCeNets.size() << ", optimized = " << optimizedCeNets.size() << std::endl;
      auto baselineCeNetIt = baselineCeNets.begin();
      auto optimizedCeNetIt = optimizedCeNets.begin();
      while (baselineCeNetIt != baselineCeNets.end() || optimizedCeNetIt != optimizedCeNets.end())
      {
        std::cout << "        ";
        if (baselineCeNetIt != baselineCeNets.end())
        {
          std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*baselineCeNetIt));
          ++baselineCeNetIt;
        }
        else
        {
          std::cout << std::left << std::setw(15) << "-";
        }
        std::cout << " | ";
        if (optimizedCeNetIt != optimizedCeNets.end())
        {
          std::cout << std::left << std::setw(15) << ("net_" + std::to_string(*optimizedCeNetIt));
          ++optimizedCeNetIt;
        }
        else
        {
          std::cout << std::left << std::setw(15) << "-";
        }
        std::cout << std::endl;
      }
    }
    std::cout << std::endl;
  }
}

// 判断inst能够放进tile所在的位置
bool Tile::hasEnoughResources(Instance *inst)
{
  std::string modelType = inst->getModelName();

  if (modelType == "LUT")
  {
    int requiredLUTs = 1; // 假设每个实例需要1个LUT
    int availableLUTs = 0;
    for (auto &usage : lutUsage)
    {
      availableLUTs += usage.numInstances; // 统计剩余LUT资源
    }
    return availableLUTs >= requiredLUTs; // 判断是否有足够LUT资源
  }

  // 检查是否为 "DFF" 类型实例
  if (modelType == "SEQ" || modelType == "DFF")
  {
    // 判断当前Tile内剩余的DFF数量
    int availableDFFs = dffUsage;
    // DFF实例的个数表示所需的DFF数量
    if (availableDFFs >= 1)
    {
      return true; // 有足够的DFF资源
    }
    else
    {
      return false; // DFF资源不足
    }
  }

  // 可以扩展其他资源判断
  return false;
}

void Tile::removeInstance(Instance *inst)
{
  std::string modelType = inst->getModelName(); // 获取实例的类型
  auto mapIter = instanceMap.find(modelType);   // 在实例映射中找到对应类型的插槽

  if (mapIter != instanceMap.end())
  {
    slotArr &slots = mapIter->second;

    //     // 遍历 slot，寻找包含这个实例的 Slot 并移除
    //     for (Slot *slot : slots) {
    //         std::list<int> optimizedInstArr = slot->getOptimizedInstances();  // 获取副本
    //         auto it = std::find(optimizedInstArr.begin(), optimizedInstArr.end(), inst->getInstanceName());
    //         if (it != optimizedInstArr.end()) {
    //             optimizedInstArr.erase(it);  // 从副本中移除实例
    //             slot->clearOptimizedInstances();  // 清空优化实例
    //             for (int id : optimizedInstArr) {
    //                 slot->addOptimizedInstance(id);  // 重新插入剩余实例
    //             }
    //             std::cout << "Instance " << inst->getInstanceName() << " removed from Tile " << getLocStr() << std::endl;
    //             return;
    //         }
    //     }
    std::cout << "Instance ";
  }

  std::cout << "Instance " << inst->getInstanceName() << " not found in Tile " << getLocStr() << std::endl;
}

bool Tile::initTile(const std::string &tileType)
{

  if (tileTypes.find(tileType) != tileTypes.end())
  {
    std::cout << "Error: Slot already initialized with same type " << tileType << std::endl;
    return false;
  }
  else
  {
    tileTypes.insert(tileType);
  }

  if (tileType == "PLB")
  {
    std::vector<Slot *> tmpLutSlotArr;
    for (unsigned int i = 0; i < MAX_LUT_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpLutSlotArr.push_back(slot);
    }
    instanceMap["LUT"] = tmpLutSlotArr;

    std::vector<Slot *> tmpDffSlotArr;
    for (unsigned int i = 0; i < MAX_DFF_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpDffSlotArr.push_back(slot);
    }
    instanceMap["SEQ"] = tmpDffSlotArr;

    std::vector<Slot *> tmpCarrySlotArr;
    for (unsigned int i = 0; i < MAX_CARRY4_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpCarrySlotArr.push_back(slot);
    }
    instanceMap["CARRY4"] = tmpCarrySlotArr;

    std::vector<Slot *> tmpF7SlotArr;
    for (unsigned int i = 0; i < MAX_F7_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpF7SlotArr.push_back(slot);
    }
    instanceMap["F7MUX"] = tmpF7SlotArr;

    std::vector<Slot *> tmpF8SlotArr;
    for (unsigned int i = 0; i < MAX_F8_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpF8SlotArr.push_back(slot);
    }
    instanceMap["F8MUX"] = tmpF8SlotArr;

    std::vector<Slot *> tmpDramSlotArr;
    for (unsigned int i = 0; i < MAX_DRAM_CAPACITY; i++)
    {
      Slot *slota = new Slot();
      tmpDramSlotArr.push_back(slota);
    }
    instanceMap["DRAM"] = tmpDramSlotArr;
  }
  else if (tileType == "DSP")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_DSP_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["DSP"] = tmpSlotArr;
  }
  else if (tileType == "RAMA")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_RAM_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["RAMA"] = tmpSlotArr;
  }
  else if (tileType == "RAMB")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_RAM_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["RAMB"] = tmpSlotArr;
  }
  else if (tileType == "IOA")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_IO_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["IOA"] = tmpSlotArr;
  }
  else if (tileType == "IOB")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_IO_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["IOB"] = tmpSlotArr;
  }
  else if (tileType == "GCLK")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_GCLK_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["GCLK"] = tmpSlotArr;
  }
  else if (tileType == "IPPIN")
  {
    std::vector<Slot *> tmpSlotArr;
    for (unsigned int i = 0; i < MAX_IPPIN_CAPACITY; i++)
    {
      Slot *slot = new Slot();
      tmpSlotArr.push_back(slot);
    }
    instanceMap["IPPIN"] = tmpSlotArr;
  }
  else if (tileType == "FIXED")
  {
  }
  else
  {
    std::cout << "Error: Invalid slot type " << tileType << std::endl;
  }

  return true;
}

bool Tile::isEmpty(bool isBaseline)
{
  for (auto &pair : instanceMap)
  {
    for (auto &slot : pair.second)
    {
      // skip ippin
      if (pair.first == "IPPIN")
      {
        continue;
      }
      if (isBaseline)
      {
        if (!slot->getBaselineInstances().empty())
        {
          return false;
        }
      }
      else
      {
        if (!slot->getOptimizedInstances().empty())
        {
          return false;
        }
      }
    }
  }
  return true;
}

std::set<int> Tile::getConnectedLutSeqInput(bool isBaseline)
{
  std::set<int> netSet; // using set to merge identical nets
  if (matchType("PLB") == false)
  {
    return netSet;
  }

  for (auto mapIter : instanceMap)
  {
    std::string slotType = mapIter.first;

    if (slotType != "LUT" && slotType != "SEQ")
    {
      continue;
    }

    for (auto slot : mapIter.second)
    {
      std::list<int> instArr;
      if (isBaseline)
      {
        instArr = slot->getBaselineInstances();
      }
      else
      {
        instArr = slot->getOptimizedInstances();
      }
      for (auto instID : instArr)
      {
        if (glbInstMap.find(instID) == glbInstMap.end())
        {
          std::cout << "Error: Instance ID " << instID << " not found in the global instance map" << std::endl;
          continue;
        }
        Instance *instPtr = glbInstMap[instID];

        int numInpins = instPtr->getNumInpins();
        for (int i = 0; i < numInpins; i++)
        {
          Pin *pin = instPtr->getInpin(i);
          // if (pin->getProp() != PIN_PROP_NONE) {
          //   // skip DFF ctrl, clk, reset pins
          //   continue;
          // }
          int netID = pin->getNetID();
          if (netID < 0)
          { // unconnected pin
            continue;
          }

          if (glbNetMap.find(netID) == glbNetMap.end())
          {
            std::cout << "Error: Net ID " << netID << " not found in the global net map" << std::endl;
            continue;
          }
          Net *netPtr = glbNetMap[netID];

          // check if driver is in the same tile
          // only count pins driven by nets from other tile
          if (netPtr->getInpin() != nullptr)
          {
            Instance *driverInstPtr = netPtr->getInpin()->getInstanceOwner();
            std::tuple<int, int, int> driverLoc;
            if (isBaseline)
            {
              driverLoc = driverInstPtr->getBaseLocation();
            }
            else
            {
              driverLoc = driverInstPtr->getLocation();
            }
            if (std::get<0>(driverLoc) == col && std::get<1>(driverLoc) == row)
            {
              continue;
            }
          }
          netSet.insert(netID);
        }
      }
    }
  }
  return netSet;
}

std::set<int> Tile::getConnectedLutSeqOutput(bool isBaseline)
{
  std::set<int> netSet;
  if (matchType("PLB") == false)
  {
    return netSet;
  }

  for (auto mapIter : instanceMap)
  {
    std::string slotType = mapIter.first;
    if (slotType != "LUT" && slotType != "SEQ")
    {
      continue;
    }

    for (auto slot : mapIter.second)
    {
      std::list<int> instArr;
      if (isBaseline)
      {
        instArr = slot->getBaselineInstances();
      }
      else
      {
        instArr = slot->getOptimizedInstances();
      }
      for (auto instID : instArr)
      {
        if (glbInstMap.find(instID) == glbInstMap.end())
        {
          std::cout << "Error: Instance ID " << instID << " not found in the global instance map" << std::endl;
          continue;
        }
        Instance *instPtr = glbInstMap[instID];

        int numOutpins = instPtr->getNumOutpins();
        for (int i = 0; i < numOutpins; i++)
        {
          Pin *pin = instPtr->getOutpin(i);
          int netID = pin->getNetID();
          if (netID < 0)
          { // unconnected pin
            continue;
          }

          if (glbNetMap.find(netID) == glbNetMap.end())
          {
            std::cout << "Error: Net ID " << netID << " not found in the global net map" << std::endl;
            continue;
          }
          Net *netPtr = glbNetMap[netID];
          if (netPtr->isIntraTileNet(isBaseline))
          {
            continue;
          }
          netSet.insert(netID);
        }
      }
    }
  }
  return netSet;
}

bool Tile::getControlSet(
    const bool isBaseline,
    const int bank,
    std::set<int> &clkNets,
    std::set<int> &ceNets,
    std::set<int> &srNets)
{

  for (auto mapIter : instanceMap)
  {
    std::string slotType = mapIter.first;
    // in PLB, only SEQ has control pins
    if (slotType != "SEQ")
    {
      continue;
    }

    // DFF bank0: 0-7, bank1: 8-15
    int startIdx = 0;
    int endIdx = 15;
    if (bank == 0)
    {
      startIdx = 0;
      endIdx = 7;
    }
    else if (bank == 1)
    {
      startIdx = 8;
      endIdx = 15;
    }
    else
    {
      std::cout << "Error: Invalid bank ID " << bank << std::endl;
      return false;
    }

    for (int slotIdx = startIdx; slotIdx <= endIdx; slotIdx++)
    {
      Slot *slotPtr = mapIter.second[slotIdx];
      std::list<int> instArr;
      if (isBaseline)
      {
        instArr = slotPtr->getBaselineInstances();
      }
      else
      {
        instArr = slotPtr->getOptimizedInstances();
      }
      for (auto instID : instArr)
      {
        if (glbInstMap.find(instID) == glbInstMap.end())
        {
          std::cout << "Error: Instance ID " << instID << " not found in the global instance map" << std::endl;
          return false;
        }
        Instance *instPtr = glbInstMap[instID];

        int numInpins = instPtr->getNumInpins();
        for (int i = 0; i < numInpins; i++)
        {
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
        int numOutpins = instPtr->getNumOutpins();
        for (int i = 0; i < numOutpins; i++)
        {
          Pin *pin = instPtr->getOutpin(i);
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
    }
  }
  return true;
}

// 关于PLB内部资源余量判断函数_WBX--2024年10月18日
void Tile::getRemainingPLBResources(bool isBaseline)
{
  int usedLUTs = 0;
  int usedDFFs = 0;

  // 重置lutUsage，初始化每个LUT site的剩余实例数和剩余引脚数
  for (auto &usage : lutUsage)
  {
    usage.numInstances = 2;
    // usage.remainingPins = 6; // 假设每个LUT有6个引脚可用，具体根据你的架构
  }

  // 遍历Tile中的实例映射，找出LUT和DFF的使用情况
  for (auto mapIter = instanceMap.begin(); mapIter != instanceMap.end(); ++mapIter)
  {
    std::string modelType = mapIter->first; // 获取实例类型，如"LUT", "DFF"
    slotArr slots = mapIter->second;        // 获取对应的插槽

    for (int idx = 0; idx < (int)slots.size(); ++idx)
    {
      Slot *slot = slots[idx];
      if (!slot)
        continue;

      std::list<int> instances;
      if (isBaseline)
      {
        instances = slot->getBaselineInstances();
      }
      else
      {
        instances = slot->getOptimizedInstances();
      }

      // 统计LUT的使用情况和实际引脚数
      if (modelType == "LUT")
      {
        int numInstances = instances.size();
        if (numInstances > 0)
        {
          lutUsage[idx].numInstances -= numInstances; // 记录该LUT site中的实例数量
          usedLUTs += numInstances;                   // 累计已使用的LUT数量

          // 遍历每个实例，计算实际使用的引脚数
          std::set<int> totalUsedPins;
          for (int instID : instances)
          {
            Instance *inst = glbInstMap.find(instID)->second; // 获取实例对象
            std::set<int> temp = getUsedPins(inst);           // 计算该实例实际使用的引脚数
            totalUsedPins.insert(temp.begin(), temp.end());
          }

          // 更新每个LUT site剩余的引脚数
          lutUsage[idx].remainingPins = totalUsedPins; // 假设每个LUT有6个引脚
        }
      }
      else if (modelType == "SEQ")
      {
        usedDFFs += instances.size(); // 统计DFF的使用情况
      }
    }
  }

  // 计算剩余的LUT和DFF资源
  int remainingLUTs = MAX_LUT_CAPACITY - usedLUTs;
  int remainingDFFs = MAX_DFF_CAPACITY - usedDFFs;

  // // 确保剩余资源不为负数
  // if (remainingLUTs < 0)
  //   remainingLUTs = 0;
  // if (remainingDFFs < 0)
  //   remainingDFFs = 0;
  dffUsage = remainingDFFs;
}

// cjq modify 返回tile的instTypes类型的可插入的offset // LUT  SEQ  返回-1表示找不到
int Tile::findOffset(std::string instTypes, Instance *inst, bool isBaseline)
{
  int offset = -1;
  if(instTypes == "LUT"){
    std::vector<int> pinNum(8,0);   // 记录对应lut位置能放下的引脚 -1为不可放置
    //获取int相关的net
    std::set<Net *> netSet = inst->getRelatedNets();
    std::set<int> netIdSet; //net ID 的set
    for(auto net : netSet){
      netIdSet.insert(net->getId());
    }

    slotArr slotArrTmp = instanceMap[instTypes];
    for(int i = 0; i < slotArrTmp.size(); i++){
      Slot *slot = slotArrTmp[i];
      std::list<int> listTmp;
      if(isBaseline)  listTmp = slot->getBaselineInstances();
      else listTmp = slot->getOptimizedInstances();
      //当已经插入的inst大于等于2则不考虑了
      if(listTmp.size() >= 2){
        pinNum[i] = -1;
      }
      else if(listTmp.size() == 1){
        int instIdTmp = listTmp.front();
        Instance *instTmp = glbInstMap[instIdTmp];
        std::set<Net *> netSetTmp = instTmp->getRelatedNets();
        std::set<int> netIdSetTmp; 
        for(auto net : netSetTmp){
          netIdSetTmp.insert(net->getId());
        }
        // 假设插入当前inst
        for(int i : netIdSet){
          netIdSetTmp.insert(i); //重复的会被过滤掉
        }
        pinNum[i] = netIdSetTmp.size();
      }
    }
    // 找到不大于6的最大的引脚的下标
    int maxVal = -1; // 用来存储不大于6的最大数
    int index = -1;  // 用来存储该数的下标
    for (int i = 0; i < pinNum.size(); ++i) {
        if (pinNum[i] <= 6 && pinNum[i] > maxVal) {
            maxVal = pinNum[i];
            index = i;
        }
    }
    offset = index;
  }
  else if(instTypes == "SEQ"){
    // SEQ只要返回一个空位子即可
    slotArr slotArrTmp = instanceMap[instTypes];
    for(int i = 0; i < slotArrTmp.size(); i++){
      Slot *slot = slotArrTmp[i];
      std::list<int> listTmp;
      if(isBaseline)  listTmp = slot->getBaselineInstances();
      else listTmp = slot->getOptimizedInstances();
      if(listTmp.size() == 0){
        offset = i;
      }
    }
  }
  return offset;
}

// 获取当前inst已使用的pin
std::set<int> Tile::getUsedPins(Instance *inst)
{
  std::set<int> usedPins;

  // 统计输入引脚的使用情况
  for (int i = 0; i < inst->getNumInpins(); ++i)
  {
    Pin *inPin = inst->getInpin(i);
    if (inPin->getNetID() != -1)
    { // 检查引脚是否连接到网络
      usedPins.insert(inPin->getNetID());
    }
  }

  // // 统计输出引脚的使用情况
  // for (int i = 0; i < inst->getNumOutpins(); ++i)
  // {
  //   Pin *outPin = inst->getOutpin(i);
  //   if (outPin->getNetID() != -1)
  //   { // 检查引脚是否连接到网络
  //     ++usedPins;
  //   }
  // }

  return usedPins; // 返回该实例实际使用的引脚数
}

void ClockRegion::reportClockRegion()
{
  std::cout << "  Clock region " << getLocStr() << " has " << clockNets.size() << " clock nets." << std::endl;
  for (auto netID : clockNets)
  {
    std::cout << "    net_" << netID << std::endl;
  }
}

Instance::~Instance()
{
  for (auto &pin : inpins)
  {
    delete pin;
  }
  inpins.clear();
  for (auto &pin : outpins)
  {
    delete pin;
  }
  outpins.clear();
}

Instance::Instance()
{
  cellLib = nullptr;
  fixed = false;
  setLocation(std::make_tuple(-1, -1, -1));
  setBaseLocation(std::make_tuple(-1, -1, -1));
  movableRegion.assign(4, -1);
}

bool Instance::isPlaced()
{
  if (std::get<0>(location) == -1 || std::get<1>(location) == -1 || std::get<2>(location) == -1)
  {
    return false;
  }
  else
  {
    return true;
  }
}

bool Instance::isMoved()
{
  if (std::get<0>(location) != std::get<0>(baseLocation) ||
      std::get<1>(location) != std::get<1>(baseLocation) ||
      std::get<2>(location) != std::get<2>(baseLocation))
  {
    return true;
  }
  else
  {
    return false;
  }
}

void Instance::setCellLib(Lib *libPtr)
{
  cellLib = libPtr;

  // create pins
  createInpins();
  createOutpins();
}

void Instance::createInpins()
{
  if (cellLib == nullptr)
  {
    return;
  }
  for (int i = 0; i < cellLib->getNumInputs(); i++)
  {
    Pin *pin = new Pin();
    pin->setNetID(-1);
    pin->setInstanceOwner(this);
    pin->setProp(cellLib->getInputProp(i));
    inpins.push_back(pin);
  }
}

void Instance::createOutpins()
{
  if (cellLib == nullptr)
  {
    return;
  }
  for (int i = 0; i < cellLib->getNumOutputs(); i++)
  {
    Pin *pin = new Pin();
    pin->setNetID(-1);
    pin->setInstanceOwner(this);
    pin->setProp(cellLib->getOutputProp(i));
    outpins.push_back(pin);
  }
}

// 获取与实例相关的所有网络_wbx_2024年10月19日
std::set<Net *> Instance::getRelatedNets() const
{
  std::set<Net *> relatedNets;

  // 获取与输入引脚相关的网络
  for (int i = 0; i < getNumInpins(); ++i)
  {
    Pin *inPin = getInpin(i);
    int netID = inPin->getNetID();
    if (netID != -1 && glbNetMap.find(netID) != glbNetMap.end())
    {
      relatedNets.insert(glbNetMap[netID]);
    }
  }

  // 获取与输出引脚相关的网络
  for (int i = 0; i < getNumOutpins(); ++i)
  {
    Pin *outPin = getOutpin(i);
    int netID = outPin->getNetID();
    if (netID != -1 && glbNetMap.find(netID) != glbNetMap.end())
    {
      relatedNets.insert(glbNetMap[netID]);
    }
  }

  return relatedNets; // 返回与实例相关的所有网络
}

// 计算与实例相关的所有网络的HPWL之和，并存储到allRelatedNetHPWL
// 得添加
void Instance::calculateAllRelatedNetHPWL(bool isBaseline)
{
  allRelatedNetHPWL = 0; // 初始化为0

  std::set<Net *> relatedNets = getRelatedNets();

  // 遍历每个相关的net，累加其HPWL
  for (Net *net : relatedNets)
  {
    // net->setNetHPWL(false);                  // 更新一下net的线长
    allRelatedNetHPWL += net->getCritHPWL(); // 累加net的CritHPWL
    allRelatedNetHPWL += net->getHPWL();     // 累加net的HPWL
  }
  // cjq modify 24.10.20 平均
  if (relatedNets.size() > 0)
    allRelatedNetHPWLAver = allRelatedNetHPWL / relatedNets.size();
  else
    allRelatedNetHPWLAver = 0;
}

// 更新instance相关的所有net的HPWL
void Instance::updateInstRelatedNet(bool isBaseline)
{
  allRelatedNetHPWL = 0; // 初始化为0

  std::set<Net *> relatedNets = getRelatedNets();

  // 遍历每个相关的net，累加其HPWL
  for (Net *net : relatedNets)
  {
    net->setNetHPWL(false);                  // 更新一下net的线长
    allRelatedNetHPWL += net->getCritHPWL(); // 累加net的CritHPWL
    allRelatedNetHPWL += net->getHPWL();     // 累加net的HPWL
  }
  // cjq modify 24.10.20 平均
  if (relatedNets.size() > 0)
    allRelatedNetHPWLAver = allRelatedNetHPWL / relatedNets.size();
  else
    allRelatedNetHPWLAver = 0;
}

// 计算instance的可移动区域
void Instance::generateMovableRegion()
{
  if (!fixed)
  {
    // if(instanceName == "inst_335"){
    //   std::cout<<" ";
    // }
    // std::cout << "生成可移动区域" << std::endl;
    // 初始化可移动区域为一个极大矩形
    int xlb = std::numeric_limits<int>::min();
    int ylb = std::numeric_limits<int>::min();
    int xrt = std::numeric_limits<int>::max();
    int yrt = std::numeric_limits<int>::max();

    // 获取与当前实例相关的所有 Net
    std::set<Net *> relatedNets = getRelatedNets();

    // 遍历所有相关 net，更新可移动区域
    for (Net *net : relatedNets)
    {
      std::vector<int> netArea = net->getBoundingBox();
      xlb = std::max(xlb, netArea[0]); // net 的左下角 x 坐标
      ylb = std::max(ylb, netArea[1]); // net 的左下角 y 坐标
      xrt = std::min(xrt, netArea[2]); // net 的右上角 x 坐标
      yrt = std::min(yrt, netArea[3]); // net 的右上角 y 坐标
    }
    // 设置该实例的可移动区域
    movableRegion = {xlb, ylb, xrt, yrt};
  }
}

bool Net::isIntraTileNet(bool isBaseline)
{
  if (inpin == nullptr)
  {
    return false;
  }
  Instance *driverInstPtr = inpin->getInstanceOwner();
  std::tuple<int, int, int> driverLoc;
  if (isBaseline)
  {
    driverLoc = driverInstPtr->getBaseLocation();
  }
  else
  {
    driverLoc = driverInstPtr->getLocation();
  }

  for (const auto *outpin : getOutputPins())
  {
    Instance *sinkInstPtr = outpin->getInstanceOwner();
    std::tuple<int, int, int> sinkLoc;
    if (isBaseline)
    {
      sinkLoc = sinkInstPtr->getBaseLocation();
    }
    else
    {
      sinkLoc = sinkInstPtr->getLocation();
    }
    if (std::get<0>(driverLoc) != std::get<0>(sinkLoc) ||
        std::get<1>(driverLoc) != std::get<1>(sinkLoc))
    {
      return false;
    }
  }
  return true;
}

int Net::getNumPins()
{
  if (inpin != nullptr)
  {
    return outputPins.size() + 1;
  }
  else
  {
    return outputPins.size();
  }
}

// read net connections from netlist
bool Net::addConnection(std::string conn)
{
  // inst_2 I1
  std::istringstream iss(conn);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token)
  {
    tokens.push_back(token);
  }

  if (tokens.size() != 2)
  {
    std::cout << "Error: Invalid connection format " << conn << std::endl;
    return false;
  }

  int errCnt = 0;
  Instance *instPtr = nullptr;
  Pin *pinPtr = nullptr;

  std::string instName = tokens[0];
  size_t underscorePos = instName.find('_');
  if (underscorePos != std::string::npos)
  {
    std::string subStr = instName.substr(underscorePos + 1);
    // Convert the second substring to an integer
    int instID = std::stoi(subStr);
    if (glbInstMap.find(instID) != glbInstMap.end())
    {
      instPtr = glbInstMap[instID];
    }
  }

  if (instPtr == nullptr)
  {
    std::cout << "Invalid instance name format: " << instName << std::endl;
    errCnt++;
  }
  else
  {
    std::string pinName = tokens[1];
    underscorePos = pinName.find('_');
    if (underscorePos != std::string::npos)
    {
      std::string dirStr = pinName.substr(0, underscorePos);
      std::string pinIDStr = pinName.substr(underscorePos + 1);
      int pinID = std::stoi(pinIDStr);
      if (dirStr == "I")
      {
        pinPtr = instPtr->getInpin(pinID);
        pinPtr->setNetID(getId());
        addOutputPin(pinPtr);
      }
      else if (dirStr == "O")
      {
        pinPtr = instPtr->getOutpin(pinID);
        pinPtr->setNetID(getId());
        if (getInpin() != nullptr)
        {
          std::cout << "Error: Multiple drivers for net ID = " << getId() << std::endl;
          errCnt++;
        }
        setInpin(pinPtr);
      }
      else
      {
        std::cout << "Invalid pin name format: " << pinName << std::endl;
        errCnt++;
      }
    }
  }

  if (errCnt > 0)
  {
    return false;
  }
  else
  {
    return true;
  }
}

int Net::getCritWireLength(bool isBaseline)
{
  int wirelength = 0;
  const Pin *driverPin = getInpin();
  if (!driverPin)
  {
    return 0; // Return 0 if there's no driver pin
  }

  std::tuple<int, int, int> driverLoc;
  if (isBaseline)
  {
    driverLoc = driverPin->getInstanceOwner()->getBaseLocation();
  }
  else
  {
    driverLoc = driverPin->getInstanceOwner()->getLocation();
  }

  std::set<std::pair<int, int>> mergedPinLocs; // merge identical pin locations  //保存x与y的坐标
  for (const auto *outpin : getOutputPins())
  {
    if (!outpin->getTimingCritical())
    {
      continue;
    }
    std::tuple<int, int, int> sinkLoc;
    if (isBaseline)
    {
      sinkLoc = outpin->getInstanceOwner()->getBaseLocation();
    }
    else
    {
      sinkLoc = outpin->getInstanceOwner()->getLocation();
    }
    mergedPinLocs.insert(std::make_pair(std::get<0>(sinkLoc), std::get<1>(sinkLoc)));
  }

  for (auto loc : mergedPinLocs)
  {
    wirelength += std::abs(std::get<0>(loc) - std::get<0>(driverLoc)) +
                  std::abs(std::get<1>(loc) - std::get<1>(driverLoc));
  }

  return wirelength;
}

int Net::setNetHPWL(bool isBaseline) // cjq modify, netArea最小外包矩形也在此处计算，包含所有的inst
{
  int wirelength = 0;
  // 计算关键路径半周线长
  const Pin *driverPin = getInpin();
  if (!driverPin)
  {
    return 0; // Return 0 if there's no driver pin
  }

  std::tuple<int, int, int> driverLoc;
  if (isBaseline)
  {
    driverLoc = driverPin->getInstanceOwner()->getBaseLocation();
  }
  else
  {
    driverLoc = driverPin->getInstanceOwner()->getLocation();
  }

  std::set<std::pair<int, int>> mergedPinLocs; // merge identical pin locations  //保存x与y的坐标
  for (const auto *outpin : getOutputPins())
  {
    if (!outpin->getTimingCritical())
    {
      continue;
    }
    std::tuple<int, int, int> sinkLoc;
    if (isBaseline)
    {
      sinkLoc = outpin->getInstanceOwner()->getBaseLocation();
    }
    else
    {
      sinkLoc = outpin->getInstanceOwner()->getLocation();
    }
    mergedPinLocs.insert(std::make_pair(std::get<0>(sinkLoc), std::get<1>(sinkLoc)));
  }
  if (!mergedPinLocs.empty())
  {
    // 初始化
    int xMax = std::get<0>(driverLoc), xMin = std::get<0>(driverLoc);
    int yMax = std::get<1>(driverLoc), yMin = std::get<1>(driverLoc);
    for (auto loc : mergedPinLocs)
    {
      int xLoc = std::get<0>(loc);
      int yLoc = std::get<1>(loc);
      xMax = std::max(xLoc, xMax);
      xMin = std::min(xLoc, xMin);
      yMax = std::max(yLoc, yMax);
      yMin = std::min(yLoc, yMin);
    }
    wirelength += xMax - xMin + yMax - yMin;
  }
  critHPWL = wirelength;

  // 计算不是关键路径上的半周线长
  std::vector<int> xCoords;
  std::vector<int> yCoords;
  getMergedNonCritPinLocs(isBaseline, xCoords, yCoords);

  if (xCoords.size() > 0)
  {
    // 初始化
    int xMax, xMin, yMax, yMin;
    xMax = xMin = xCoords[0];
    yMax = yMin = yCoords[0];
    for (int i = 0; i < xCoords.size(); i++)
    {
      xMax = std::max(xMax, xCoords[i]);
      xMin = std::min(xMin, xCoords[i]);
      yMax = std::max(yMax, yCoords[i]);
      yMin = std::min(yMin, yCoords[i]);
    }

    wirelength += xMax - xMin + yMax - yMin;
    if (!mergedPinLocs.empty())
    {
      for (auto loc : mergedPinLocs)
      {
        int xLoc = std::get<0>(loc);
        int yLoc = std::get<1>(loc);
        xMax = std::max(xMax, xLoc);
        xMin = std::min(xMin, xLoc);
        yMax = std::max(yMax, yLoc);
        yMin = std::min(yMin, yLoc);
      }
    }
    netArea[0] = xMin;
    netArea[1] = yMin;
    netArea[2] = xMax;
    netArea[3] = yMax;
  }
  HPWL = wirelength;
}

int Net::getCritHPWL(bool isBaseline) // cjq modify
{
  int wirelength = 0;
  const Pin *driverPin = getInpin();
  if (!driverPin)
  {
    return 0; // Return 0 if there's no driver pin
  }

  std::tuple<int, int, int> driverLoc;
  if (isBaseline)
  {
    driverLoc = driverPin->getInstanceOwner()->getBaseLocation();
  }
  else
  {
    driverLoc = driverPin->getInstanceOwner()->getLocation();
  }

  std::set<std::pair<int, int>> mergedPinLocs; // merge identical pin locations  //保存x与y的坐标
  for (const auto *outpin : getOutputPins())
  {
    if (!outpin->getTimingCritical())
    {
      continue;
    }
    std::tuple<int, int, int> sinkLoc;
    if (isBaseline)
    {
      sinkLoc = outpin->getInstanceOwner()->getBaseLocation();
    }
    else
    {
      sinkLoc = outpin->getInstanceOwner()->getLocation();
    }
    mergedPinLocs.insert(std::make_pair(std::get<0>(sinkLoc), std::get<1>(sinkLoc)));
  }
  if (!mergedPinLocs.empty())
  {
    // 初始化
    int xMax = std::get<0>(driverLoc), xMin = std::get<0>(driverLoc);
    int yMax = std::get<1>(driverLoc), yMin = std::get<1>(driverLoc);
    for (auto loc : mergedPinLocs)
    {
      int xLoc = std::get<0>(loc);
      int yLoc = std::get<1>(loc);
      xMax = std::max(xLoc, xMax);
      xMin = std::min(xLoc, xMin);
      yMax = std::max(yLoc, yMax);
      yMin = std::min(yLoc, yMin);
    }
    wirelength += xMax - xMin + yMax - yMin;
  }

  return wirelength;
}

void Net::getMergedNonCritPinLocs(bool isBaseline, std::vector<int> &xCoords, std::vector<int> &yCoords)
{
  const Pin *driverPin = getInpin();
  if (!driverPin)
  {
    return;
  }
  std::set<std::pair<int, int>> rsmtPinLocs;

  std::tuple<int, int, int> driverLoc;
  if (isBaseline)
  {
    driverLoc = driverPin->getInstanceOwner()->getBaseLocation();
  }
  else
  {
    driverLoc = driverPin->getInstanceOwner()->getLocation();
  }
  rsmtPinLocs.insert(std::make_pair(std::get<0>(driverLoc), std::get<1>(driverLoc)));

  for (const auto *outpin : getOutputPins())
  {
    if (outpin->getTimingCritical())
    {
      continue;
    }
    std::tuple<int, int, int> sinkLoc;
    if (isBaseline)
    {
      sinkLoc = outpin->getInstanceOwner()->getBaseLocation();
    }
    else
    {
      sinkLoc = outpin->getInstanceOwner()->getLocation();
    }
    rsmtPinLocs.insert(std::make_pair(std::get<0>(sinkLoc), std::get<1>(sinkLoc)));
  }
  for (auto loc : rsmtPinLocs)
  {
    xCoords.push_back(loc.first);
    yCoords.push_back(loc.second);
  }
}

// flute算法计算非关键路径长度，
int Net::getNonCritWireLength(bool isBaseline)
{
  const Pin *driverPin = getInpin();
  if (!driverPin)
  {
    return 0; // Return 0 if there's no driver pin
  }

  std::vector<int> xCoords;
  std::vector<int> yCoords;
  getMergedNonCritPinLocs(isBaseline, xCoords, yCoords);

  if (xCoords.size() > 1)
  {
    Tree mst = rsmt.fltTree(xCoords, yCoords);
    int wirelength = rsmt.wirelength(mst);
    rsmt.free_tree(mst);
    return wirelength; // v0.6 modify
  }
  else
  {
    return 0;
  }
}

int Net::getNonCritHPWL(bool isBaseline) // cjq modify
{
  int wirelength = 0;
  const Pin *driverPin = getInpin();
  if (!driverPin)
  {
    return 0; // Return 0 if there's no driver pin
  }

  std::vector<int> xCoords;
  std::vector<int> yCoords;
  getMergedNonCritPinLocs(isBaseline, xCoords, yCoords);

  if (xCoords.size() > 1)
  {
    // 初始化
    int xMax, xMin, yMax, yMin;
    xMax = xMin = xCoords[0];
    yMax = yMin = yCoords[0];
    for (int i = 0; i < xCoords.size(); i++)
    {
      xMax = std::max(xMax, xCoords[i]);
      xMin = std::min(xMin, xCoords[i]);
      yMax = std::max(yMax, yCoords[i]);
      yMin = std::min(yMin, yCoords[i]);
    }
    wirelength += xMax - xMin + yMax - yMin;
  }
  return wirelength;
}

bool Net::reportNet()
{
  std::string propStr;
  if (isClock())
  {
    propStr = "clock";
  }

  if (this->isIntraTileNet(false))
  { // report optimized net property
    if (propStr.empty())
    {
      propStr = "intra_tile";
    }
    else
    {
      propStr += "| intra_tile";
    }
  }

  std::cout << "  net_" << id << " " << propStr << std::endl;

  int numNonCritFanoutPins = 0;
  int numCritFanoutPins = 0;
  for (const auto *outpin : getOutputPins())
  {
    if (outpin->getTimingCritical())
    {
      numCritFanoutPins++;
    }
    else
    {
      numNonCritFanoutPins++;
    }
  }
  std::cout << "    Number of critical fanout pins: " << numCritFanoutPins << std::endl;
  std::cout << "    Number of non-critical fanout pins: " << numNonCritFanoutPins << std::endl;
  std::cout << std::endl;
  std::cout << "    Critical wirelength: Baseline = " << getCritWireLength(true) << "; Optimized = " << getCritWireLength(false) << std::endl;
  std::cout << "    Non-critical wirelength: Baseline = " << getNonCritWireLength(true) << "; Optimized = " << getNonCritWireLength(false) << std::endl;
  std::cout << std::endl;

  // if (nonCritWirelength > 0) {

  //   std::vector<int> xCoords;
  //   std::vector<int> yCoords;
  //   getMergedNonCritPinLocs(xCoords, yCoords);

  //   std::cout << "    Net Rectilinear Steiner Minimum Tree with " << xCoords.size() << " merged locations." << std::endl;
  //   if (xCoords.size() > 1) {
  //     Tree mst = rsmt.fltTree(xCoords, yCoords);
  //     rsmt.printtree(mst);
  //   }
  // }

  return true;
}
