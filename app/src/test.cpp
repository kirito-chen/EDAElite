#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include "global.h"
#include "object.h"
#include "rsmt.h"
#include "util.h"
#include "test.h"

int testHPWL()
{
  int totalWirelengthBaseline = 0;
  int totalCritWirelengthBaseline = 0;
  int totalWirelengthOptimized = 0;
  int totalCritWirelengthOptimized = 0;
  for (auto iter : glbNetMap)
  {
    Net *net = iter.second;
    net->setNetHPWL(true);
    net->setNetHPWL(false);
  }
  for (auto iter : glbNetMap)
  {
    Net *net = iter.second;
    std::cout << "netID:" << net->getId() <<" CritHPWL:"<< net->getCritHPWL() <<" total HPWL:"<< net->getHPWL()<<std::endl;
  }

  return 0;
}