#ifndef LLVM_INSTR_H
#define LLVM_INSTR_H

#include <list>
#include <string>
#include <map>

#include "rewriter.hpp"
#include "instr_analyzer.hpp"
#include "points_to_plugin.hpp"

#include <llvm/IR/Module.h>

typedef std::map<std::string, llvm::Value*> Variables;

class LLVMInstrumentation {
    public:
        llvm::Module& module;
        llvm::Module& definitionsModule;
        std::list<std::unique_ptr<InstrPlugin>> plugins;
        std::string outputName;
        std::list<llvm::Value*> rememberedValues;
        Variables variables;
        Rewriter rewriter;
		std::set<const llvm::Function*> reachableFunctions;
        PointsToPlugin* ppPlugin = nullptr;


        LLVMInstrumentation(llvm::Module& m, llvm::Module& dm)
          : module(m), definitionsModule(dm) {}
};

#endif
