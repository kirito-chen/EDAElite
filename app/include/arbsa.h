#pragma once

#include <iostream>
#include <map>
#include "object.h"
#include "arch.h"
#include <chrono> // 计时

int arbsa(bool isBaseline, std::string nodeFile, const std::chrono::time_point<std::chrono::high_resolution_clock>& start); 

int arbsaMtx(bool isBaseline); //多线程版本