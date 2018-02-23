#include "points_to_plugin.hpp"
#include <set>

using dg::analysis::pta::PSNode;

std::string PointsToPlugin::isNull(llvm::Value* a) {
    if (!a->getType()->isPointerTy())
        // null must be a pointer
        return "false";

    // need to have the PTA
    assert(PTA);
    PSNode *psnode = PTA->getPointsTo(a);
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

std::string PointsToPlugin::hasKnownSize(llvm::Value* a) {
    // check is a is getelementptr
    if (const llvm::GetElementPtrInst *GI = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsTo(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            // we know nothing about the allocated size
            return "maybe";
        }

        const auto& first = *(psnode->pointsTo.begin());
        for (const auto& ptr : psnode->pointsTo) {
            // ptset cannot contain null, unknown or invalidated pointer
            if (ptr.isNull() || ptr.isUnknown() || ptr.isInvalidated()){
                return "false";}
            // the sizes and offsets need to be the same
            if (ptr.offset.isUnknown() || *(ptr.offset) != *(first.offset) ||
                     ptr.target->getSize() != first.target->getSize()) {
                return "false";
            }
        }
        return "true";
    }
    else {
        return "false";
    }
}

std::tuple<llvm::Value*, uint64_t, uint64_t> PointsToPlugin::getPointerInfo(llvm::Value* a) {
    // check is a is getelementptr
    if (llvm::GetElementPtrInst *GI = llvm::dyn_cast<llvm::GetElementPtrInst>(a)) {
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsTo(GI->getPointerOperand());
        if (!psnode || psnode->pointsTo.empty()) {
            return std::make_tuple(nullptr, 0, 0);
        }

        const auto& first = *(psnode->pointsTo.begin());
        return std::make_tuple(GI->getPointerOperand(), *(first.offset),
                                first.target->getSize());

    }
    else {
        return std::make_tuple(nullptr, 0, 0);
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
    PSNode *psnode = PTA->getPointsTo(a);
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
                if(Iptr->getParent()->getParent() != F || cg.isRecursive(F))
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
        PSNode *psnode = PTA->getPointsTo(a);
        if (!psnode) return "maybe";
        for (auto& ptr : psnode->pointsTo) {
            llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
            if(llvmVal == b) return "true";
        }
    } else {
        return "true"; // a may point to b
    }

    return "false";
}

void PointsToPlugin::getReachableFunctions(std::set<const llvm::Function*>& reachableFunctions, const llvm::Function* from) {
    cg.getReachableFunctions(reachableFunctions, from);
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
        return new PointsToPlugin(module);
}
