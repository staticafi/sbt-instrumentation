#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <llvm/IR/Module.h>
#include "instr_plugin.hpp"

class Analyzer {

  public:
	static InstrPlugin* analyze(const std::string &path, llvm::Module* module);

  private:
	 Analyzer() {}
};

#endif
