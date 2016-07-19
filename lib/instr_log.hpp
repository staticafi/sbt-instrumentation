#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

#include "rewriter.hpp"

class Logger {
    std::string file;
  public:
    Logger (std::string path);
    void write_error (const std::string &text);
    void write_info (const std::string &text);
    		
	/**
	 * Writes log about inserting new call instruction.
	 * @param where before/after
	 * @param calledFunction function from inserted call
	 * @param foundInstr found instruction for instrumentation
	 */
	void LogInsertion(std::string where, llvm::Function* calledFunction, llvm::Instruction* foundInstr);

	/**
	 * Writes log about replacing instruction.
	 * @param foundInstrs found instructions for instrumentation
	 * @param newInstr name of the new instruction
	 */
	void LogInsertion(InstrumentSequence foundInstrs, std::string newInstr);
};

#endif
