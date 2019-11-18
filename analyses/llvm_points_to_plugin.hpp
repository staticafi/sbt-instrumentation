#ifndef POINTS_TO_PLUGIN_H
#define POINTS_TO_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <tuple>
#include "instr_plugin.hpp"

class LLVMPointsToPlugin : public InstrPlugin {
    const llvm::DataLayout& DL;

    std::string isNull(llvm::Value* a);
    std::string isValidPointer(llvm::Value* a, llvm::Value *len);
    std::string pointsToHeap(llvm::Value* a);
    std::string pointsToStack(llvm::Value* a);
    std::string pointsToGlobal(llvm::Value* a);
    // Use LLVM AA in the future
    //std::string pointsToSetsOverlap(llvm::Value* a, llvm::Value *b);
    //
public:
    bool supports(const std::string& query) override;
    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands)
    {
        if (query == "isValidPointer") {
            assert(operands.size() == 2 && "Wrong number of operands");
            return isValidPointer(operands[0], operands[1]);
        } else if (query == "isNull") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return isNull(operands[0]);
        } else if (query == "pointsToHeap") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return pointsToHeap(operands[0]);
        } else if (query == "pointsToGlobal") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return pointsToGlobal(operands[0]);
        } else if (query == "pointsToStack") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return pointsToStack(operands[0]);
        } else {
            return "unsupported query";
        }
    }

    LLVMPointsToPlugin(llvm::Module* module)
    : InstrPlugin("LLVMPointsTo"), DL(module->getDataLayout()) {}
};

#endif

