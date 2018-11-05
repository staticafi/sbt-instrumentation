#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <list>
#include "instr_plugin.hpp"
#include "rewriter.hpp"

class Analyzer {

  public:
	static std::unique_ptr<InstrPlugin> analyze(const std::string &path, llvm::Module* module);
	static bool shouldInstrument(const std::list<std::pair<llvm::Value*, std::string>>& rememberedValues,
                                 std::vector<llvm::Value*>& rememberedPTSets, InstrPlugin* plugin,
                                 const Condition &condition, const std::list<llvm::Value*>& parameters);

  private:
	 Analyzer() {}
};

#endif
