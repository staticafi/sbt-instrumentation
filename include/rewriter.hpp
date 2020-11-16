#ifndef REWRITER_H
#define REWRITER_H

#include <list>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <map>

// Configuration
enum class InstrumentPlacement {
    BEFORE,
    AFTER,
    REPLACE,
    RETURN,
    ENTRY
};

enum class BinOpType {
    NBOP,
    INT32,
    INT64,
    INT16,
    INT8
};

class InstrumentInstruction {
 public:
    std::string returnValue;
    std::string instruction;
    std::list<std::string> parameters;
    std::string getSizeTo;
    std::string stripInboundsOffsets;
    std::list<std::string> getPointerInfoTo;
    std::list<std::string> getPointerInfoMinTo;
    std::list<std::string> getPInfoMinMaxTo;
    BinOpType type;
    std::string getDestType;
};

class InstrumentGlobalVar {
 public:
	std::string globalVariable;
	std::string getSizeTo;
};

class Condition {
    public:
        std::string name;
        std::list<std::string> arguments;
        std::list<std::string> expectedValues;
};


class GlobalVarsRule {
 public:
    InstrumentGlobalVar globalVar;
    InstrumentInstruction newInstr;
    std::string inFunction;
    std::list<Condition> conditions;
    bool mustHoldForAll = false;
};

typedef std::list<InstrumentInstruction> InstrumentSequence;
typedef std::map<std::string, std::string> Flags;
typedef std::pair<std::string, std::string> Flag;

class RewriteRule {
 public:
    InstrumentSequence foundInstrs;
    InstrumentInstruction newInstr;
    InstrumentPlacement where;
    std::string inFunction;
    std::list<Condition> conditions;
    bool mustHoldForAll = false;
    Flags setFlags;
    std::string remember;
    std::string rememberPTSet;
};

typedef std::list<RewriteRule> RewriterConfig;
typedef std::list<GlobalVarsRule> RewriterGlobalsConfig;

class Phase {
 public:
    RewriterConfig config;
    RewriterGlobalsConfig gconfig;
};

typedef std::list<Phase> Phases;

// Rewriter
class Rewriter {
    Phases phases;
    Flags flags;
    public:
        std::vector<std::vector<std::string>> analysisPaths;
        const Phases& getPhases();
        void parseConfig(std::ifstream &config_file);
        void setFlag(std::string name, std::string value);
        bool isFlag(std::string name);
        std::string getFlagValue(std::string name);
};

#endif
