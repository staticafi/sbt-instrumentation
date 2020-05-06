#ifndef VALUE_RELATIONS_PLUGIN_H
#define VALUE_RELATIONS_PLUGIN_H

#include <vector> // just because instr_plugin doesn't include it itself
#include <llvm/IR/Value.h> // just because instr_plugin doesn't include it itself
#include "instr_plugin.hpp"
#include "dg/llvm/ValueRelations/GraphElements.hpp"
#include "dg/llvm/ValueRelations/StructureAnalyzer.hpp"
#include "dg/llvm/ValueRelations/getValName.h"

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>

#include <iostream>

class ValueRelationsPlugin : public InstrPlugin
{
    std::map<const llvm::Instruction *, dg::analysis::vr::VRLocation *> locationMapping;
    std::map<const llvm::BasicBlock *, std::unique_ptr<dg::analysis::vr::VRBBlock>> blockMapping;

    dg::analysis::vr::StructureAnalyzer structure;

    const unsigned maxPass = 20;

    std::string isValidPointer(llvm::Value* ptr, llvm::Value *len);

    std::string isValidForGraph(
            const dg::analysis::vr::ValueRelations& relations,
            const std::vector<bool> validMemory,
            const llvm::GetElementPtrInst* gep,
            uint64_t readSize) const;

public:
    bool supports(const std::string& query) override {
        return query == "isValidPointer";
    }

    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands) override
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

