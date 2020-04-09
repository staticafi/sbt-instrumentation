#ifndef RANGE_ANALYSIS_PLUGIN_H
#define RANGE_ANALYSIS_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include "instr_plugin.hpp"

class CheckNSWPlugin : public InstrPlugin
{
     private:
        std::string canOverflow(llvm::Value*);

    public:
        bool supports(const std::string& query) override;
        std::string query(const std::string& query,
                const std::vector<llvm::Value *>& operands) override {
            if (query == "canOverflow") {
                assert(operands.size() == 1 && "Wrong number of operands");
                return canOverflow(operands[0]);
            } else {
                return "unsupported query";
            }
        }

        CheckNSWPlugin(llvm::Module* module) : InstrPlugin("CheckNSWPlugin") {}
};

#endif

