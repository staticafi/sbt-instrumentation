#ifndef PREDATOR_PLUGIN_H
#define PREDATOR_PLUGIN_H

#include <set>
#include <llvm/IR/Module.h>
#include "instr_plugin.hpp"

class PredatorPlugin : public InstrPlugin
{
private:
    bool isPointerDangerous(const llvm::Value* deref) const;
    bool isInstructionDangerous(const llvm::Instruction& deref) const;

    void loadPredatorOutput();
    void runPredator(llvm::Module* mod);

    std::set<std::pair<unsigned, unsigned>> predatorErrors;
    bool predatorSuccess;

public:
    PredatorPlugin(llvm::Module* module) : InstrPlugin("Predator") {
        predatorSuccess = false;
        llvm::errs() << "PredatorPlugin: Running Predator...\n";
        runPredator(module);
        loadPredatorOutput();
    }

    bool supports(const std::string& query) override;

    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands) {

        if (query == "isInvalid") {
            assert(operands.size() == 1);
            if (isPointerDangerous(operands[0])) {
                return "maybe";
            } else {
                return "false";
            }
        } else if (query == "isValidPointer") {
            assert(operands.size() == 2);
            if (isPointerDangerous(operands[0])) {
                return "maybe";
            } else {
                return "true";
            }
        }

        return "unsupported query";
    }

    virtual ~PredatorPlugin() = default;
};


#endif /* PREDATOR_PLUGIN_H */
