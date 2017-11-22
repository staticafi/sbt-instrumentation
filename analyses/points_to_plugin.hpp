#ifndef POINTS_TO_PLUGIN_H
#define POINTS_TO_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include "instr_plugin.hpp"
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToWithInvalidate.h"
#include "call_graph.hpp"

using dg::analysis::pta::PSNode;
class PointsToPlugin : public InstrPlugin
{
     private:
        std::unique_ptr<dg::LLVMPointerAnalysis> PTA;
        CallGraph cg;

     public:
        std::string isNull(llvm::Value* a);
        std::string isValidPointer(llvm::Value* a, llvm::Value *len);
        std::string pointsTo(llvm::Value* a, llvm::Value *b);
        std::string hasKnownSize(llvm::Value* a);
        virtual std::pair<llvm::Value*, uint64_t> getPointerInfo(llvm::Value* a);
        virtual bool isReachableFunction(const llvm::Function& from, const llvm::Function& f);

        PointsToPlugin(llvm::Module* module) : InstrPlugin("PointsTo") {
            llvm::errs() << "Running points-to analysis...\n";
            PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(module));
            PTA->run<dg::analysis::pta::PointsToWithInvalidate>();
            cg.buildCallGraph(*module, PTA);
        }
};

#endif

