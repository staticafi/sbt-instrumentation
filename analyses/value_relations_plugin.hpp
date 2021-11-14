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

    std::vector<dg::vr::AllocatedSizeView>
    getAllocatedViews(const dg::vr::ValueRelations &relations, const std::vector<bool> &validMemory,
                      const llvm::Value *ptr) const;

    bool isValidForGraph(const dg::vr::ValueRelations &relations,
                         const std::vector<bool> &validMemory, const llvm::GetElementPtrInst *gep,
                         uint64_t readSize) const;
    bool isValidForGraph(const dg::vr::ValueRelations &relations,
                         const std::vector<bool> &validMemory, const llvm::LoadInst *load,
                         uint64_t readSize) const;
    bool isValidForGraph(const dg::vr::ValueRelations &relations,
                         const std::vector<bool> &validMemory, const llvm::AllocaInst *alloca,
                         uint64_t readSize) const;
    bool isValidForGraph(const dg::vr::ValueRelations &relations,
                         const std::vector<bool> &validMemory, const llvm::Instruction *inst,
                         uint64_t readSize) const;

    bool satisfiesPreconditions(const dg::vr::CallRelation &callRels,
                                const llvm::Function *function) const;

    // returns true if all relations were merged without conflict
    // conflict signifies, that relations' location is not reachable from given
    // call site
    bool merge(const dg::vr::ValueRelations &relations, const dg::vr::CallRelation &callRels,
               dg::vr::ValueRelations &merged) const;

    bool fillInBorderValues(const std::vector<dg::vr::BorderValue> &borderValues,
                            const llvm::Function *func, dg::vr::ValueRelations &target) const;

    std::vector<bool> getValidMemory(const dg::vr::ValueRelations &relations,
                                     const dg::vr::ValueRelations &callRels) const;

    std::string isValidPointer(llvm::Value *ptr, llvm::Value *len);

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
