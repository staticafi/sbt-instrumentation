#include "value_relations_plugin.hpp"

#if LLVM_VERSION_MAJOR < 5
#include <llvm/ADT/iterator_range.h>
#endif

#include "dg/llvm/ValueRelations/GraphBuilder.h"
#include "dg/llvm/ValueRelations/RelationsAnalyzer.h"
#include "dg/llvm/ValueRelations/StructureAnalyzer.h"

using namespace dg::vr;

ValueRelationsPlugin::ValueRelationsPlugin(llvm::Module *module)
        : InstrPlugin("ValueRelationsPlugin"), structure(*module, codeGraph) {
    assert(module);
    GraphBuilder gb(*module, codeGraph);
    gb.build();

    structure.analyzeBeforeRelationsAnalysis();

    RelationsAnalyzer ra(*module, codeGraph, structure);
    ra.analyze(maxPass);

    structure.analyzeAfterRelationsAnalysis();
}

// if gep has any zero indices at the beginning, function returns first non-zero index
// which is not followed by any other index
// else it returns nullptr instead of the index
const llvm::Value *getRelevantIndex(const llvm::GetElementPtrInst *gep) {
    if (gep->hasAllZeroIndices())
        return llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(gep->getContext()), 0);

    const llvm::Value *firstIndex = nullptr;
#if LLVM_VERSION_MAJOR < 5
    for (const llvm::Value *index : llvm::make_range(gep->idx_begin(), gep->idx_end())) {
#else
    for (const llvm::Value *index : gep->indices()) {
#endif
        // consider only cases when nonzero index is the last
        if (firstIndex)
            return nullptr;

        if (auto constIndex = llvm::dyn_cast<llvm::ConstantInt>(index)) {
            if (constIndex->isZero())
                continue;
        }

        firstIndex = index;
    }
    return firstIndex;
}

std::vector<AllocatedSizeView>
ValueRelationsPlugin::getAllocatedViews(const ValueRelations &relations,
                                        const std::vector<bool> &validMemory,
                                        const llvm::Value *ptr) const {
    const AllocatedArea *area = nullptr;
    unsigned index = 0;
    for (const llvm::Value *equal : relations.getEqual(ptr)) {
        std::tie(index, area) = structure.getAllocatedAreaFor(equal);
        // if the pointed-to memory was allocated by ordinary means
        if (area) {
            // if it is valid at given location
            if (index < validMemory.size() && validMemory[index]) {
                return area->getAllocatedSizeViews();
            }
            return {};
        }
    }
    return {};
}

// returns the verdict of gep validity for given relations graph
bool ValueRelationsPlugin::isValidForGraph(const ValueRelations &relations,
                                           const std::vector<bool> &validMemory,
                                           const llvm::GetElementPtrInst *gep,
                                           uint64_t readSize) const {
    const auto *alloca = relations.getInstance<llvm::AllocaInst>(gep->getPointerOperand());
    if (!alloca || !isValidForGraph(relations, validMemory, alloca, readSize))
        return false;

    const llvm::Value *gepIndex = getRelevantIndex(gep);
    // if gep has more indices than one, or there are zeros after
    if (!gepIndex)
        return false;

    const llvm::Type *gepType = gep->getType()->getPointerElementType();
    uint64_t gepElemSize = AllocatedArea::getBytes(gepType);

    const llvm::Constant *zero = llvm::ConstantInt::getSigned(gepIndex->getType(), 0);
    if (!relations.are(zero, Relations::SLE, gepIndex))
        return false;

    const auto &views = getAllocatedViews(relations, validMemory, gep->getPointerOperand());

    // // check if index doesnt point after memory
    for (const AllocatedSizeView &view : views) {
        if (view.elementSize != gepElemSize)
            continue;

        if (relations.are(gepIndex, Relations::SLT, view.elementCount)) {
            if (gepElemSize <= view.elementSize)
                return readSize <= view.elementSize;
        }
    }
    return false;
}

bool ValueRelationsPlugin::isValidForGraph(const ValueRelations &relations,
                                           const std::vector<bool> &validMemory,
                                           const llvm::LoadInst *load, uint64_t readSize) const {
    for (auto handleRel : relations.getRelated(load, Relations().sge())) {
        if (handleRel.second.has(Relations::EQ))
            continue;

        auto maybeBaseH = handleRel.first;
        auto foo = relations.getInstance<llvm::AllocaInst>(maybeBaseH);
        if (!foo)
            continue;

        if (!isValidForGraph(relations, validMemory, foo, readSize))
            continue;
        assert(!relations.getEqual(maybeBaseH).empty());
        const llvm::Value *any = *relations.getEqual(maybeBaseH).begin();

        const llvm::Type *loadType = load->getType();
        uint64_t loadSize = AllocatedArea::getBytes(loadType);

        const auto &views = getAllocatedViews(relations, validMemory, any);

        for (auto greaterRel : relations.getRelated(load, Relations().sle())) {
            if (handleRel.second.has(Relations::EQ))
                continue;

            auto maybeCeilH = greaterRel.first;
            auto bar = relations.getInstance<llvm::GetElementPtrInst>(maybeCeilH);
            if (!bar)
                continue;

            if (!isValidForGraph(relations, validMemory, bar, readSize))
                continue;

            return true;
        }

        auto zero = llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(load->getContext()), 0);
        for (auto related : relations.getRelated(zero, Relations().slt())) {
            auto equal = relations.getEqual(related.first);
            if (equal.empty())
                continue;

            auto val = *relations.getEqual(related.first).begin();
            const llvm::Instruction *anyRelated = llvm::dyn_cast<llvm::Instruction>(val);
            if (!anyRelated || anyRelated->getFunction() != load->getFunction())
                continue;

            for (const AllocatedSizeView &view : views) {
                if (relations.are(related.first, Relations::SLE, view.elementCount)) {
                    if (loadSize <= view.elementSize)
                        return readSize <= view.elementSize;
                }
            }
        }
    }
    return false;
}

bool ValueRelationsPlugin::isValidForGraph(const dg::vr::ValueRelations &relations,
                                           const std::vector<bool> &validMemory,
                                           const llvm::AllocaInst *alloca,
                                           uint64_t readSize) const {
    const llvm::Constant *zero = llvm::ConstantInt::getSigned(alloca->getOperand(0)->getType(), 0);
    const auto &views = getAllocatedViews(relations, validMemory, alloca);

    for (const AllocatedSizeView &view : views) {
        if (view.elementSize != readSize)
            continue;

        if (relations.are(zero, Relations::SLT, view.elementCount))
            return true;
    }
    return false;
}

bool ValueRelationsPlugin::isValidForGraph(const ValueRelations &relations,
                                           const std::vector<bool> &validMemory,
                                           const llvm::Instruction *inst, uint64_t readSize) const {
    if (const auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(inst))
        return isValidForGraph(relations, validMemory, gep, readSize);
    if (const auto *load = llvm::dyn_cast<llvm::LoadInst>(inst))
        return isValidForGraph(relations, validMemory, load, readSize);
    if (const auto *alloca = llvm::dyn_cast<llvm::AllocaInst>(inst))
        return isValidForGraph(relations, validMemory, alloca, readSize);
    return false;
}

const llvm::Value *getRealArg(const CallRelation &callRels, const llvm::Value *formalArg) {
    const llvm::Value *arg = nullptr;
    for (const auto &pair : callRels.equalPairs) {
        if (formalArg == pair.first)
            arg = pair.second;
    }
    assert(arg);
    return arg;
}

std::pair<bool, uint64_t> getReadSize(const llvm::Value *size) {
    if (auto *constant = llvm::dyn_cast<llvm::ConstantInt>(size)) {
        uint64_t readSize = constant->getLimitedValue();

        // size may not be expressible as uint64_t
        return {readSize != ~((uint64_t) 0), readSize};
    }
    return {false, 0};
}

bool ValueRelationsPlugin::satisfiesPreconditions(const CallRelation &callRels,
                                                  const llvm::Function *function) const {
    if (!structure.hasPreconditions(function))
        return true;

    assert(callRels.callSite);
    for (auto &prec : structure.getPreconditionsFor(function)) {
        const llvm::Value *arg = getRealArg(callRels, prec.arg);
        if (!callRels.callSite->relations.are(arg, prec.rel, prec.val))
            return false;
    }
    return true;
}

bool ValueRelationsPlugin::merge(const ValueRelations &relations, const CallRelation &callRels,
                                 ValueRelations &merged) const {
    merged.merge(relations);
    for (auto &equalPair : callRels.equalPairs) {
        if (merged.hasConflictingRelation(equalPair.first, equalPair.second, Relations::EQ)) {
            return false; // this vrlocation is unreachable with given parameters
        }
        merged.setEqual(equalPair.first, equalPair.second);
    }
    return merged.merge(callRels.callSite->relations);
}

bool ValueRelationsPlugin::fillInBorderValues(const std::vector<BorderValue> &borderValues,
                                              const llvm::Function *func,
                                              ValueRelations &target) const {
    const ValueRelations &entryRels = codeGraph.getEntryLocation(*func).relations;

    for (const auto &borderVal : borderValues) {
        for (auto stored : entryRels.getEqual(entryRels.getPointedTo(borderVal.handle))) {
            for (auto from : target.getRelated(stored, Relations().pf())) {
                const auto *gep = target.getInstance<llvm::GetElementPtrInst>(from.first);
                if (!gep)
                    continue;

                if (target.are(gep->getPointerOperand(), Relations::EQ, borderVal.from)) {
                    auto h = target.getCorrespondingBorder(entryRels, borderVal.handle);
                    if (!h)
                        return false;

                    target.set(*h, Relations::EQ, gep);
                }
            }
        }
    }
    return true;
}

std::vector<bool> ValueRelationsPlugin::getValidMemory(const ValueRelations &relations,
                                                       const ValueRelations &callRels) const {
    std::vector<bool> validMemory(structure.getNumberOfAllocatedAreas());

    const auto &relationsValidAreas = relations.getValidAreas();
    const auto &callsiteValidAreas = callRels.getValidAreas();

    if (relationsValidAreas.empty() || callsiteValidAreas.empty())
        return {};

    assert(relationsValidAreas.size() == callsiteValidAreas.size());
    for (unsigned i = 0; i < structure.getNumberOfAllocatedAreas(); ++i)
        validMemory[i] = relationsValidAreas[i] || callsiteValidAreas[i];

    return validMemory;
}

std::string ValueRelationsPlugin::isValidPointer(llvm::Value *ptr, llvm::Value *size) {
    assert(ptr->getType()->isPointerTy());
    auto inst = llvm::dyn_cast<llvm::Instruction>(ptr);
    if (!inst)
        return "unknown";

    bool correct;
    uint64_t readSize;
    std::tie(correct, readSize) = getReadSize(size);
    if (!correct)
        return "unknown";
    assert(readSize > 0 && readSize < ~((uint64_t) 0));

    if (!llvm::isa<llvm::GetElementPtrInst>(inst) && !llvm::isa<llvm::LoadInst>(inst))
        return "unknown";

    const ValueRelations &relations = codeGraph.getVRLocation(inst).getSuccLocation(0)->relations;
    const std::vector<CallRelation> &callRelations = structure.getCallRelationsFor(inst);

    if (callRelations.empty())
        return isValidForGraph(relations, relations.getValidAreas(), inst, readSize) ? "true"
                                                                                     : "unknown";

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
    const llvm::Function *function = inst->getParent()->getParent();
#else
    const llvm::Function *function = inst->getFunction();
#endif

    // else we have to check that access is valid in every case
    for (const CallRelation &callRels : callRelations) {
        if (!satisfiesPreconditions(callRels, function))
            return "unknown";

        auto validMemory = getValidMemory(relations, callRels.callSite->relations);
        if (validMemory.empty())
            return "unknown";

        ValueRelations merged;
        // conflict during merge -> location is unreachable from call
        // therefore it does not make sense to validate a memory access
        if (!merge(relations, callRels, merged))
            continue;

        if (structure.hasBorderValues(function)) {
            bool result =
                    fillInBorderValues(structure.getBorderValuesFor(function), function, merged);
            if (!result)
                return "unknown";
        }

        bool result = isValidForGraph(merged, validMemory, inst, readSize);
        if (!result)
            return "unknown";
    }
    return "true";
}

extern "C" InstrPlugin *create_object(llvm::Module *module) {
    return new ValueRelationsPlugin(module);
}
