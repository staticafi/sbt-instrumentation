#ifndef CALLGRAPH_H
#define CALLGRAPH_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowInsensitive.h"

class CallGraph {
	public:
		CallGraph(llvm::Module &M, const dg::LLVMPointerAnalysis &PTA);

	private:
		void handleCallInst(const dg::LLVMPointerAnalysis &PTA, const llvm::Function *F, const llvm::CallInst *CI);
};

#endif
