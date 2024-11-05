#include "binary.h"

//读取二进制文件
std::vector<std::pair<int, int>> readBinary(std::string filename){
    std::vector<std::pair<int, int>> data;
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading." << filename << std::endl;
        exit(1) ;
    }
    // 读取数据
    std::pair<int, int> pair;
    while (inFile.read(reinterpret_cast<char*>(&pair), sizeof(pair))) {
        std::cout<<pair.first<<' '<<pair.second<<" data.bin read\n";
        data.push_back(pair);
    }
    inFile.close();
    return data;
}

//写入二进制文件
void writeBinary(std::string filename, std::vector<std::pair<int, int>> data){
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error opening file for writing." << filename << std::endl;
        exit(1);
    }

    for (const auto& pair : data) {
        std::cout<<pair.first<<' '<<pair.second<<" data.bin write\n";
        outFile.write(reinterpret_cast<const char*>(&pair), sizeof(pair));
    }

    outFile.close();
}

bool fileExists(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
}