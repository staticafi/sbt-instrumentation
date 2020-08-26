#include <fstream>
#include <string>

#include "instr_log.hpp"

using namespace std;
using namespace llvm;

/**
 * Writes log about inserting new call instruction.
 * @param where before/after
 * @param calledFunction function from inserted call
 * @param foundInstr found instruction for instrumentation
 */
void Logger::log_insertion(const string& where,
                           const Function* calledFunction,
                           const Instruction* foundInstr) {
    string newCall = calledFunction->getName().str();
    string foundInstrOpName = foundInstr->getOpcodeName();

    if (foundInstrOpName == "call") {
        if (const CallInst *ci = dyn_cast<CallInst>(foundInstr)) {
            // get called value and strip away any bitcasts
#if LLVM_VERSION_MAJOR >= 8
            const llvm::Value *calledVal = ci->getCalledOperand()->stripPointerCasts();
#else
            const llvm::Value *calledVal = ci->getCalledValue()->stripPointerCasts();
#endif
            string name;
            if (calledVal->hasName()) {
                name = calledVal->getName().str();
            }
            else {
                name = "<func pointer>";
            }

            write_info("Inserting " + newCall + " " +  where + " " +
                              foundInstrOpName + " " + name);
        }
    }
    else {
        write_info("Inserting " + newCall + " " +  where + " " + foundInstrOpName);
    }
}

/**
 * Writes log about replacing instruction.
 * @param foundInstrs found instructions for instrumentation
 * @param newInstr name of the new instruction
 */
void Logger::log_insertion(InstrumentSequence foundInstrs, string newInstr) {

    string instructions;
    uint i = 0;

    for (list<InstrumentInstruction>::iterator sit=foundInstrs.begin(); sit != foundInstrs.end(); ++sit) {
        InstrumentInstruction foundInstr = *sit;

        if (i < foundInstrs.size() - 1) {
            instructions += foundInstr.instruction + ", ";
        }
        else {
            instructions += foundInstr.instruction;
        }

        i++;
    }

    write_info("Replacing " + instructions + " with " + newInstr);
}
