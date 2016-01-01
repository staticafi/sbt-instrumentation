#include <list>
#include <string>


// Configuration
enum class InstrumentPlacement {
	BEFORE,
	AFTER,
	REPLACE
};

class RewriteRule {
 public:
	std::list<std::string> from;
	std::list<std::string> to;
	InstrumentPlacement where;
};

typedef std::list<RewriteRule> RewriterConfig;

// Rewriter
class Rewriter {
 RewriterConfig config;

 public:
	Rewriter(RewriterConfig config) : config(config) {}

};
