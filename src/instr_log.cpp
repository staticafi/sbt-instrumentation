#include "../lib/instr_log.hpp"
#include <fstream>
#include <string>

using namespace std;
using namespace llvm;

Logger::Logger (string path) {
  file = path;
  ofstream ofs;
  ofs.open(path, std::ofstream::out | std::ofstream::trunc);
  ofs.close();
}

void Logger::write_error(const string &text)
{
    std::ofstream log_file(
        file, ios_base::out | ios_base::app );
    log_file << "Error: " << text << endl;
}

void Logger::write_info(const string &text)
{
    std::ofstream log_file(
        file, ios_base::out | ios_base::app );
    log_file << "Info: " << text << endl;
}


/**
 * Writes log about inserting new call instruction.
 * @param where before/after
 * @param calledFunction function from inserted call
 * @param foundInstr found instruction for instrumentation
 */
void Logger::log_insertion(string where, Function* calledFunction, Instruction* foundInstr) {
	string newCall = calledFunction->getName().str();
	string foundInstrOpName = foundInstr->getOpcodeName();
	
	if(foundInstrOpName == "call") {
		if (CallInst *ci = dyn_cast<CallInst>(foundInstr)) { //TODO what if this fails
			// get called value and strip away any bitcasts
			llvm::Value *calledVal = ci->getCalledValue()->stripPointerCasts();
			string name;
			if (calledVal->hasName())
				name = calledVal->getName().str();
			else
				name = "<func pointer>";

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
		
		if(i < foundInstrs.size() - 1) {
			instructions += foundInstr.instruction + ", ";
		}
		else {
			instructions += foundInstr.instruction;
		}
		
		i++;
	}
		
	write_info("Replacing " + instructions + " with " + newInstr);
}
