#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <list>
#include "instr_plugin.hpp"
#include "rewriter.hpp"

class Logger;

class Analyzer
{

    using RememberedValues = std::vector<std::pair<llvm::Value*, std::string>>;
    using ValuesVector = std::vector<llvm::Value*>;

public:
    static std::unique_ptr<InstrPlugin> analyze(const std::string &path,
                                                llvm::Module* module);
    static bool shouldInstrument(const RememberedValues& rememberedValues,
                                 const ValuesVector& rememberedPTSets,
                                 InstrPlugin* plugin,
                                 const Condition &condition,
                                 const ValuesVector& parameters,
                                 Logger& logger);

private:
     Analyzer() {}
};

#endif
