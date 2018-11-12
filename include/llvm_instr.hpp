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
        std::vector<std::pair<llvm::Value*, std::string>> rememberedValues;
        std::vector<llvm::Value*> rememberedPTSets;
        bool rememberedUnknown = false;
        Variables variables;
        Rewriter rewriter;
		std::set<const llvm::Function*> reachableFunctions;
        PointsToPlugin* ppPlugin = nullptr;


        LLVMInstrumentation(llvm::Module& m, llvm::Module& dm)
          : module(m), definitionsModule(dm) {}
};

#endif
