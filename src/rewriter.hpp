#include <list>
#include <string>


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
};

typedef std::list<RewriteRule> RewriterConfig;

// Rewriter
class Rewriter {
 RewriterConfig config;

 public:
	Rewriter(RewriterConfig config) : config(config) {}

};
