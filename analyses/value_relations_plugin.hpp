#ifndef VALUE_RELATIONS_PLUGIN_H
#define VALUE_RELATIONS_PLUGIN_H

#include "dg/llvm/ValueRelations/ValueRelations.h"
#include "instr_plugin.hpp"

namespace llvm {
    class Module;
    class Value;
}

using dg::analysis::LLVMValueRelations;

class ValueRelationsPlugin : public InstrPlugin
{
    LLVMValueRelations VR;
    std::string isValidPointer(llvm::Value* ptr, llvm::Value *len);

public:
    bool supports(const std::string& query) override {
        return query == "isValidPointer";
    }

    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands)
    {
        if (query == "isValidPointer") {
            assert(operands.size() == 2 && "Wrong number of operands");
            return isValidPointer(operands[0], operands[1]);
        } else {
            return "unsupported query";
        }
    }

    ValueRelationsPlugin(llvm::Module* module);
};

#endif

