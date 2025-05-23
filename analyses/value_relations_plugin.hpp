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
  public:
    using Borders = std::map<size_t, const llvm::Value *>;

  private:
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
                         const std::vector<bool> &validMemory, const llvm::AllocaInst *load,
                         uint64_t readSize) const;
    bool isValidForGraph(const dg::vr::ValueRelations &relations,
                         const std::vector<bool> &validMemory, const llvm::Instruction *inst,
                         uint64_t readSize) const;

    bool storedToInLoop(const dg::vr::VRLocation &join, const llvm::Value *from) const;
    std::pair<const dg::vr::ValueRelations *, dg::vr::ValueRelations::HandlePtr>
    getInitial(const dg::vr::ValueRelations &relations,
               const dg::vr::ValueRelations::Handle &h) const;
    std::pair<const dg::vr::ValueRelations *, dg::vr::ValueRelations::HandlePtr>
    getInitial(const dg::vr::ValueRelations &relations, const llvm::Instruction *val) const;
    dg::vr::ValueRelations::Handle getInitialInThis(const dg::vr::ValueRelations &relations,
                                                    const llvm::Instruction *val) const;

    bool rangeLimitedBy(const dg::vr::ValueRelations &relations, const llvm::Instruction *load,
                        const llvm::Value *from, const llvm::Value *lower,
                        dg::vr::ValueRelations::Handle h, const llvm::Value *upper) const;

    std::vector<std::pair<const llvm::Value *, const llvm::Value *>>
    getDecisive(const dg::vr::VRLocation &loadLoc) const;

    bool satisfiesPreconditions(const dg::vr::CallRelation &callRels,
                                const llvm::Function *function) const;

    // returns true if all relations were merged without conflict
    // conflict signifies, that relations' location is not reachable from given
    // call site
    bool merge(const dg::vr::ValueRelations &relations, const dg::vr::CallRelation &callRels,
               dg::vr::ValueRelations &merged) const;

    bool fillInBorderVals(const llvm::Function *function, dg::vr::ValueRelations &relations) const;

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
