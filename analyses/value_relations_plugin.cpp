#include "value_relations_plugin.hpp"

#if LLVM_VERSION_MAJOR < 5
#include <llvm/ADT/iterator_range.h>
#endif

#include "dg/llvm/ValueRelations/GraphBuilder.h"
#include "dg/llvm/ValueRelations/RelationsAnalyzer.h"
#include "dg/llvm/ValueRelations/StructureAnalyzer.h"

ValueRelationsPlugin::ValueRelationsPlugin(llvm::Module *module)
        : InstrPlugin("ValueRelationsPlugin"), structure(*module, codeGraph) {
    using namespace dg::vr;

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
std::pair<const llvm::Value *, const llvm::Type *>
getOnlyNonzeroIndex(const llvm::GetElementPtrInst *gep) {
    const llvm::Value *firstIndex = nullptr;
    const llvm::Type *readType = gep->getSourceElementType();

#if LLVM_VERSION_MAJOR < 5
    for (const llvm::Value *index : llvm::make_range(gep->idx_begin(), gep->idx_end())) {
#else
    for (const llvm::Value *index : gep->indices()) {
#endif
        // consider only cases when nonzero index is the last
        if (firstIndex)
            return {nullptr, nullptr};

        if (const auto *constIndex = llvm::dyn_cast<llvm::ConstantInt>(index)) {
            if (constIndex->isZero()) {
                if (!readType->isArrayTy())
                    return {nullptr, nullptr};

                readType = readType->getArrayElementType();
                continue;
            };
        }

        firstIndex = index;
    }
    return {firstIndex, readType};
}

// returns the verdict of gep validity for given relations graph
std::string ValueRelationsPlugin::isValidForGraph(const dg::vr::ValueRelations &relations,
                                                  const std::vector<bool> &validMemory,
                                                  const llvm::GetElementPtrInst *gep,
                                                  uint64_t readSize) const {
    using namespace dg::vr;

    const AllocatedArea *area = nullptr;
    unsigned index = 0;

    for (const llvm::Value *equal : relations.getEqual(gep->getPointerOperand())) {
        std::tie(index, area) = structure.getAllocatedAreaFor(equal);
        if (area)
            break;
    }
    if (!area)
        return "maybe"; // memory was not allocated by ordinary means (or at all)
    if (index > validMemory.size() || !validMemory[index])
        return "maybe"; // memory is not valid at given location

    const std::vector<AllocatedSizeView> &views = area->getAllocatedSizeViews();
    if (views.empty())
        return "unknown"; // the size of allocated memory cannot be determined

    if (gep->hasAllZeroIndices())
        return readSize <= views[0].elementSize ? "true" : "maybe";
    // maybe, because can read i64 from an array of two i32

    const llvm::Value *gepIndex;
    const llvm::Type *gepType;
    std::tie(gepIndex, gepType) = getOnlyNonzeroIndex(gep);
    // if gep has more indices than one, or there are zeros after
    if (!gepIndex)
        return "unknown";

    uint64_t gepElem = AllocatedArea::getBytes(gepType);

    // DANGER just an arbitrary type
    llvm::Type *i32 = llvm::Type::getInt32Ty(views[0].elementCount->getContext());
    const llvm::Constant *zero = llvm::ConstantInt::getSigned(i32, 0);

    // check if index doesnt point before memory
    if (relations.isLesser(gepIndex, zero))
        // we know that pointed memory is alloca because allocCount is not nullptr
        return "false";
    if (!relations.isLesserEqual(zero, gepIndex))
        return "maybe";

    // check if index doesnt point after memory
    for (const AllocatedSizeView &view : views) {
        if (relations.isLesser(gepIndex, view.elementCount)) {
            if (gepElem <= view.elementSize)
                return readSize <= view.elementSize ? "true" : "maybe";
        }
        if (relations.isLesserEqual(view.elementCount, gepIndex) && gepElem >= view.elementSize)
            return "false";
    }

    return "maybe";
}

const llvm::Value *getRealArg(const dg::vr::CallRelation &callRels, const llvm::Value *formalArg) {
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
        return isValidForGraph(relations, relations.getValidAreas(), gep, readSize);

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

        std::string result = isValidForGraph(merged, validMemory, gep, readSize);
        if (result != "true")
            return result;
    }
    return "true";
}

extern "C" InstrPlugin *create_object(llvm::Module *module) {
    return new ValueRelationsPlugin(module);
}
