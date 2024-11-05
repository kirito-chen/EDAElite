#pragma once

#include <iostream>
#include <fstream>
#include <vector>

std::vector<std::pair<int, int>> readBinary(std::string filename);
void writeBinary(std::string filename, std::vector<std::pair<int, int>> vec);
bool fileExists(const std::string& filePath);

