#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <llvm/IR/Module.h>

class Analyzer {

  public:
	static bool shouldInstrument(const std::string &path, llvm::Module* module);

  private:
	 Analyzer() {}
};

#endif
