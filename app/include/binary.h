#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <map>

// 解析嵌套结构：外层是字符串（"1"、"2"等），内层包含键值对（"60": 11, "1180": 100 等）
using NestedMap = std::map<std::string, std::map<std::string, int>>;


std::map<int, int> readBinary(std::string filename);
void writeBinary(std::string filename, std::map<int, int> data);
bool fileExists(const std::string& filePath);
NestedMap readJsonFile(const std::string& filename);
void writeJsonFile(const std::string& filename, const NestedMap& data);

