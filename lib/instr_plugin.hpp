#ifndef INSTR_PLUGIN_H
#define INSTR_PLUGIN_H

#include <llvm/IR/Module.h>

class InstrPlugin
{
	public:
	  InstrPlugin();

  /* use virtual otherwise linker will try to perform static linkage */
  virtual bool Analyze(llvm::Module* module);

};

#endif
