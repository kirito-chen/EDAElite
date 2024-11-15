#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"
#include <chrono> // 计时

int arbsa(bool isBaseline,  std::string nodesFile); 

int arbsaMtx(bool isBaseline); //多线程版本