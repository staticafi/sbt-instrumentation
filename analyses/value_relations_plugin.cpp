#include "value_relations_plugin.hpp"
#include <llvm/IR/LegacyPassManager.h>

using namespace llvm;

ValueRelationsPlugin::ValueRelationsPlugin(llvm::Module* module)
: InstrPlugin("ValueRelationsPlugin"), VR(module) {
    ///
    // Compute value relations
    VR.build();
    VR.compute();

}

const llvm::Value *getMemorySize(const llvm::Value *v) {
    if (auto AI = dyn_cast<AllocaInst>(v)) {
        if (AI->isArrayAllocation())
            return AI->getArraySize();
    } else if (auto CI = dyn_cast<CallInst>(v)) {
        auto F = CI->getCalledFunction();
        if (!F)
            return nullptr;
        if (F->getName().equals("malloc")) {
            return CI->getOperand(0);
        }
    }

    return nullptr;
}

std::string ValueRelationsPlugin::isValidPointer(Value *ptr, Value *len) const {
    if (!ptr->getType()->isPointerTy())
        // null must be a pointer
        return "false";

    uint64_t size = 0;
    if (ConstantInt *C = dyn_cast<ConstantInt>(len)) {
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

    auto GEP = dyn_cast<GetElementPtrInst>(ptr);
    if (!GEP) {
        return "unknown";
    }

    if (GEP->getNumIndices() > 1)
        return "nothandled";

    errs() << *GEP << " + " << *len << "\n";

    auto memSize = getMemorySize(GEP->getPointerOperand());
    if (!memSize)
        return "unknown";

    auto idx = GEP->getOperand(1);
    errs() << "idx " << *idx << "\n";

    return "unknown";

}


extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new ValueRelationsPlugin(module);
}
