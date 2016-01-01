#include "llvm/IR/DataLayout.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <list>
#include <string>

using namespace llvm;

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
