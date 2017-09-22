#ifndef REWRITER_H
#define REWRITER_H

#include <list>
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

class InstrumentInstruction {
 public:
	std::string returnValue;
	std::string instruction;
	std::list<std::string> parameters;
	std::string getSizeTo;
	std::string stripInboundsOffsets;
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
};


class GlobalVarsRule {
 public:
	InstrumentGlobalVar globalVar;
	InstrumentInstruction newInstr;
	std::string inFunction;
	std::list<Condition> conditions;
};

typedef std::list<InstrumentInstruction> InstrumentSequence;

class RewriteRule {
 public:
	InstrumentSequence foundInstrs;
	InstrumentInstruction newInstr;
	InstrumentPlacement where;
	std::string inFunction;
	std::list<Condition> conditions;
};

typedef std::list<RewriteRule> RewriterConfig;

class Phase {
 public:
    RewriterConfig config; 
};

typedef std::list<Phase> Phases;
typedef std::map<std::string, std::string> Flags;
typedef std::pair<std::string, std::string> Flag; 

// Rewriter
class Rewriter {
	Phases phases;
	GlobalVarsRule globalVarsRule;
    Flags flags;
	public:
		std::list<std::string> analysisPaths;
		const Phases& getPhases();
		const GlobalVarsRule& getGlobalsConfig();
		void parseConfig(std::ifstream &config_file);
        void setFlag(std::string name, std::string value);
        bool isFlag(std::string name);
        std::string getFlagValue(std::string name);
        void addFlag(std::string name);
};

#endif
