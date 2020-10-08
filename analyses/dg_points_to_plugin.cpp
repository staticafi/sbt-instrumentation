#include "dg_points_to_plugin.hpp"

#include "dg/SCC.h"

#include <set>
#include <string>
#include <iostream>

using dg::pta::PSNode;
using dg::pta::Pointer;
using dg::pta::PSNodeAlloc;

std::string PointsToPlugin::pointsToStack(llvm::Value* a) {
    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        // llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be null
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        if (ptr.isUnknown())
            return "false";

        PSNodeAlloc *target = PSNodeAlloc::get(ptr.target);
        if (!target || target->isGlobal() || target->isHeap())
            return "false";
    }

    // a points to stack
    return "true";
}

std::string PointsToPlugin::notMinMemoryBlock(llvm::Value* p, llvm::Value* a) {
    // check is a is getelementptr
    if (llvm::GetElementPtrInst *GI = llvm::dyn_cast<llvm::GetElementPtrInst>(p)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsToNode(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            return "true";
        }

        uint64_t min_offset = *((*psnode->pointsTo.begin()).offset);
        uint64_t min_space = (*psnode->pointsTo.begin()).target->getSize()
                             - *((*psnode->pointsTo.begin()).offset);
        bool pointsTo = false;

        uint64_t act_offset = 0;
        uint64_t act_space = 0;

        for (const auto& ptr : psnode->pointsTo) {
            llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();

            if (llvmVal == a) {
                act_offset = *(ptr.offset);
                act_space = (ptr.target->getSize() - *(ptr.offset));
                pointsTo = true;
            }

            if ((*(ptr.offset) < min_offset))
                min_offset = *(ptr.offset);

            if ((ptr.target->getSize() - *(ptr.offset))
                < min_space) {
                min_space = ptr.target->getSize() - *(ptr.offset);
            }
        }

        if (!pointsTo) {
            return "unknown";
        }

        if (act_offset <= min_offset && act_space <= min_space) {
            return "false";
        } else {
            return "true";
        }
    }
    else {
        return "unknown";
    }
}

std::string PointsToPlugin::pointsToGlobal(llvm::Value* a) {
    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        // llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be null
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        if (ptr.isUnknown())
            return "false";

        PSNodeAlloc *target = PSNodeAlloc::get(ptr.target);
        if (!target || !target->isGlobal())
            return "false";
    }

    // a points to a global variable
    return "true";
}

std::string PointsToPlugin::pointsToHeap(llvm::Value* a) {
    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        // llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be null
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        if (ptr.isUnknown())
            return "false";

        PSNodeAlloc *target = PSNodeAlloc::get(ptr.target);
        if (!target || !target->isHeap())
            return "false";
    }

    // a points to heap
    return "true";
}

std::string PointsToPlugin::isInvalid(llvm::Value* a) {
    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        // llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be null
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        if (!ptr.isNull() && !ptr.isInvalidated())
            return "false";
    }

    // a is null or invalidated
    return "true";
}

std::string PointsToPlugin::isNull(llvm::Value* a) {
    if (!a->getType()->isPointerTy())
        // null must be a pointer
        return "false";

    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        // llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be null
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        // unknown pointer can be null too
        if (ptr.isNull() || ptr.isUnknown())
            return "true";
    }

    // a can not be null
    return "false";
}

std::string PointsToPlugin::hasKnownSizes(llvm::Value* a) {
    // check is a is getelementptr
    if (const llvm::GetElementPtrInst *GI
            = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsToNode(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            // we know nothing about the allocated size
            return "false";
        }

        // check that all the objects has known sizes and
        // offsets
        for (const auto& ptr : psnode->pointsTo) {
            // ptset cannot contain null, unknown or invalidated pointer
            if(ptr.isNull() || ptr.isUnknown() || ptr.isInvalidated())
                return "false";

            // the sizes and offsets need to be known
            if (ptr.offset.isUnknown() ||
                ptr.target->getSize() == 0) {
                return "false";
            }
        }

        return "true";
    }

    return "false";
}

std::string PointsToPlugin::hasKnownSize(llvm::Value* a) {
    // check is a is getelementptr
    if (const llvm::GetElementPtrInst *GI
            = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsToNode(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            // we know nothing about the allocated size
            return "false";
        }

        const auto& first = *(psnode->pointsTo.begin());
        // we must have concrete offset and concrete size
        if (first.isNull() || first.isUnknown() || first.isInvalidated())
            return "false";
        if (first.offset.isUnknown())
            return "false";
        if (first.target->getSize() == 0)
            return "false";

        // check that all the objects has the same size and the
        // same offset (and are know, but that follows from
        // the non-zero size
        for (const auto& ptr : psnode->pointsTo) {
            // the sizes and offsets need to be the same
            if (*(ptr.offset) != *(first.offset) ||
                ptr.target->getSize() != first.target->getSize()) {
                return "false";
            }
            // ptset cannot contain null, unknown or invalidated pointer
            assert(!(ptr.isNull() || ptr.isUnknown() || ptr.isInvalidated()));
        }

        return "true";
    }

    return "false";
}

PointerInfo PointsToPlugin::getPointerInfo(llvm::Value* a) {
    // check is a is getelementptr
    if (llvm::GetElementPtrInst *GI = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsToNode(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            return PointerInfo();
        }

        const auto& first = *(psnode->pointsTo.begin());
        return PointerInfo(GI->getPointerOperand(), *(first.offset),
                           first.target->getSize());

    }
    else {
        return PointerInfo();
    }
}

PointerInfo PointsToPlugin::getPInfoMinMax(llvm::Value* a,
                                            std::vector<llvm::Value*>& ptset)
{
    // check is a is getelementptr
    if (llvm::GetElementPtrInst *GI = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsToNode(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            return PointerInfo();
        }

        uint64_t min_offset = *((*psnode->pointsTo.begin()).offset);
        uint64_t min_space = (*psnode->pointsTo.begin()).target->getSize()
                             - *((*psnode->pointsTo.begin()).offset);
        uint64_t max_offset = *((*psnode->pointsTo.begin()).offset);
        uint64_t max_space = (*psnode->pointsTo.begin()).target->getSize()
                             - *((*psnode->pointsTo.begin()).offset);

        for (const auto& ptr : psnode->pointsTo) {
            if ((*(ptr.offset) < min_offset))
                min_offset = *(ptr.offset);
            if ((ptr.target->getSize() - *(ptr.offset))
                < min_space) {
                min_space = ptr.target->getSize() - *(ptr.offset);
            }
            if ((*(ptr.offset) > max_offset))
                max_offset = *(ptr.offset);
            if ((ptr.target->getSize() - *(ptr.offset))
                > max_space) {
                max_space = ptr.target->getSize() - *(ptr.offset);
            }
        }

        for (const auto& ptr : psnode->pointsTo) {
            if ((*(ptr.offset) > min_offset) &&
                (ptr.target->getSize() - *(ptr.offset)) > min_space) {
                llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
                if (llvmVal)
                    ptset.push_back(llvmVal);
            }
        }

        return PointerInfo(GI->getPointerOperand(), min_offset,
                           min_space, max_offset, max_space);

    }
    else {
        return PointerInfo();
    }
}


PointerInfo PointsToPlugin::getPInfoMin(llvm::Value* a) {
    // check is a is getelementptr
    if (llvm::GetElementPtrInst *GI = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsToNode(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            return PointerInfo();
        }

        uint64_t min_offset = *((*psnode->pointsTo.begin()).offset);
        uint64_t min_space = (*psnode->pointsTo.begin()).target->getSize()
                             - *((*psnode->pointsTo.begin()).offset);
        for (const auto& ptr : psnode->pointsTo) {
            if ((*(ptr.offset) < min_offset))
                min_offset = *(ptr.offset);
            if ((ptr.target->getSize() - *(ptr.offset))
                < min_space) {
                min_space = ptr.target->getSize() - *(ptr.offset);
            }
        }
        return PointerInfo(GI->getPointerOperand(), min_offset,
                           min_space);

    }
    else {
        return PointerInfo();
    }
}

std::string PointsToPlugin::isValidPointer(llvm::Value* a, llvm::Value *len) {
    if (!a->getType()->isPointerTy())
        // null must be a pointer
        return "false";

    uint64_t size = 0;
    if (llvm::ConstantInt *C = llvm::dyn_cast<llvm::ConstantInt>(len)) {
        size = C->getLimitedValue();
        // if the size cannot be expressed as an uint64_t,
        // say we do not know
        if (size == ~((uint64_t) 0))
            return "maybe";

        // the offset is concrete number, fall-through
    } else {
        // we do not know anything with variable length
        return "maybe";
    }

    assert(size > 0 && size < ~((uint64_t) 0));

    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        //llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be invalid
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        // unknown pointer and null are invalid
        if (ptr.isNull() || ptr.isUnknown())
            return "false";

        // the memory this pointer points-to was invalidated
        if (ptr.isInvalidated())
            return "false";

        // if the offset is unknown, than the pointer
        // may point after the end of allocated memory
        if (ptr.offset.isUnknown())
            return "maybe";

        // if the offset + size > the size of allocated memory,
        // then this can be invalid operation. Check it so that
        // we won't overflow, that is, first ensure that psnode->size <= size
        // and than use this fact and equality with ptr.offset + size > psnode->size)
        if (size > ptr.target->getSize()
                || *ptr.offset > ptr.target->getSize() - size) {
            return "false";
        }

        if (llvm::Instruction *I = llvm::dyn_cast<llvm::Instruction>(a)) {
            if(llvm::Instruction *Iptr = llvm::dyn_cast<llvm::Instruction>(ptr.target->getUserData<llvm::Value>())) {
                llvm::Function *F = I->getParent()->getParent();
                if(Iptr->getParent()->getParent() != F || isRecursive(F))
                    return "false";
            } else {
                //llvm::errs() << "In bound pointer for non-allocated memory: " << *a << "\n";
            }
        } else {
            //llvm::errs() << "In bound pointer for non-allocated memory: " << *a << "\n";
        }
    }

    // this pointer is valid
    return "true";
}

std::string PointsToPlugin::pointsTo(llvm::Value* a, llvm::Value* b) {
    if(PTA) {
        PSNode *psnode = PTA->getPointsToNode(a);
        if (!psnode) return "maybe";
        for (const auto& ptr : psnode->pointsTo) {
            llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
            if(llvmVal == b) return "true";
        }
    } else {
        return "true"; // a may point to b
    }

    return "false";
}

std::string PointsToPlugin::pointsToSetsOverlap(llvm::Value* a, llvm::Value* b) {
    assert(PTA);
    PSNode *nodeA = PTA->getPointsToNode(a);
    PSNode *nodeB = PTA->getPointsToNode(a);
    if (!nodeA || !nodeB)
        return "maybe";
    if (nodeA->pointsTo.empty() || nodeB->pointsTo.empty())
        return "maybe";

    for (const auto& ptrA : nodeA->pointsTo) {
        if (ptrA.isUnknown())
            return "maybe";

        for (const auto& ptrB : nodeB->pointsTo) {
            if (ptrB.isUnknown())
                return "maybe";

            if ((ptrA.target == ptrB.target) &&
                ((ptrA.offset.isUnknown() || ptrB.offset.isUnknown()) ||
                 (ptrA.offset == ptrB.offset)))
                return "true";
        }
    }

    return "false";
}


/**
 * Gets points to set of llvm value a.
 * @return true if ptset contains unknown
**/
bool PointsToPlugin::getPointsTo(llvm::Value* a, std::vector<llvm::Value*>& ptset) {
    bool containsUnknown = false;
    if(PTA) {
        PSNode *psnode = PTA->getPointsToNode(a);
        if (!psnode) return false;
        for (const auto& ptr : psnode->pointsTo) {
            if (ptr.isUnknown()) {
                containsUnknown = true;
                continue;
            }

            if (ptr.isNull() || ptr.isInvalidated())
                continue;

            llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
            if(llvmVal)
                ptset.push_back(llvmVal);
        }
    }

    return containsUnknown;
}

static std::set<PSNode *>
gatherReachableObjects(dg::pta::PointerAnalysisFSInv::MemoryMapT *mm) {
    // FIXME: use proper queue...
    std::set<PSNode *> reachable;

    std::set<PSNode *> queue;
    for (auto& it : *mm) {
        // skip allocations not in main and also heap objects
        // (which cannot stand on its own in the code
        // -- their address must be stored somewhere)
        PSNodeAlloc *target = PSNodeAlloc::cast(it.first);
        if (target->isHeap())
            continue;
        if (auto val = target->getUserData<llvm::Value>()) {
            if (auto A = llvm::dyn_cast<llvm::AllocaInst>(val)) {
                if (!A->getParent()->getParent()->getName().equals("main")) {
                    continue;
                }
            }
        }

        queue.insert(it.first);
        reachable.insert(it.first);
    }

    while (!queue.empty()) {
        PSNode *obj = *queue.begin();
        queue.erase(queue.begin());

        auto it = mm->find(obj);
        if (it == mm->end())
            continue;

        auto *memobj = it->second.get();
        for (auto& ptit : *memobj) {
            for (const auto& ptr : ptit.second) {
                if (reachable.insert(ptr.target).second) {
                    queue.insert(ptr.target);
                }
            }
        }
    }
    return reachable;
}

///
// Return true if this store may cause loosing the last
// reference to some heap allocated memory
std::string PointsToPlugin::storeMayLeak(llvm::Value *v) {
    auto SI = llvm::dyn_cast<llvm::StoreInst>(v);
    if (!SI) {
        assert(false && "Called not on store");
        return "false";
    }

    PSNode *snode = PTA->getPointsToNode(SI);
    if (!snode || snode->pointsTo.hasUnknown()) {
        return "maybe";
    }

    auto mm = snode->getData<dg::pta::PointerAnalysisFSInv::MemoryMapT>();
    if (!mm) {
        return "maybe";
    }

    auto sobjects = gatherReachableObjects(mm);

    for (auto *pred : snode->predecessors()) {
        auto pm = pred->getData<dg::pta::PointerAnalysisFSInv::MemoryMapT>();
        if (!pm) {
            return "maybe";
        }

        auto pobjects = gatherReachableObjects(pm);
        std::set<PSNode *> diff;
        std::set_difference(pobjects.begin(), pobjects.end(),
                            sobjects.begin(), sobjects.end(),
                            std::inserter(diff, diff.end()));
        for (auto *obj : diff) {
            auto alloc = PSNodeAlloc::get(obj);
            if (alloc && alloc->isHeap()) {
                //llvm::errs() << "Leaking store:" << *SI << "\n";
                //llvm::errs() << "leaking: "
                //             << *alloc->getUserData<llvm::Value>() << "\n";
                possiblyLeaked.insert(alloc);
                return "true";
            }
        }
    }

    return "false";
}


void PointsToPlugin::gatherPossiblyLeaked(llvm::ReturnInst *RI) {
    PSNode *ret = PTA->getPointsToNode(RI);
    if (!ret) {
        allMayBeLeaked = true;
        return;
    }

    auto mm = ret->getData<dg::pta::PointerAnalysisFSInv::MemoryMapT>();
    if (!mm) {
        allMayBeLeaked = true;
        return;
    }

    // Find all heap allocations in the memory map.
    // If the memory was freed for sure, it was replaced with invalidated,
    // therefore if it is in the map, it may not have been freed
    for (auto& it : *mm) {
        for (auto& ptrs : *(it.second.get())) {
            for (const auto &ptr : ptrs.second) {
                const auto alloc = PSNodeAlloc::get(ptr.target);
                if (alloc && alloc->isHeap()) {
                    possiblyLeaked.insert(alloc);
                }
            }
        }
    }
}

void PointsToPlugin::gatherPossiblyLeaked(llvm::Module *M) {
    auto Main = M->getFunction("main");
    if (!Main) {
        allMayBeLeaked = true;
        return;
    }

    for (auto& B : *Main) {
        auto term = B.getTerminator();
        if (auto RI = llvm::dyn_cast<llvm::ReturnInst>(term)) {
            gatherPossiblyLeaked(RI);
            if (allMayBeLeaked) {
                possiblyLeaked.clear();
                return;
            }
        }
    }
}

static inline bool pointsToUnknownOrNull(const PSNode *node) {
    for (const auto& ptr : node->pointsTo) {
        if (!ptr.isNull() && !ptr.isUnknown())
            return false;
    }
    return true;
}

std::string PointsToPlugin::mayBeLeaked(llvm::Value* a) {
    if (llvm::isa<llvm::ConstantInt>(a)) {
        return "false";
    }

    // assume that undefined functions
    // do not return a pointer to leakable memory
    if (auto *C = llvm::dyn_cast<llvm::CallInst>(a)) {
#if LLVM_VERSION_MAJOR >= 8
        auto *V = C->getCalledOperand();
#else
        auto *V = C->getCalledValue();
#endif
        auto *F = llvm::dyn_cast<llvm::Function>(V->stripPointerCasts());
        if (F && F->isDeclaration()) {
            // of course, skip the calls that allocate memory...
            const auto& name = F->getName();
            if (!name.equals("malloc") &&
                !name.equals("calloc") &&
                !name.equals("realloc") &&
                !name.startswith("__VERIFIER_malloc") &&
                !name.startswith("__VERIFIER_calloc")) {
                return "false";
            }
        }
    }

    if (allMayBeLeaked) {
        return "true";
    }

    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        return "true";
    }

    // a number, not a pointer
    if (a->getType()->isIntegerTy() && pointsToUnknownOrNull(psnode)) {
        return "false";
    }

    for (const auto& ptr : psnode->pointsTo) {
        if (ptr.isUnknown()) {
            return "true";
        }
        if (possiblyLeaked.count(ptr.target) > 0) {
            return "true";
        }
    }

    return "false";
}

std::string PointsToPlugin::mayBeLeakedOrFreed(llvm::Value* a) {
    if (allMayBeLeaked) {
        return "true";
    }

    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty())
        return "true";

    for (const auto& ptr : psnode->pointsTo) {
        if (ptr.isUnknown() || ptr.isInvalidated())
            return "true";
        if (possiblyLeaked.count(ptr.target) > 0)
            return "true";
    }

    return "false";
}

std::string PointsToPlugin::safeForFree(llvm::Value* a) {
    assert(PTA);
    PSNode *psnode = PTA->getPointsToNode(a);
    if (!psnode || psnode->pointsTo.empty()) {
        // llvm::errs() << "No points-to for " << *a << "\n";
        // we know nothing, it may be null
        return "maybe";
    }

    for (const auto& ptr : psnode->pointsTo) {
        if (ptr.offset != 0)
            return "false";
        if (ptr.isUnknown() || ptr.isInvalidated())
            return "false";

        if (auto alloc = PSNodeAlloc::get(ptr.target)) {
            if (!alloc->isHeap())
                return "false";
        } else {
            return "false";
        }
    }

    // points only to beginnings of known memory or to nullptr
    return "true";
}

void PointsToPlugin::computeRecursiveFuns(llvm::Module *module) {
    auto& cg = PTA->getPTA()->getPG()->getCallGraph();
    if (cg.empty()) // only main function without any calls
        return;

    auto *entrynd = PTA->getPointsToNode(module->getFunction("main"));
    assert(entrynd && "PTA has no node for entry to main function");
    auto *entry = cg.get(entrynd);
    assert(entry && "CallGraph has no node for the main function");

    auto SCCs = dg::SCC<dg::GenericCallGraph<dg::PSNode *>::FuncNode>().compute(entry);
    for (auto& component : SCCs) {
        if (component.size() > 1 ||
            (component.size() == 1 && component[0]->calls(component[0]))) {

            for (auto *funcnd : component) {
                auto *fun = funcnd->getValue()->getUserData<llvm::Function>();
                //llvm::errs() << "Recursive fun: " << fun->getName() << "\n";
                recursiveFuns.insert(fun);
            }
        }
    }

}

bool PointsToPlugin::isRecursive(const llvm::Function *F) {
    return recursiveFuns.count(F) > 0;
}

bool PointsToPlugin::isReachableFun(const llvm::Function *F) const {
    // if PTA built the node for F, it is reachable
    return PTA->getPointsToNode(F) != nullptr;
}


static const std::string supportedQueries[] = {
    "isValidPointer",
    "pointsTo",
    "hasKnownSize",
    "hasKnownSizes",
    "getPointerInfo",
    "isNull",
    "pointsToHeap",
    "pointsToGlobal",
    "pointsToStack",
    "isInvalid",
    "mayBeLeaked",
    "mayBeLeakedOrFreed",
    "safeForFree",
    "storeMayLeak",
    "pointsToSetsOverlap",
};

bool PointsToPlugin::supports(const std::string& query) {
    for (unsigned idx = 0; idx < sizeof(supportedQueries)/sizeof(*supportedQueries); ++idx) {
        if (query == supportedQueries[idx])
            return true;
    }

    return false;
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
        return new PointsToPlugin(module);
}
