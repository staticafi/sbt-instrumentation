#ifndef CALLGRAPH_H
#define CALLGRAPH_H

#include <map>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowInsensitive.h"

class CallGraph {
	public:
		CallGraph(llvm::Module &M, std::unique_ptr<dg::LLVMPointerAnalysis> &PTA);
		bool containsCall(const llvm::Function* caller, const llvm::Function* callee);

	private:
		void handleCallInst(std::unique_ptr<dg::LLVMPointerAnalysis> &PTA, const llvm::Function *F, const llvm::CallInst *CI);
		std::multimap<const llvm::Function*, const llvm::Function*> callsMap;
};

#endif
