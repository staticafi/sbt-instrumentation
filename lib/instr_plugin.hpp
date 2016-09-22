#ifndef INSTR_PLUGIN_H
#define INSTR_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

class InstrPlugin
{
	public:
	  InstrPlugin();

	  /* use virtual otherwise linker will try to perform static linkage */
	  virtual bool isEqual(llvm::Value* a, llvm::Value* b);
	  virtual bool isNotEqual(llvm::Value* a, llvm::Value* b);
	  virtual bool greaterThan(llvm::Value* a, llvm::Value* b);
	  virtual bool lessThan(llvm::Value* a, llvm::Value* b);
	  virtual bool lessOrEqual(llvm::Value* a, llvm::Value* b);
	  virtual bool greaterOrEqual(llvm::Value* a, llvm::Value* b);
	  virtual bool isConstant(llvm::Value* a);
};

#endif
