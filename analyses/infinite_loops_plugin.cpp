#include "infinite_loops_plugin.hpp"
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

std::string InfiniteLoopsPlugin::handleUnconditional(const BranchInst* br) {
    BasicBlock* bb = br->getSuccessor(0);

    if (!bb)
        return "unknown";

    for (Instruction& succ : *bb) {
        if (isa<CallInst>(succ))
            return "unknown";

        if (isa<ReturnInst>(succ))
            return "unknown";

        if (isa<SwitchInst>(succ))
            return "unknown";

        auto* bs = dyn_cast<BranchInst>(&succ);

        if (bs && bs->isConditional())
            return "unknown";

        if (bs && bs->isUnconditional() &&
                  bs->getOperand(0) == br->getOperand(0))
            return "true";
        else if (bs && bs->isConditional())
            return "unknown";
    }
    return "unknown";
}

std::string InfiniteLoopsPlugin::handleConditional(const BranchInst* br) {
    return "unknown";
}

std::string InfiniteLoopsPlugin::isInfinite(Value* value) {
    // Support only branch instructions
    auto* inst = dyn_cast<BranchInst>(value);
    value->dump();
    if (!inst)
        return "unknown";

    if (inst->isUnconditional())
        return handleUnconditional(inst);
    else
        return handleConditional(inst);

    return "unknown";
}

static const std::string supportedQueries[] = {
    "isInfinite",
};

bool InfiniteLoopsPlugin::supports(const std::string& query) {
    for (unsigned idx = 0; idx < sizeof(supportedQueries)/sizeof(*supportedQueries); ++idx) {
        if (query == supportedQueries[idx])
            return true;
    }

    return false;
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new InfiniteLoopsPlugin(module);
}
