#include "predator_plugin.hpp"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DebugInfoMetadata.h>

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

bool PredatorPlugin::someUserHasSomeErrorReport(const llvm::Value* operand) const {
    for (const auto& user : operand->users()) {
        if (auto* inst = llvm::dyn_cast_or_null<llvm::Instruction>(user)) {
            // there cannot be error report for instruction without debug location
            if (!inst->getDebugLoc())
                continue;

            const unsigned line = inst->getDebugLoc().getLine();
            const unsigned col = inst->getDebugLoc().getCol();

            if (errors.hasAnyReport(line, col)) {
                return true;
            }

        }
    }

    return false;
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
        return;
    }

    while (!is.eof()) {
        unsigned lineNumber, colNumber;
        std::string err;
        std::string col = "x";
        ErrorType et;

        is >> lineNumber;

        if (is.eof())
            break;

        is >> col;

        if (col == "none") {
            lineOnlyErrors.push_back(lineNumber);
        }

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

    if (errors.empty() && lineOnlyErrors.empty()) {
        llvm::errs() << "PredatorPlugin: Predator found no errors\n";
        abort();
    }

}

bool PredatorPlugin::supports(const std::string& query) {
    return predatorSuccess && (supportedQueries.find(query) != supportedQueries.end());
}

void PredatorPlugin::addReportsForLineErrors(llvm::Module* mod) {
    for (unsigned lineNumber : lineOnlyErrors) {
        bool found = false;
        for (llvm::GlobalVariable& var : mod->globals()) {
            llvm::SmallVector<llvm::DIGlobalVariableExpression *, 8> exprs;
            var.getDebugInfo(exprs);

            for (auto* e : exprs) {
                auto* gv = e->getVariable();
                if (gv) {
                    if (gv->getLine() == lineNumber) {
                        dangerous.insert(&var);
                        found = true;
                    }
                }
            }

        }
        if (!found) {
            predatorSuccess = false;
        }
    }
}
