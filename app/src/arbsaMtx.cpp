#include <iomanip>
#include "global.h"
#include "object.h"
#include "arbsa.h"
#include <algorithm>
#include <cmath>
#include "wirelength.h"
#include <random>
// 计时
#include <chrono>
//多线程
#include <thread>
#include <mutex>
std::mutex mtx; // 互斥锁
bool timeupMtx = false; // 超时标志
const int maxThreads = 8; // 最大线程数
//多线程版本
int arbsaMtx(bool isBaseline){

    return 0;
}