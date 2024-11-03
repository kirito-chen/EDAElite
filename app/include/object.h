
#pragma once
#include <tuple>  // 包含对 std::tuple 的支持
#include <string> // 包含对 std::string 的支持
#include <list>   // 包含对 std::list 的支持
#include <map>    // 包含对 std::map 的支持
#include <vector> // 包含对 std::vector 的支持
#include <set>    // 包含对 std::set 的支持
#include <limits>
#include <algorithm>

// PLB slots
#define MAX_LUT_CAPACITY 8
#define MAX_DFF_CAPACITY 16
#define MAX_CARRY4_CAPACITY 2
#define MAX_F7_CAPACITY 4
#define MAX_F8_CAPACITY 2
#define MAX_DRAM_CAPACITY 2
#define MAX_SEQ_CAPACITY = 16 // SEQ组的最大容量
// # other slots
#define MAX_RAM_CAPACITY 1
#define MAX_DSP_CAPACITY 1
#define MAX_IO_CAPACITY 2
#define MAX_IPPIN_CAPACITY 256
#define MAX_GCLK_CAPACITY 28

//
#define MAX_TILE_CE_PER_PLB_BANK 2
#define MAX_TILE_RESET_PER_PLB_BANK 1
#define MAX_TILE_CLOCK_PER_PLB_BANK 1

#define MAX_REGION_CLOCK_COUNT 28

#define MAX_TILE_PIN_INPUT_COUNT 112.0 // 48 LUT input pins + 16 SEQ input pins (D) + 48 SEQ ctrl pins (CE CLK SR)
#define MAX_TILE_PIN_OUTPUT_COUNT 32.0 // 16 LUT output pins + 16 SEQ output pins



enum PinProp
{
    PIN_PROP_NONE,
    PIN_PROP_CE,
    PIN_PROP_RESET,
    PIN_PROP_CLOCK
};

class Instance;
class Net;
class Slot
{
private:
    // normally each slot is holding 1 instance
    // the exception is the LUT slot can hold up to 2 LUTs
    // with shared inputs
    std::list<int> optimizedInstArr; // container of the optimized design instances
    std::list<int> baselineInstArr;  // container of the input design instances
public:
    // Constructor
    Slot() {}
    // Destructor
    ~Slot() {}

    // Getter and setter for type
    void clearInstances()
    {
        optimizedInstArr.clear();
        baselineInstArr.clear();
    }
    void clearBaselineInstances() { baselineInstArr.clear(); }
    void clearOptimizedInstances() { optimizedInstArr.clear(); }

    void addOptimizedInstance(int instID) { optimizedInstArr.push_back(instID); }
    std::list<int> getOptimizedInstances() const { return optimizedInstArr; }

    void addBaselineInstance(int instID) { baselineInstArr.push_back(instID); }
    std::list<int> &getBaselineInstances() { return baselineInstArr; }
    std::list<int> &getOptimizedInstancesRef() { return optimizedInstArr; }
};

typedef std::vector<Slot *> slotArr;

struct LUTUsage
{
    int numInstances;            // 已用的LUT实例数量
    std::set<int> remainingPins; // 使用该引脚的netID
};

class Tile {
    private:
        int col;
        int row;
        std::set<std::string> tileTypes;  //cjq 包含PLB DSP RAMA IOA等

    // container to record instances belone to this tile
    std::map<std::string, slotArr> instanceMap;

    // 新增成员变量：lutUsage，存储每个LUT site的使用情况——吴白轩——2024年10月18日
    // 使用 vector 存储每个LUT site的详细使用情况
    std::vector<LUTUsage> lutUsage;
    int dffUsage; // 剩余的DFF资源

public:
    // Constructor
    Tile(int c, int r) : col(c), row(r), lutUsage(MAX_LUT_CAPACITY) {} // 初始化lutUsage为全2——吴白轩
    // Tile(int c, int r) : col(c), row(r) {}

    // Destructor
    ~Tile();

    // Getter and setter
    int getCol() const { return col; }
    void setCol(int value) { col = value; }
    int getRow() const { return row; }
    void setRow(int value) { row = value; }

    // Getter and setter for tileTypes
    std::set<std::string> getTileTypes() const { return tileTypes; }
    void addType(const std::string &tileType) { tileTypes.insert(tileType); }
    unsigned int getNumTileTypes() const { return tileTypes.size(); }

    std::string getLocStr() { return "X" + std::to_string(col) + "Y" + std::to_string(row); }

    bool initTile(const std::string &tileType);   // allocate slots
    bool matchType(const std::string &modelType); // LUT/SEQ to PLB

    bool isEmpty(bool isBaseline);
    bool addInstance(int instID, int offset, std::string modelType, const bool isBaseline);
    void clearInstances();
    void clearBaselineInstances();
    void clearOptimizedInstances();
    void clearLUTandSEQOptimizedInstances(); // 只清理LUT和SEQ
    std::map<std::string, slotArr>::iterator getInstanceMapBegin() { return instanceMap.begin(); }
    std::map<std::string, slotArr>::iterator getInstanceMapEnd() { return instanceMap.end(); }
    slotArr *getInstanceByType(std::string type);

    bool getControlSet(
        const bool isBaseline,
        const int bank,
        std::set<int> &clkNets,
        std::set<int> &ceNets,
        std::set<int> &srNets);

    std::set<int> getConnectedLutSeqInput(bool isBaseline);
    std::set<int> getConnectedLutSeqOutput(bool isBaseline);

    // report util
    void getRemainingPLBResources(bool isBaseline);
    std::set<int> getUsedPins(Instance *inst);
    void reportTile();

    // 判断tile内部有没有足够资源
    bool hasEnoughResources(Instance *inst);
    // 移除inst
    bool removeInstance(Instance *inst);

    // cjq modify 返回tile的instTypes类型的可插入的offset  // LUT  SEQ
    int findOffset(std::string instTypes, Instance *inst, bool isBaseline);
    int getLUTCount() const;

    std::vector<std::set<Instance*>> getFixedOptimizedLUTGroups() const;
    std::vector<int> getFixedOptimizedDRAMGroups() const;

    
    std::vector<int> getSeqInstanceBankNum();
};

class ClockRegion
{
private:
    std::string regionName;
    int xLeft;
    int xRight;
    int yTop;
    int yBottom;
    std::set<int> clockNets; // <netID>
public:
    // Constructor
    ClockRegion() : regionName("undefined"), xLeft(0), xRight(0), yTop(0), yBottom(0)
    {
        // Add your constructor code here
    }

    // Destructor
    ~ClockRegion()
    {
        // Add your destructor code here
    }

    // Getter and setter for regionName
    std::string getRegionName() const
    {
        return regionName;
    }
    void setRegionName(const std::string &name)
    {
        regionName = name;
    }

    void setBoundingBox(int left, int right, int bottom, int top)
    {
        xLeft = left;
        xRight = right;
        yTop = top;
        yBottom = bottom;
    }

    // Getter for xLeft, xRight, yTop, yBottom
    int getXLeft() const { return xLeft; }
    int getXRight() const { return xRight; }
    int getYTop() const { return yTop; }
    int getYBottom() const { return yBottom; }

    void addClockNet(int netID) { clockNets.insert(netID); }
    int getNumClockNets() const { return clockNets.size(); }
    void clearClockNets() { clockNets.clear(); }

    // report util
    std::string getLocStr() { return "[" + std::to_string(xLeft) + "," + std::to_string(yBottom) + "][" + std::to_string(xRight) + "," + std::to_string(yTop) + "]"; }
    void reportClockRegion();
};

class Lib
{
    std::string name;
    std::vector<std::pair<std::string, PinProp>> inputs;
    std::vector<std::pair<std::string, PinProp>> outputs;

public:
    Lib(std::string libname) : name(libname) {} // 默认构造函数
    ~Lib() {}                                   // 析构函数

    // Getter and setter for name
    std::string getName() const { return name; }

    // Getter and setter for inputs
    int getNumInputs() const { return inputs.size(); }
    void setNumInputs(const int numIn) { inputs.resize(numIn); }
    std::vector<std::pair<std::string, PinProp>> getInputs() const { return inputs; }
    void setInput(int idx, std::string input, PinProp prop)
    {
        inputs[idx].first = input;
        inputs[idx].second = prop;
    }

    // Getter and setter for outputs
    int getNumOutputs() const { return outputs.size(); }
    void setNumOutputs(const int numOut) { outputs.resize(numOut); }
    std::vector<std::pair<std::string, PinProp>> getOutputs() const { return outputs; }
    void setOutput(int idx, std::string output, PinProp prop)
    {
        outputs[idx].first = output;
        outputs[idx].second = prop;
    }

    PinProp getInputProp(int idx) const { return inputs[idx].second; }
    PinProp getOutputProp(int idx) const { return outputs[idx].second; }
};

class Pin
{
    int netID;
    PinProp prop;
    bool timingCritical;
    Instance *instanceOwner;

public:
    Pin() : netID(-1), prop(PIN_PROP_NONE), timingCritical(false), instanceOwner(nullptr) {} // 默认构造函数
    ~Pin() {}                                                                                // 析构函数

    // Getter and setter for netID
    int getNetID() const { return netID; } // -1 means unconnected
    void setNetID(int value) { netID = value; }

    // Getter and setter for prop
    PinProp getProp() const { return prop; }
    void setProp(PinProp value) { prop = value; }

    // Getter and setter for isTimingCritical
    bool getTimingCritical() const { return timingCritical; }
    void setTimingCritical(bool value) { timingCritical = value; }

    // Getter and setter for instanceOwner
    Instance *getInstanceOwner() const { return instanceOwner; }
    void setInstanceOwner(Instance *inst) { instanceOwner = inst; }
};

class Instance
{
    bool fixed;                             // 声明 fixed 成员变量
    Lib *cellLib;                           // 声明 cellLib 成员变量
    std::string instanceName;               // 声明 instanceName 成员变量
    std::string modelName;                  // 声明 modelName 成员变量
    std::tuple<int, int, int> baseLocation; // location before optimization
    std::tuple<int, int, int> location;     // location after optimization
    std::vector<Pin *> inpins;
    std::vector<Pin *> outpins;

    int allRelatedNetHPWL;     // 存取与inst相关的所有net的HPWL之和
    int allRelatedNetHPWLAver; // cjq modify 24.10.20 平均HPWL

    std::vector<int> movableRegion; // wbx，inst的可移动区域，由inst的net最小外包矩形给出
    int matchedLUTID;               // 用于存储与当前LUT匹配的LUT实例ID

    bool isMatch;
    int plbGroupID;
    int lutSetID;       //指定LUT的LUT组编号

    bool lutInitialed; // 这个是用来确保LUT类型的instance在updateInstancesToTiles只被调整一次位置，默认为false
    int seqGroupID;

public:
    Instance();
    ~Instance();

    void modifyFixed(bool _fix) { fixed = _fix; }

    bool isMatched() { return isMatch; }

    // 添加 Getter 和 Setter 函数
    int getMatchedLUTID() const { return matchedLUTID; }
    void setMatchedLUTID(int lutID) { matchedLUTID = lutID; }

    // Getter and setter
    std::tuple<int, int, int> getBaseLocation() const { return baseLocation; }
    void setBaseLocation(const std::tuple<int, int, int> &loc) { baseLocation = loc; }

    std::tuple<int, int, int> getLocation() const { return location; }
    void setLocation(const std::tuple<int, int, int> &loc) { location = loc; }

    bool isFixed() const { return fixed; }
    void setFixed(bool value) { fixed = value; }
    
    bool isLUTInitial() const { return lutInitialed; }
    void setLUTInitial(bool _isLUTInitial) { lutInitialed = _isLUTInitial; }

    std::string getInstanceName() const { return instanceName; }
    void setInstanceName(const std::string &name) { instanceName = name; }

    std::string getModelName() const { return modelName; }
    void setModelName(const std::string &name) { modelName = name; }

    Lib *getCellLib() const { return cellLib; }
    void setCellLib(Lib *lib);

    bool isPlaced();
    bool isMoved();

    void createInpins();
    int getNumInpins() const { return inpins.size(); }
    int getUsedNumInpins() const;
    std::vector<Pin *> getInpins() const { return inpins; }
    Pin *getInpin(int idx) const { return inpins[idx]; }
    // void connectInpin(int netID, int idx) { inpins[idx].setNetID(netID); }

    void createOutpins();
    int getNumOutpins() const { return outpins.size(); }
    std::vector<Pin *> getOutpins() const { return outpins; }
    Pin *getOutpin(int idx) const { return outpins[idx]; }

    // wbx—2024年10月19日
    std::set<Net *> getRelatedNets() const;
    void calculateAllRelatedNetHPWL(bool isBaseline);
    void updateInstRelatedNet(bool isBaseline);

    // Getter and setter for HPWL
    int getAllRelatedNetHPWL() const { return allRelatedNetHPWL; }
    int getAllRelatedNetHPWLAver() const { return allRelatedNetHPWLAver; }

    // 获取instance的可移动区域
    void generateMovableRegion();
    std::vector<int> getMovableRegion() { return movableRegion; }

    // void connectOutpin(int netID, int idx) { outpins[idx].setNetID(netID); }
    void setPLBGroupID(int plbID) { plbGroupID = plbID; }
    int getPLBGroupID() { return plbGroupID; }

    void setLUTSetID(int _lutsetID) { lutSetID = _lutsetID; }
    int getLUTSetID() { return lutSetID; }

    void setSEQID(int _seqID) { seqGroupID = _seqID; }
    int getSEQID() { return seqGroupID; }
    int getInstID()
    {
        size_t underscorePos = instanceName.find('_');                                                        // 找到下划线的位置
        return (underscorePos != std::string::npos) ? std::stoi(instanceName.substr(underscorePos + 1)) : -1; // 提取并转换
    }
};

class Net
{
    int id;     // 声明 id 成员变量
    bool clock; // 声明 clock 成员变量
    Pin *inpin;
    std::list<Pin *> outputPins;
    int critHPWL; // 当前net的关键路径半周线长
    int HPWL;     // 当前net的总半周线长

    std::vector<int> netArea; // net 的最小外包矩形

public:
    Net(int netID) : id(netID), clock(false), inpin(nullptr)
    {
        netArea.assign(4, -1); //(xlb,ylb,xrt,yrt)
    } // 默认构造函数
    ~Net() {} // 析构函数

    // Getter and setter for id
    int getId() const { return id; }

    // Getter and setter for clock
    bool isClock() const { return clock; }
    void setClock(bool value) { clock = value; }

    // as named, is this net fanin and all fanouts are in the same tile
    bool isIntraTileNet(bool isBaseline);

    // Getter and setter for inpin
    Pin *getInpin() const { return inpin; }
    void setInpin(Pin *pin) { inpin = pin; }

    // Getter and setter for outputPins
    std::list<Pin *> getOutputPins() const { return outputPins; }
    void addOutputPin(Pin *pin) { outputPins.push_back(pin); }

    int setNetHPWL(bool isBaseline); // cjq 设置每个net的半周线长
    int getCritHPWL() { return critHPWL; }
    int getHPWL() { return HPWL; }

    int getNumPins();

    bool addConnection(std::string conn);

    int getCritWireLength(bool isBaseline);
    int getCritHPWL(bool isBaseline); // cjq 增加半周线长的计算方式

    void getMergedNonCritPinLocs(bool isBaseline, std::vector<int> &xCoords, std::vector<int> &yCoords); // 返回不是关键时序上的节点
    int getNonCritWireLength(bool isBaseline);
    int getNonCritHPWL(bool isBaseline); // cjq 增加半周线长的计算方式

    std::vector<int> getBoundingBox() { return netArea; } // 获取最小外包矩形信息

    // report util
    bool reportNet();

    std::vector<Pin *> getPins();
};

class PLBPlacement
{
private:
    int plbID;                                   // PLB ID
    std::tuple<int, int> location;               // PLB 的 (x, y) 位置, PLB组不需要z位置
    std::set<int> connectedNets;                 // PLB 组的引脚连接情况，存储 net ID
    std::vector<std::set<Instance *>> lutGroups; // 当前 PLB 内部的 LUT 组
    bool isFixed;                                // 当前 PLB 组的固定情况

public:
    // 默认构造函数
    PLBPlacement() : plbID(-1)
    {
        location = std::tuple<int, int>(-1, -1);
        isFixed = false;
    }

    // 带有 ID 的构造函数
    PLBPlacement(int id) : plbID(id) {}

    // 获取和设置 PLB ID
    int getPLBID() const { return plbID; }
    void setPLBID(int id) { plbID = id; }

    // 获取和设置位置
    std::tuple<int, int> getLocation() const { return location; }
    void setPLBLocation(const std::tuple<int, int> &loc)
    {
        location = loc;
    }

    // 获取和更新连接的 nets
    const std::set<int> &getConnectedNets() const { return connectedNets; }
    void addConnectedNet(int netID) { connectedNets.insert(netID); }

    // 获取和设置 LUT 组
    const std::vector<std::set<Instance *>> &getLUTGroups() const { return lutGroups; }
    void addLUTGroup(const std::set<Instance *> &lutGroup) { lutGroups.push_back(lutGroup); }

    bool getFixed() const { return isFixed; }

    // 获取 PLB 内的总 LUT 数
    int getTotalLUTCount() const
    {
        int count = 0;
        for (const auto &group : lutGroups)
        {
            count += group.size();
        }
        return count;
    }

    void setFixed(bool isfix) { isFixed = isfix; }
};

class SEQBankPlacement
{
private:
    int bankID;                           // Bank ID
    std::tuple<int, int> location;        // Bank的(x, y)位置
    std::vector<Instance *> seqInstances; // 当前Bank内的SEQ实例
    std::set<int> clkNets;                // 时钟引脚
    std::set<int> ceNets;                 // CE引脚
    std::set<int> srNets;                 // RESET引脚
    bool isFixed;                         // 当前Bank是否固定

public:
    // 默认构造函数
    SEQBankPlacement() : bankID(-1)
    {
    }

    // 构造函数
    SEQBankPlacement(int id) : bankID(id), isFixed(false) {}

    // 设置和获取Bank的ID
    int getBankID() const { return bankID; }
    void setBankID(int id) { bankID = id; }

    // 获取和设置位置
    std::tuple<int, int> getLocation() const { return location; }
    void setLocation(int x, int y) { location = std::make_tuple(x, y); }

    // 添加SEQ实例到当前Bank
    bool addInstance(Instance *inst)
    {
        int newClk = clkNets.size();
        int newCE = ceNets.size();
        int newSR = srNets.size();

        for (Pin *pin : inst->getInpins())
        {
            int netID = pin->getNetID();
            PinProp prop = pin->getProp();

            if (prop == PIN_PROP_CLOCK && clkNets.find(netID) == clkNets.end())
                newClk++;
            if (prop == PIN_PROP_CE && ceNets.find(netID) == ceNets.end())
                newCE++;
            if (prop == PIN_PROP_RESET && srNets.find(netID) == srNets.end())
                newSR++;
        }

        // 检查是否超过Bank约束
        if (newClk <= MAX_TILE_CLOCK_PER_PLB_BANK && newCE <= MAX_TILE_CE_PER_PLB_BANK && newSR <= MAX_TILE_RESET_PER_PLB_BANK)
        {
            seqInstances.push_back(inst);
            for (Pin *pin : inst->getInpins())
            {
                int netID = pin->getNetID();
                PinProp prop = pin->getProp();

                if (prop == PIN_PROP_CLOCK)
                    clkNets.insert(netID);
                if (prop == PIN_PROP_CE)
                    ceNets.insert(netID);
                if (prop == PIN_PROP_RESET)
                    srNets.insert(netID);
            }
            return true;
        }
        return false;
    }

    int getSEQCount()
    {
        return seqInstances.size();
    }

    // 设置和获取是否固定
    bool getFixed() const { return isFixed; }
    void setFixed(bool fixed) { isFixed = fixed; }

    // 获取 SEQ 实例列表
    const std::vector<Instance *> &getSEQInstances() const { return seqInstances; }
};