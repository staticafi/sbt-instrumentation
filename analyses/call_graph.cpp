#include "call_graph.hpp"
#include <llvm/IR/Function.h>

#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 5)
#include "llvm/IR/InstIterator.h"
#else
#include "llvm/Support/InstIterator.h"
#endif

using namespace llvm;
using dg::analysis::pta::PSNode;

CallGraph::CallGraph(llvm::Module &M, const dg::LLVMPointerAnalysis &PTA) {
	
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

void CallGraph::handleCallInst(const dg::LLVMPointerAnalysis &PTA, const Function *F,  const CallInst *CI) {
	//TODO
}
