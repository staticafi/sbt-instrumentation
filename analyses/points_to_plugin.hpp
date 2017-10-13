#ifndef POINTS_TO_PLUGIN_H
#define POINTS_TO_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include "instr_plugin.hpp"
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowSensitive.h"
#include "call_graph.hpp"

using dg::analysis::pta::PSNode;
class PointsToPlugin : public InstrPlugin
{
     private:
        std::unique_ptr<dg::LLVMPointerAnalysis> PTA;
        CallGraph cg;
        
     public:
        bool isNull(llvm::Value* a);
        bool isValidPointer(llvm::Value* a, llvm::Value *len); 
        bool isEqual(llvm::Value* a, llvm::Value* b); 
        bool isNotEqual(llvm::Value* a, llvm::Value* b); 
        bool greaterThan(llvm::Value* a, llvm::Value* b);
        bool lessThan(llvm::Value* a, llvm::Value* b); 
        bool lessOrEqual(llvm::Value* a, llvm::Value* b);
        bool greaterOrEqual(llvm::Value* a, llvm::Value* b);
        bool knownSize(llvm::Value* a);
        virtual uint64_t getAllocatedSize(llvm::Value* a);
        virtual bool isReachableFunction(const llvm::Function& from, const llvm::Function& f);

        PointsToPlugin(llvm::Module* module) : InstrPlugin("PointsTo") {
            llvm::errs() << "Running points-to analysis...\n";
            PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(module));
            PTA->run<dg::analysis::pta::PointsToFlowSensitive>();
            cg.buildCallGraph(*module, PTA);
        }
};

#endif

