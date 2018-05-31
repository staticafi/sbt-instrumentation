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
        std::string canOverflowTrunc(const Range&, const llvm::TruncInst&);
        std::string canOverflowAdd(const Range&, const Range&, const llvm::IntegerType&);
        std::string canOverflowMul(const Range&, const Range&, const llvm::IntegerType&);
        std::string canOverflowSub(const Range&, const Range&, const llvm::IntegerType&);
        std::string canOverflowDiv(const Range&, const Range&, const llvm::IntegerType&);

     public:
        std::string canOverflow(llvm::Value*);

        RangeAnalysisPlugin(llvm::Module* module) : InstrPlugin("RangeAnalysis") {
            llvm::errs() << "Running range analysis...\n";
            IntraProceduralRA ra;
            for (auto& f : *module) {
                RA.emplace(&f, ra.run(f));
            }
            llvm::errs() << "RA plugin done.\n";
        }
};

#endif

