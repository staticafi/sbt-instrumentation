#include "value_relations_plugin.hpp"

#if LLVM_VERSION_MAJOR < 5
#include <llvm/ADT/iterator_range.h>
#endif

#include "dg/llvm/ValueRelations/GraphBuilder.h"
#include "dg/llvm/ValueRelations/RelationsAnalyzer.h"
#include "dg/llvm/ValueRelations/StructureAnalyzer.h"

using namespace dg::vr;
using Borders = ValueRelationsPlugin::Borders;

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
    if (!alloca || !isValidForGraph(relations, validMemory, alloca, readSize)) {
        const VRLocation *gepLoc = codeGraph.getVRLocation(gep).getSuccLocation(0);
        const llvm::Value *index = nullptr;
        for (auto handleRel : relations.getRelated(gep, Relations().slt())) {
            auto gep = relations.getInstance<llvm::GetElementPtrInst>(handleRel.first);
            if (!gep)
                continue;
            auto possible = relations.getInstance<llvm::AllocaInst>(
                    *relations.getHandle(gep->getPointerOperand()));
            if (possible) {
                alloca = possible;
                index = *gep->indices().begin();
            }
        }
        if (!alloca)
            return false;
        if (!gepLoc->join)
            return false;
        const llvm::Type *gepType = gep->getType()->getPointerElementType();
        uint64_t gepElemSize = AllocatedArea::getBytes(gepType);

        const auto &views = getAllocatedViews(relations, validMemory, alloca);

        auto decisivePairs = getDecisive(*gepLoc);

        for (auto pair : decisivePairs) {
            const llvm::Value *from;
            const llvm::Value *decisive;
            std::tie(from, decisive) = pair;

            if (!relations.contains(decisive) && !relations.has(from, Relations::PT))
                continue;

            auto &decisiveH = relations.contains(decisive) ? *relations.getHandle(decisive)
                                                           : relations.getPointedTo(from);

            for (auto handleRel : relations.getRelated(decisiveH, Relations().sle())) {
                if (handleRel.second.has(Relations::EQ))
                    continue;

                auto &relatedH = handleRel.first;

                auto zero =
                        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(gep->getContext()), 0);
                if (relations.are(zero, Relations::SLT, decisiveH)) {
                    for (auto view : views) {
                        if (relations.are(relatedH, Relations::SLE, view.elementCount) &&
                            relations.are(index, Relations::SLE, view.elementCount)) {
                            if (gepElemSize <= view.elementSize) {
                                if (readSize <= view.elementSize) {
                                    // std::cerr << "    smaller than same-increment value\n";
                                    return true;
                                }
                                return false;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

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
            if (gepElemSize <= view.elementSize) {
                if (readSize <= view.elementSize) {
                    return true;
                }
                return false;
            }
        }
    }
    return false;
}

bool ValueRelationsPlugin::storedToInLoop(const VRLocation &join, const llvm::Value *from) const {
    for (const auto *val : structure.getInloopValues(join)) {
        const auto *store = llvm::dyn_cast<llvm::LoadInst>(val);
        if (store && store->getPointerOperand() == from)
            return true;
    }
    return false;
}

std::pair<const ValueRelations *, ValueRelations::HandlePtr>
ValueRelationsPlugin::getInitial(const ValueRelations &relations,
                                 const ValueRelations::Handle &h) const {
    std::pair<const ValueRelations *, ValueRelations::HandlePtr> result{nullptr, nullptr};

    for (const auto *val : relations.getEqual(h)) {
        if (const auto *inst = llvm::dyn_cast<llvm::Instruction>(val)) {
            auto thisH = getInitial(relations, inst);
            if (!thisH.second)
                continue;
            if (!result.second || result.second == thisH.second)
                result = thisH;
            else
                assert(0);
        }
    }
    return result;
}

size_t countIntoLoopEdges(const VRLocation &loc) {
    assert(loc.join);
    size_t count = 0;
    for (size_t i = 0; i < loc.succsSize(); ++i) {
        const auto *succEdge = loc.getSuccEdge(i);
        if (succEdge->target->join == loc.join)
            ++count;
    }
    return count;
}

std::pair<const ValueRelations *, ValueRelations::HandlePtr>
ValueRelationsPlugin::getInitial(const ValueRelations &relations,
                                 const llvm::Instruction *val) const {
    const VRLocation *valLoc = codeGraph.getVRLocation(val).getSuccLocation(0);
    assert(valLoc->join);

    const VRLocation &predLoc = valLoc->join->getTreePredecessor();
    const ValueRelations &predGraph = predLoc.relations;

    const auto &valFroms = RelationsAnalyzer::getFroms(relations, val);
    if (valFroms.empty())
        return {nullptr, nullptr};
    if (predGraph.has(valFroms.back(), Relations::PT))
        return {&predGraph, RelationsAnalyzer::getHandleFromFroms(predGraph, valFroms)};

    for (auto it = ++codeGraph.dfs_begin(*val->getFunction(), *valLoc->join);
         it != codeGraph.dfs_end(); ++it) {
        if (it->join != valLoc->join) {
            it.skipSuccessors();
            continue;
        }
        if (countIntoLoopEdges(*it) > 1)
            break;
        if (it->relations.has(valFroms.back(), Relations::PT))
            return {&it->relations, RelationsAnalyzer::getHandleFromFroms(it->relations, valFroms)};
    }
    return {nullptr, nullptr};
}

ValueRelations::Handle ValueRelationsPlugin::getInitialInThis(const ValueRelations &relations,
                                                              const llvm::Instruction *val) const {
    auto pair = getInitial(relations, val);
    assert(pair.first && pair.second);

    auto *result =
            RelationsAnalyzer::getCorrespondingByContent(relations, *pair.first, *pair.second);
    assert(result);
    return *result;
}

std::vector<std::pair<const llvm::Value *, const llvm::Value *>>
ValueRelationsPlugin::getDecisive(const VRLocation &loadLoc) const {
    std::vector<std::pair<const llvm::Value *, const llvm::Value *>> result;
    for (auto *leaveEdge : loadLoc.join->loopEnds) {
        assert(leaveEdge->op->isAssumeBool());
        for (const llvm::ICmpInst *icmp :
             structure.getRelevantConditions(static_cast<VRAssumeBool *>(leaveEdge->op.get()))) {
            const ValueRelations &forkRels = codeGraph.getVRLocation(icmp).relations;

            for (const auto &op : icmp->operands()) {
                if (const auto *inst = llvm::dyn_cast<llvm::Instruction>(&op)) {
                    auto load = forkRels.getInstance<llvm::LoadInst>(inst);
                    const llvm::Value *decisive = nullptr;
                    if (load && !forkRels.getInstance<llvm::AllocaInst>(load->getPointerOperand()))
                        decisive = load->getPointerOperand();
                    else
                        decisive = inst;
                    assert(decisive);

                    const llvm::Value *from = nullptr;
                    if (auto load = llvm::dyn_cast<llvm::LoadInst>(decisive))
                        from = load->getPointerOperand();
                    else {
                        auto related = forkRels.getRelated(decisive, Relations().pf());
                        if (related.size() != 1)
                            continue;
                        from = *forkRels.getEqual(related.begin()->first).begin();
                    }
                    assert(from);
                    if (storedToInLoop(*loadLoc.join, from))
                        result.emplace_back(from, decisive);
                }
            }
        }
    }
    return result;
}

bool rangeLimitedBy(const ValueRelations &relations, const llvm::Value *lower,
                    const ValueRelations::Handle &h, const llvm::Value *upper) {
    return (relations.are(lower, Relations::SLT, h) && relations.are(h, Relations::SLE, upper)) ||
           (relations.are(lower, Relations::SLE, h) && relations.are(h, Relations::SLT, upper));
}

bool rangeLimitedByAny(const ValueRelations &relations, const llvm::Value *lower,
                       ValueRelations::Handle h, const VectorSet<const llvm::Value *> &uppers) {
    for (const auto *upper : uppers) {
        if (rangeLimitedBy(relations, lower, h, upper))
            return true;
    }
    return false;
}

bool ValueRelationsPlugin::rangeLimitedBy(const ValueRelations &relations,
                                          const llvm::Instruction *inst, const llvm::Value *from,
                                          const llvm::Value *lower, ValueRelations::Handle h,
                                          const llvm::Value *upper) const {
    auto &initLoadH = getInitialInThis(relations, inst);
    const auto &loadLoc = *codeGraph.getVRLocation(inst).getSuccLocation(0);
    if (!loadLoc.join)
        return false;
    if (relations.getInstance<llvm::AllocaInst>(initLoadH))
        return ::rangeLimitedBy(relations, lower, h, upper);

    auto initNPair = getInitial(relations, h);
    assert(initNPair.first && initNPair.second);
    const ValueRelations &predRels = *initNPair.first;
    const auto &initNH = *initNPair.second;

    auto eqvals = predRels.getEqual(initNH);
    if (eqvals.empty()) {
        for (const auto *inloopVal : structure.getInloopValues(*loadLoc.join)) {
            if (inloopVal == inst)
                break;

            const VRLocation &inloopLoc = *codeGraph.getVRLocation(inloopVal).getSuccLocation(0);
            if (!inloopLoc.relations.has(from, Relations::PT))
                break;

            const auto &eqInloops =
                    inloopLoc.relations.getEqual(inloopLoc.relations.getPointedTo(from));
            if (!eqInloops.empty()) {
                eqvals = eqInloops;
                break;
            }
        }
    }
    assert(!eqvals.empty());
    // verify range in this loop
    if (!rangeLimitedByAny(relations, lower, h, eqvals))
        return false;

    // verify previous changes
    for (const auto *initVal : relations.getEqual(initLoadH)) {
        const auto *initInst = llvm::cast<llvm::Instruction>(initVal);
        const VRLocation &initLoc = *codeGraph.getVRLocation(initInst).getSuccLocation(0);
        const ValueRelations &initRels = initLoc.relations;

        for (const auto *splitN : predRels.getEqual(initNH)) {
            for (const auto &fromPair : predRels.getRelated(splitN, Relations().pf())) {
                for (const auto *initFrom : predRels.getEqual(fromPair.first)) {
                    if (!initRels.has(initFrom, Relations::PT))
                        continue;

                    const auto &otherRelatedH = initRels.getPointedTo(initFrom);
                    if (rangeLimitedByAny(initRels, splitN, otherRelatedH,
                                          relations.getEqual(upper)))
                        return true;
                }
            }
        }

        if (predRels.getEqual(initNH).empty()) {
            if (!initRels.has(from, Relations::PT) ||
                !rangeLimitedByAny(initRels, lower, initRels.getPointedTo(from),
                                   relations.getEqual(upper)))
                continue;

            for (auto it = codeGraph.backward_dfs_begin(*inst->getFunction(),
                                                        loadLoc.join->getTreePredecessor());
                 it != codeGraph.backward_dfs_end(); ++it) {
                if (!it->relations.has(from, Relations::PT) ||
                    !it->relations.getEqual(it->relations.getPointedTo(from)).empty())
                    break;
                if (it->id == initLoc.id)
                    return true;
            }
        }
    }
    return false;
}

bool ValueRelationsPlugin::isValidForGraph(const ValueRelations &relations,
                                           const std::vector<bool> &validMemory,
                                           const llvm::LoadInst *load, uint64_t readSize) const {
    const auto *alloca = RelationsAnalyzer::getOrigin(relations, load);
    if (!alloca || !isValidForGraph(relations, validMemory, alloca, readSize))
        return false;

    auto *loadLoc = codeGraph.getVRLocation(load).getSuccLocation(0);
    if (!loadLoc->join) {
        for (auto handleRel : relations.getRelated(load, Relations().sle())) {
            if (const auto *gep = relations.getInstance<llvm::GetElementPtrInst>(handleRel.first)) {
                if (relations.are(gep->getPointerOperand(), Relations::EQ, alloca) &&
                    isValidForGraph(relations, validMemory, gep, readSize)) {
                    return true;
                }
            }
        }
        return false;
    }

    const auto &init = getInitialInThis(relations, load);

    const llvm::Type *loadType = load->getType();
    uint64_t loadSize = AllocatedArea::getBytes(loadType);

    const auto &views = getAllocatedViews(relations, validMemory, alloca);

    auto decisivePairs = getDecisive(*loadLoc);

    for (auto pair : decisivePairs) {
        const llvm::Value *from;
        const llvm::Value *decisive;
        std::tie(from, decisive) = pair;

        if (!relations.contains(decisive) && !relations.has(from, Relations::PT))
            continue;

        auto &decisiveH = relations.contains(decisive) ? *relations.getHandle(decisive)
                                                       : relations.getPointedTo(from);

        for (auto handleRel : relations.getRelated(decisiveH, Relations().sle())) {
            if (handleRel.second.has(Relations::EQ))
                continue;

            auto &relatedH = handleRel.first;

            if (auto gep = relations.getInstance<llvm::GetElementPtrInst>(relatedH)) {
                if (!isValidForGraph(relations, validMemory, gep, readSize))
                    continue;
                auto index = getRelevantIndex(gep);
                for (auto view : views) {
                    if (relations.getInstance<llvm::AllocaInst>(init)) {
                        if (relations.are(index, Relations::SLE, view.elementCount)) {
                            return readSize <= view.elementSize;
                        }
                    } else {
                        // TODO handle cstrcat
                    }
                }
            }
        }

        const auto *zero =
                llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(load->getContext()), 0);

        for (auto view : views) {
            if (rangeLimitedBy(relations, load, from, zero, decisiveH, view.elementCount)) {
                if (loadSize <= view.elementSize) {
                    if (readSize <= view.elementSize)
                        return true;
                    return false;
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

bool ValueRelationsPlugin::fillInBorderVals(const llvm::Function *function,
                                            ValueRelations &relations) const {
    if (!structure.hasBorderValues(function))
        return true;

    for (const auto &borderVal : structure.getBorderValuesFor(function)) {
        auto *idH = relations.getBorderH(borderVal.id);
        if (!idH)
            continue;
        for (auto from : relations.getRelated(borderVal.stored, Relations().pf())) {
            const auto *gep = relations.getInstance<llvm::GetElementPtrInst>(from.first);
            if (!gep || !relations.are(gep->getPointerOperand(), Relations::EQ, borderVal.from))
                continue;

            for (const auto *val : relations.getEqual(gep->getPointerOperand())) {
                if (const auto *arg = llvm::dyn_cast<llvm::Argument>(val)) {
                    // each passed pointer must be derived
                    // from different memory
                    if (arg != borderVal.from)
                        return false;
                }
            }

            relations.set(borderVal.id, Relations::EQ, gep);
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

        bool result = fillInBorderVals(function, merged);
        if (!result)
            return "unknown";


        result = isValidForGraph(merged, validMemory, inst, readSize);
        if (!result)
            return "unknown";
    }
    return "true";
}

extern "C" InstrPlugin *create_object(llvm::Module *module) {
    return new ValueRelationsPlugin(module);
}
