#include <iomanip>
#include "global.h"
#include "wirelength.h"
#include "rsmt.h"

int reportWirelength()
{
  int totalWirelengthBaseline = 0;
  int totalCritWirelengthBaseline = 0;
  int totalWirelengthOptimized = 0;
  int totalCritWirelengthOptimized = 0;
  for (auto iter : glbNetMap)
  {
    Net *net = iter.second;
    if (net->isClock())
    {
      continue;
    }

    totalCritWirelengthBaseline += net->getCritWireLength(true);
    // if (net->getCritWireLength(true) > 0)
    // {
    //   // std::cout << "net_" << net->getId() << std::endl;
    //   net->reportNet();
    // }
    totalWirelengthBaseline += net->getNonCritWireLength(true);

    totalCritWirelengthOptimized += net->getCritWireLength(false);
    totalWirelengthOptimized += net->getNonCritWireLength(false);
  }

  // append critical wirelength to total wirelength
  totalWirelengthBaseline += totalCritWirelengthBaseline;
  totalWirelengthOptimized += totalCritWirelengthOptimized;

  double ratioBaseline = 100.0 * (double)totalCritWirelengthBaseline / (double)totalWirelengthBaseline;
  double ratioOptimized = 100.0 * (double)totalCritWirelengthOptimized / (double)totalWirelengthOptimized;
  std::cout << "  Baseline wirelength: total = " << totalWirelengthBaseline << "; crit = " << totalCritWirelengthBaseline << " (" << std::setprecision(2) << ratioBaseline << "%)" << std::endl;
  std::cout << "  Optimized wirelength: total  = " << totalWirelengthOptimized << "; crit = " << totalCritWirelengthOptimized << " (" << std::setprecision(2) << ratioOptimized << "%)" << std::endl;
  return 0;
}

//cjq modify 获取线长
int getWirelength(bool isBaseline){
  int totalCritWirelength = 0;
  int totalWirelength = 0;
  for (auto iter : glbNetMap)
  {
    Net *net = iter.second;
    if (net->isClock())
    {
      continue;
    }
    totalCritWirelength += net->getCritWireLength(isBaseline);
    totalWirelength += net->getNonCritWireLength(isBaseline);
  }

  // append critical wirelength to total wirelength
  totalWirelength += totalCritWirelength;
  return totalWirelength;
}

// cjq modify 获取inst相关net的线长
int getRelatedWirelength(bool isBaseline, const std::set<int>& instRelatedNetId){  
  int totalCritWirelength = 0;
  int totalWirelength = 0;
  for (int i : instRelatedNetId)
  {
    if(glbNetMap.count(i) > 0){
      Net *net = glbNetMap[i];
      if (net->isClock())
      {
        continue;
      }
      totalCritWirelength += net->getCritWireLength(isBaseline);
      totalWirelength += net->getNonCritWireLength(isBaseline);
    }
    else{
      std::cout<<"getRelatedWirelength can not find this netId:"<<i<<std::endl;
    }
  }

  // append critical wirelength to total wirelength
  totalWirelength += totalCritWirelength;
  return totalWirelength;
}
// cjq modify 获取半周线长
int getHPWL(bool isBaseline){
  int HPWL = 0;
  for (auto iter : glbNetMap)
  {
    Net *net = iter.second;
    if (net->isClock())
    {
      continue;
    }
    //记录引脚位置
    std::set<int> locXSet;  // (*locXSet.rbegin()) 最大值  (*locXSet.begin()) 最小值
    std::set<int> locYSet;
    int x,y,z;
    //inpin
    Pin* inpin = net->getInpin();
    Instance* inst = inpin->getInstanceOwner();
    if(isBaseline){
      std::tie(x,y,z) = inst->getBaseLocation();
    }
    else{
      std::tie(x,y,z) = inst->getLocation();
    }
    locXSet.insert(x);
    locYSet.insert(y);
    //outpin
    std::list<Pin*> outputPin = net->getOutputPins();
    for(auto pin : outputPin){
      Instance* inst = pin->getInstanceOwner();
      if(isBaseline){
        std::tie(x,y,z) = inst->getBaseLocation();
      }
      else{
        std::tie(x,y,z) = inst->getLocation();
      }
      locXSet.insert(x);
      locYSet.insert(y);
    }
    //计算一个net的半周线长
    int oneNetHPWL = (*locXSet.rbegin()) - (*locXSet.begin()) + (*locYSet.rbegin()) - (*locYSet.begin());
    HPWL += oneNetHPWL;
  }

  return HPWL;
}


//wbx 获取pack线长
int getPackWirelength(bool isBaseline){
  int totalCritWirelength = 0;
  int totalWirelength = 0;
  for (auto iter : glbPackNetMap)
  {
    Net *net = iter.second;
    if (net->isClock())
    {
      continue;
    }
    totalCritWirelength += net->getCritWireLength(isBaseline);
    totalWirelength += net->getNonCritWireLength(isBaseline);
  }

  // append critical wirelength to total wirelength
  totalWirelength += totalCritWirelength;
  return totalWirelength;
}