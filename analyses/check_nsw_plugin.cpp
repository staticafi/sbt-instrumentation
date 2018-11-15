#include "check_nsw_plugin.hpp"
#include <limits>
#include <cmath>

#include <llvm/IR/Operator.h>

using namespace llvm;

std::string CheckNSWPlugin::canOverflow(Value* value) {
    // Support only instructions
    auto* inst = dyn_cast<Instruction>(value);
    if (!inst)
        return "unknown";

    // Support only instruction of integer type
    const auto* intT = dyn_cast<IntegerType>(inst->getType());
    if (!intT)
        return "unknown";

    if (const auto* binOp
        = dyn_cast<OverflowingBinaryOperator>(inst)) {
        // we are looking for bin. ops with nsw
        if (!binOp->hasNoSignedWrap())
            return "false";
    }

    return "unknown";
}

static const std::string supportedQueries[] = {
    "canOverflow",
};

bool CheckNSWPlugin::supports(const std::string& query) {
    for (unsigned idx = 0; idx < sizeof(supportedQueries)/sizeof(*supportedQueries); ++idx) {
        if (query == supportedQueries[idx])
            return true;
    }

    return false;
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new CheckNSWPlugin(module);
}
