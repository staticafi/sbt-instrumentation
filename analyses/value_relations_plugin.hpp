#ifndef VALUE_RELATIONS_PLUGIN_H
#define VALUE_RELATIONS_PLUGIN_H

#include "dg/llvm/ValueRelations/GraphElements.h"
#include "dg/llvm/ValueRelations/StructureAnalyzer.h"
#include "dg/llvm/ValueRelations/getValName.h"
#include "instr_plugin.hpp"
#include <llvm/IR/Value.h>
#include <vector>

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

class ValueRelationsPlugin : public InstrPlugin {
    dg::vr::VRCodeGraph codeGraph;
    dg::vr::StructureAnalyzer structure;

    const unsigned maxPass = 20;

    std::string isValidPointer(llvm::Value *ptr, llvm::Value *size);

    std::string isValidForGraph(const dg::vr::ValueRelations &relations,
                                const std::vector<bool> &validMemory,
                                const llvm::GetElementPtrInst *gep, uint64_t readSize) const;

  public:
    bool supports(const std::string &query) override { return query == "isValidPointer"; }

    std::string query(const std::string &query,
                      const std::vector<llvm::Value *> &operands) override {
        if (query == "isValidPointer") {
            assert(operands.size() == 2 && "Wrong number of operands");
            return isValidPointer(operands[0], operands[1]);
        }
        return "unsupported query";
    }

    ValueRelationsPlugin(llvm::Module *module);
};

#endif
