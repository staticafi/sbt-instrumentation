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

const llvm::Value *getRealArg(const CallRelation &callRels, const llvm::Value *formalArg) {
    const llvm::Value *arg = nullptr;
    for (const auto &pair : callRels.equalPairs) {
        if (formalArg == pair.first)
            arg = pair.second;
    }
    assert(arg);
    return arg;
}

std::string ValueRelationsPlugin::isValidPointer(llvm::Value *ptr, llvm::Value *size) {
    using namespace dg::vr;

    // ptr is not a pointer
    if (!ptr->getType()->isPointerTy())
        return "false";

    uint64_t readSize = 0;
    if (auto *constant = llvm::dyn_cast<llvm::ConstantInt>(size)) {
        readSize = constant->getLimitedValue();

        // size cannot be expressed as uint64_t
        if (readSize == ~((uint64_t) 0))
            return "maybe";
    } else {
        // size is not constant int
        return "maybe";
    }

    assert(readSize > 0 && readSize < ~((uint64_t) 0));

    const auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr);
    if (!gep)
        return "unknown";

    const ValueRelations &relations = codeGraph.getVRLocation(gep).relations;
    const std::vector<CallRelation> &callRelations = structure.getCallRelationsFor(gep);

    if (callRelations.empty())
        return isValidForGraph(relations, relations.getValidAreas(), gep, readSize) ? "true"
                                                                                    : "unknown";

    const llvm::Function *function = gep->getFunction();

    // else we have to check that access is valid in every case
    for (const CallRelation &callRelation : callRelations) {
        if (structure.hasPreconditions(function)) {
            for (const auto &prec : structure.getPreconditionsFor(function)) {
                assert(callRelation.callSite);
                const llvm::Value *arg = getRealArg(callRelation, prec.arg);
                if (!callRelation.callSite->relations.are(arg, prec.rel, prec.val))
                    return "unknown";
            }
        }
        ValueRelations merged;
        merged.merge(relations);

        bool hasConflict = false;
        for (const auto &equalPair : callRelation.equalPairs) {
            if (merged.hasConflictingRelation(equalPair.first, equalPair.second,
                                              Relations::Type::EQ)) {
                hasConflict = true;
                break; // this vrlocation is unreachable with given parameters
            }
            merged.setEqual(equalPair.first, equalPair.second);
        }

        const ValueRelations &callSiteRelations = callRelation.callSite->relations;

        // this vrlocation is unreachable with relations from given call relation
        hasConflict = hasConflict || !merged.merge(callSiteRelations);

        // since location is unreachable, it does not make sence to qualify the memory access
        if (hasConflict)
            continue;

        std::vector<bool> validMemory(structure.getNumberOfAllocatedAreas());

        if (relations.getValidAreas().empty() || callSiteRelations.getValidAreas().empty())
            return "unknown";
        for (unsigned i = 0; i < structure.getNumberOfAllocatedAreas(); ++i)
            validMemory[i] = relations.getValidAreas()[i] || callSiteRelations.getValidAreas()[i];

        std::string result =
                isValidForGraph(merged, validMemory, gep, readSize) ? "true" : "unknown";
        if (result != "true")
            return result;
    }
    return "true";
}

extern "C" InstrPlugin *create_object(llvm::Module *module) {
    return new ValueRelationsPlugin(module);
}
