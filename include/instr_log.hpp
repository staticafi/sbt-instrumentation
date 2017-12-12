#ifndef LOGGER_H
#define LOGGER_H

#include <ostream>
#include <string>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

#include "rewriter.hpp"

class Logger {
    std::ofstream stream;
  public:
    Logger(const std::string& path) {
        stream.open(path, std::ios::out | std::ios::trunc);
    }

    ~Logger() {
        stream.flush();
        stream.close();
    }

    void write_error(const std::string &text) {
        stream << "Error: " << text << "\n";
    }

    void write_info(const std::string &text) {
        stream << "Info: " << text << "\n";
    }

    /**
     * Writes log about inserting new call instruction.
     * @param where before/after
     * @param calledFunction function from inserted call
     * @param foundInstr found instruction for instrumentation
     */
    void log_insertion(const std::string& where,
                       const llvm::Function* calledFunction,
                       const llvm::Instruction* foundInstr);

    /**
     * Writes log about replacing instruction.
     * @param foundInstrs found instructions for instrumentation
     * @param newInstr name of the new instruction
     */
    void log_insertion(InstrumentSequence foundInstrs, std::string newInstr);
};

#endif
