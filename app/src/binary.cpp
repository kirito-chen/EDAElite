#include "binary.h"

//读取二进制文件
std::map<int, int> readBinary(std::string filename){
    std::map<int, int> data;
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading." << filename << std::endl;
        exit(1) ;
    }
    // 读取数据
    inFile.read(reinterpret_cast<char*>(&data), sizeof(data));
    inFile.close();
    for(auto& i : data){
        std::cout<< i.first<<' '<<i.second << std::endl;
    }
    return data;
}

//写入二进制文件
void writeBinary(std::string filename, std::map<int, int> data){
    for(auto& i : data){
        std::cout<< i.first<<' '<<i.second << std::endl;
    }
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing." << filename << std::endl;
        exit(1);
    }
    outFile.write(reinterpret_cast<const char*>(&data), sizeof(data));
    outFile.close();
}

bool fileExists(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
}


// 从 JSON 文件读取并解析数据到 NestedMap
NestedMap readJsonFile(const std::string& filename) {
    NestedMap data;
    std::ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        std::cerr << "Error opening file for reading." << filename << std::endl;
        exit(1);
    }

    std::string line;
    std::string outerKey, innerKey;
    int value;

    while (std::getline(inputFile, line)) {
        // 查找外层键（例如："1"）
        if (line.find("\"") != std::string::npos && line.find(": {") != std::string::npos) {
            outerKey = line.substr(line.find("\"") + 1, line.rfind("\"") - line.find("\"") - 1);
            data[outerKey] = {};
        }

        // 查找内层键值对（例如："60": 11）
        if (line.find(":") != std::string::npos && line.find("{") == std::string::npos) {
            size_t keyStart = line.find("\"") + 1;
            size_t keyEnd = line.find("\"", keyStart);
            innerKey = line.substr(keyStart, keyEnd - keyStart);

            size_t valueStart = line.find(": ", keyEnd) + 2;
            value = std::stoi(line.substr(valueStart));

            data[outerKey][innerKey] = value;
        }
    }

    inputFile.close();
    return data;
}

// 将 NestedMap 写入到 JSON 文件
void writeJsonFile(const std::string& filename, const NestedMap& data) {
    std::ofstream outputFile(filename);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file for writing." << filename << std::endl;
        exit(1);
    }

    outputFile << "{\n";
    for (auto outerIt = data.begin(); outerIt != data.end(); ++outerIt) {
        outputFile << "    \"" << outerIt->first << "\": {\n";
        for (auto innerIt = outerIt->second.begin(); innerIt != outerIt->second.end(); ++innerIt) {
            outputFile << "        \"" << innerIt->first << "\": " << innerIt->second;
            if (std::next(innerIt) != outerIt->second.end()) outputFile << ",";
            outputFile << "\n";
        }
        outputFile << "    }";
        if (std::next(outerIt) != data.end()) outputFile << ",";
        outputFile << "\n";
    }
    outputFile << "}\n";

    outputFile.close();
}