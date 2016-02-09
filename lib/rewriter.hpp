#include <list>
#include <string>
#include <iostream>
#include <fstream>


// Configuration
enum class InstrumentPlacement {
	BEFORE,
	AFTER,
	REPLACE
};

class InstrumentInstruction {
 public:
	std::string returnValue;
	std::string instruction;
	std::list<std::string> parameters;
};

class RewriteRule {
 public:
	InstrumentInstruction foundInstr;
	InstrumentInstruction newInstr;
	InstrumentPlacement where;
	std::string inFunction;
};

typedef std::list<RewriteRule> RewriterConfig;

// Rewriter
class Rewriter {
	RewriterConfig config;
	public:
		RewriterConfig getConfig();
		void parseConfig(std::ifstream &config_file);
};
