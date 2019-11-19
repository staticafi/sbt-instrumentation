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
    "mayBeLeaked",
    "mayBeLeakedOrFreed",
    "safeForFree",
};


extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new PredatorPlugin(module);
}

bool PredatorPlugin::someUserHasErrorReport(const llvm::Value* operand, ErrorType et) const {
    for (const auto& user : operand->users()) {
        if (auto* inst = llvm::dyn_cast_or_null<llvm::Instruction>(user)) {
            // there cannot be error report for instruction without debug location
            if (!inst->getDebugLoc())
                continue;

            const unsigned line = inst->getDebugLoc().getLine();
            const unsigned col = inst->getDebugLoc().getCol();

            if (errors.hasReport(line, col, et)) {
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
    llvm::errs() << "|> " << str << "\n";
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
        std::string err;
        ErrorType et;

        is >> lineNumber;

        if (is.eof())
            break;

        is >> colNumber;

        if (is.eof())
            break;

        is >> err;

        if (err == "invalid") {
            et = ErrorType::Invalid;
        } else if (err == "leak") {
            et = ErrorType::Leak;
        } else if (err == "free") {
            et = ErrorType::Free;
        } else {
            assert(false && "PredatorPlugin: got invalid error type");
            abort();
        }

        errors.add(lineNumber, colNumber, et);
    }

}

bool PredatorPlugin::supports(const std::string& query) {
    return predatorSuccess && (supportedQueries.find(query) != supportedQueries.end());
}
