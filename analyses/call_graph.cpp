#include "call_graph.hpp"
#include <llvm/IR/Function.h>
#include <list>

#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 5)
#include "llvm/IR/InstIterator.h"
#else
#include "llvm/Support/InstIterator.h"
#endif

using namespace llvm;
using dg::analysis::pta::PSNode;

CallGraph::CallGraph(llvm::Module &M, std::unique_ptr<dg::LLVMPointerAnalysis> &PTA) {
	
	// Go through functions
	for (Module::iterator Fiterator = M.begin(), E = M.end(); Fiterator != E; ++Fiterator) {
		// Look for call instructions
		for (inst_iterator Iiterator = inst_begin(*Fiterator); Iiterator != inst_end(*Fiterator); ++Iiterator){
			if (const CallInst *CI = dyn_cast<CallInst const>(&*Iiterator)){
				handleCallInst(PTA, &*Fiterator, CI);		
			}
		}
	}
}

bool CallGraph::containsCall(const Function* caller, const Function* callee) {
	auto callerCalls = callsMap.equal_range(caller);
	for (auto c = callerCalls.first; c != callerCalls.second; ++c) {
		if (c->second == callee) {
			return true;
		}
	}
	return false;
}

template<typename OutputIterator>
void getCalls(std::unique_ptr<dg::LLVMPointerAnalysis> &PTA, const CallInst *CI, OutputIterator out) {
	const Value *CV = CI->getCalledValue()->stripPointerCasts();
	
	if (const Function *F = dyn_cast<Function>(CV)) {
		*out++ = F;
	} else {
		PSNode *psnode = PTA->getPointsTo(CV);
		for (auto& ptr : psnode->pointsTo) {
			Value *llvmValue = ptr.target->getUserData<llvm::Value>();
			if (const Function *F = dyn_cast<Function>(llvmValue))
					  *out++ = F;
		}
	}
}

void CallGraph::handleCallInst(std::unique_ptr<dg::LLVMPointerAnalysis> &PTA, const Function *F,  const CallInst *CI) {
	std::list<const Value*> calledFunctions;
	getCalls(PTA, CI, std::back_inserter(calledFunctions));

	// Store called functions for F in a map
	for (auto calledValue : calledFunctions) {
		const Function *calledF = dyn_cast<Function>(calledValue);
		if(!containsCall(F, calledF)) {
			callsMap.insert(std::pair<const Function*, const Function*>(F, calledF));
		}
	}
}


