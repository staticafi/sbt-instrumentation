#include "predator_plugin.hpp"

#include <unordered_set>
#include <fstream>

static const std::unordered_set<std::string> supportedQueries {
    "isValidPointer",
    "isNull",
    "isInvalid",
};


extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new PredatorPlugin(module);
}

bool PredatorPlugin::isInstructionDangerous(const llvm::Instruction& inst) const {
    const unsigned line = inst.getDebugLoc().getLine();
    const unsigned col = inst.getDebugLoc().getCol();

    const auto pair = std::make_pair(line, col);

    return predatorErrors.find(pair) != predatorErrors.end();
}

bool PredatorPlugin::isPointerDangerous(const llvm::Value* deref) const {
    // pointer is dangerous if some of its users is dangerous
    for (const auto& user : deref->users()) {
        if (auto* inst = llvm::dyn_cast_or_null<llvm::Instruction>(user)) {
            if (isInstructionDangerous(*inst)) {
                return true;
            }
        }
    }

    return false;
}

void PredatorPlugin::loadPredatorOutput(const std::string& name) {
    // TODO
    std::ifstream is(name);
    if (!is.is_open()) {
        llvm::errs() << "PredatorPlugin: failed to open file with predator output\n";
        return;
    }

    while (!is.eof()) {
        unsigned lineNumber, colNumber;
        is >> lineNumber;
        is >> colNumber;
        predatorErrors.insert(std::make_pair(lineNumber, colNumber));
    }

}

bool PredatorPlugin::supports(const std::string& query) {
    return supportedQueries.find(query) != supportedQueries.end();
}
