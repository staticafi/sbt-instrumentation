#include "predator_plugin.hpp"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_os_ostream.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>

static const std::unordered_set<std::string> supportedQueries {
    "isValidPointer",
    "isInvalid",
};


extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new PredatorPlugin(module);
}

bool PredatorPlugin::isInstructionDangerous(const llvm::Instruction& inst) const {
    if (!inst.getDebugLoc())
        return false;

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

void PredatorPlugin::runPredator(llvm::Module* mod) {

    // write module to aux file
    {
        std::ofstream ofs("predator_in.bc");
        llvm::raw_os_ostream ostream(ofs);
#if (LLVM_VERSION_MAJOR > 6)
        llvm::WriteBitcodeToFile(*mod, ostream);
#else
        llvm::WriteBitcodeToFile(mod, ostream);
#endif
    }

    // build predator command
    std::stringstream cmd;
    cmd << "predator_wrapper.py "
        << "--out predator.log "
        // TODO: only if 32-bit
        << "--32 "
        << "predator_in.bc ";

    // run predator on that file
    auto str = cmd.str();
    std::system(str.c_str());
}

void PredatorPlugin::loadPredatorOutput() {
    std::ifstream is("predator.log");
    if (!is.is_open()) {
        llvm::errs() << "PredatorPlugin: failed to open file with predator output\n";
        return;
    }

    std::string result;
    is >> result;
    predatorSuccess = (result == "ok");

    if (result != "ok") {
        llvm::errs() << "PredatorPlugin: Predator failed with '" << result << "', always saying \"maybe\" \n";
    }

    while (!is.eof()) {
        unsigned lineNumber, colNumber;
        is >> lineNumber;

        if (is.eof())
            break;

        is >> colNumber;

        predatorErrors.insert(std::make_pair(lineNumber, colNumber));
    }

}

bool PredatorPlugin::supports(const std::string& query) {
    return predatorSuccess && (supportedQueries.find(query) != supportedQueries.end());
}
