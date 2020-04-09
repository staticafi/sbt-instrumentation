#ifndef INFINITE_LOOPS_PLUGIN_H
#define INFINITE_LOOPS_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include "llvm/IR/Instructions.h"
#include "instr_plugin.hpp"

class InfiniteLoopsPlugin : public InstrPlugin
{
private:
    std::string isInfinite(llvm::Value*);
    std::string handleConditional(const llvm::BranchInst*);
    std::string handleUnconditional(const llvm::BranchInst*);

public:
    bool supports(const std::string& query) override;
    std::string query(const std::string& query,
            const std::vector<llvm::Value *>& operands) override
    {
        if (query == "isInfinite") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return isInfinite(operands[0]);
        } else {
            return "unsupported query";
        }
    }

    InfiniteLoopsPlugin(llvm::Module* module) : InstrPlugin("InfiniteLoopsPlugin") {}
};

#endif

