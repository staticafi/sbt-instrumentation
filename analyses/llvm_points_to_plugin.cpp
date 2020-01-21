#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#include "llvm_points_to_plugin.hpp"

#include <string>
#include <iostream>

using namespace llvm;

std::string LLVMPointsToPlugin::pointsToStack(Value* a) {
    auto val = a->stripPointerCasts();
    if (isa<AllocaInst>(val)) {
        return "true";
    } else if (isa<GlobalValue>(val)) {
        return "false";
    } else if (isa<Constant>(val) && !isa<ConstantExpr>(val)) {
        return "false";
    }

    return "maybe";
}

std::string LLVMPointsToPlugin::pointsToGlobal(Value* a) {
    auto val = a->stripPointerCasts();
    if (isa<AllocaInst>(val)) {
        return "false";
    } else if (isa<GlobalValue>(val)) {
        return "true";
    }

    return "maybe";
}

std::string LLVMPointsToPlugin::pointsToHeap(Value* a) {
    auto val = a->stripPointerCasts();
    if (isa<AllocaInst>(val)) {
        return "false";
    } else if (isa<GlobalValue>(val)) {
        return "false";
    } else if (isa<Constant>(val) && !isa<ConstantExpr>(val)) {
        return "false";
    } else if (auto C = dyn_cast<CallInst>(val)) {
        auto fun = C->getCalledFunction();
        auto name = fun->getName();
        if (name.equals("malloc") || name.equals("realloc") ||
            name.equals("calloc")) {
            return "true";
        }
    }

    return "maybe";
}

std::string LLVMPointsToPlugin::isNull(Value* a) {
    if (auto C = dyn_cast<ConstantInt>(a)) {
        if (C->getZExtValue() == 0)
            return "true";
        else
            return "false";
    } else if (isa<ConstantPointerNull>(a)) {
        return "true";
    }

    return "maybe";
}

std::string LLVMPointsToPlugin::isValidPointer(Value* a, Value *len) {
    if (!a->getType()->isPointerTy())
        // null must be a pointer
        return "false";

    uint64_t size = 0;
    if (auto *C = dyn_cast<ConstantInt>(len)) {
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

    if (auto A = dyn_cast<AllocaInst>(a->stripPointerCasts())) {
        size_t memsize = DL.getTypeAllocSize(A->getAllocatedType());
        if (A->isArrayAllocation()) {
            if (auto *C = dyn_cast<ConstantInt>(A->getArraySize())) {
                auto elemNum = C->getLimitedValue();
                if (elemNum == ~((uint64_t) 0))
                    return "maybe";
                memsize *= elemNum;
            } else {
                return "maybe";
            }
        }

        // if the offset + size > the size of allocated memory,
        // then this can be invalid operation. Check it so that
        // we won't overflow, that is, first ensure that psnode->size <= size
        // and than use this fact and equality with ptr.offset + size > psnode->size)
        if (size <= memsize) {
            return "true";
        }

        // FIXME: recursive functions...

    }

    // FIXME: pointers to global variables

    // this pointer is valid
    return "maybe";
}

std::string LLVMPointsToPlugin::mayBeLeaked(llvm::Value* a) {
    if (llvm::isa<llvm::ConstantInt>(a)) {
        return "false";
    }

    // assume that undefined functions
    // do not return a pointer to leakable memory
    if (auto *C = llvm::dyn_cast<llvm::CallInst>(a)) {
        auto *F = llvm::dyn_cast<llvm::Function>(
                    C->getCalledValue()->stripPointerCasts());
        if (F && F->isDeclaration()) {
            return "false";
        }
    }

    if (llvm::isa<llvm::AllocaInst>(a)) {
        return "false";
    }

    return "maybe";
}


static const std::string supportedQueries[] = {
    "isValidPointer",
    "isNull",
    "pointsToHeap",
    "pointsToGlobal",
    "pointsToStack",
};

bool LLVMPointsToPlugin::supports(const std::string& query) {
    for (unsigned idx = 0; idx < sizeof(supportedQueries)/sizeof(*supportedQueries); ++idx) {
        if (query == supportedQueries[idx])
            return true;
    }

    return false;
}

extern "C" InstrPlugin* create_object(Module* module) {
        return new LLVMPointsToPlugin(module);
}
