#ifndef REWRITER_H
#define REWRITER_H

#include <list>
#include <string>
#include <iostream>
#include <fstream>


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

class GlobalVarsRule {
 public:
	InstrumentGlobalVar globalVar;
	InstrumentInstruction newInstr;
	std::string inFunction;
	std::list<std::string> condition;
};

typedef std::list<InstrumentInstruction> InstrumentSequence;

class RewriteRule {
 public:
	InstrumentSequence foundInstrs;
	InstrumentInstruction newInstr;
	InstrumentPlacement where;
	std::string inFunction;
	std::list<std::string> condition;
};

typedef std::list<RewriteRule> RewriterConfig;

class Phase {
 public:
    RewriterConfig config; 
};

typedef std::list<Phase> Phases;

// Rewriter
class Rewriter {
	Phases phases;
	GlobalVarsRule globalVarsRule;
	public:
		const Phases& getPhases();
		const GlobalVarsRule& getGlobalsConfig();
		void parseConfig(std::ifstream &config_file);
		std::list<std::string> analysisPaths;
};

#endif
