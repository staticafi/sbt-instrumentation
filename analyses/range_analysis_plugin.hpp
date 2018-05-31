#ifndef RANGE_ANALYSIS_PLUGIN_H
#define RANGE_ANALYSIS_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <map>
#include "instr_plugin.hpp"
#include "RangeAnalysis.h"

class RangeAnalysisPlugin : public InstrPlugin
{
     private:
        std::map<llvm::Function*, Cousot> RA;
        Range getRange(ConstraintGraph&, llvm::Value*);
        std::string canOverflowAdd(const Range&, const Range&, const llvm::IntegerType&);
        std::string canOverflowMul(const Range&, const Range&, const llvm::IntegerType&);
        std::string canOverflowSub(const Range&, const Range&, const llvm::IntegerType&);
        std::string canOverflowDiv(const Range&, const Range&, const llvm::IntegerType&);

     public:
        std::string canOverflow(llvm::Instruction*);

        RangeAnalysisPlugin(llvm::Module* module) : InstrPlugin("RangeAnalysis") {
            llvm::errs() << "Running range analysis...\n";
            for (auto& f : *module) {
                IntraProceduralRA ra;
                RA.emplace(&f, ra.run(f));
            }
            llvm::errs() << "RA plugin done.\n";
        }
};

#endif

