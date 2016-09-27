#include "../lib/rewriter.hpp"
#include "../lib/instr_log.hpp"
#include "../lib/instr_analyzer.hpp"
#include <iostream>
#include <fstream>
#include <exception>

#include <llvm/Pass.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#if (LLVM_VERSION_MINOR >= 5)
  #include "llvm/IR/InstIterator.h"
#else
  #include "llvm/Support/InstIterator.h"
#endif
#include <llvm/Support/SourceMgr.h>

//#include "llvm-c/BitWriter.h"

#include <memory>
#include <string>

#define uint unsigned int

using namespace llvm;

using namespace std;

Logger logger("log.txt");
list<unique_ptr<InstrPlugin>> plugins;

string outputName;

void usage(char *name) {
	cerr << "Usage: " << name << " <config.json> <llvm IR> <outputFileName>" << endl;
}

uint64_t getAllocatedSize(Instruction *I, Module* M){
	DataLayout* DL = new DataLayout(M);

	const AllocaInst *AI = dyn_cast<AllocaInst>(I);

	if(!AI)
		return 0;

	Type* Ty = AI->getAllocatedType();

	if(!Ty->isSized())
		return 0;

	uint64_t size = DL->getTypeAllocSize(Ty);

	delete DL;

	return size;
}

/**
 * Returns the next instruction after the one specified.
 * @param ins specified instruction
 * @return next instruction or null, if ins is the last one.
 */
Instruction* GetNextInstruction(Instruction* ins) {
	BasicBlock::iterator I(ins);
    if (++I == ins->getParent()->end())
      return NULL;
    return &*I;
}

/**
 * Returns the previous instruction before the one specified.
 * @param ins specified instruction
 * @return previous instruction or null, if ins is the first one.
 */
Instruction* GetPreviousInstruction(Instruction* ins) {
	BasicBlock::iterator I(ins);
    if (--I == ins->getParent()->begin())
      return NULL;
    return &*I;
}

/**
 * Inserts new call instruction.
 * @param I first instruction to be removed
 * @param count number of instructions to be removed
 */
void EraseInstructions(Instruction &I, int count) {
	Instruction* currentInstr = &I;
	for (int i = 0; i < count; i++) {
		if (!currentInstr){
			return;
		}
		Instruction* prevInstr = GetPreviousInstruction(currentInstr);
		currentInstr->eraseFromParent();
		currentInstr = prevInstr;
	}

}

/**
 * Inserts new call instruction.
 * @param CalleeF function to be called
 * @param args arguments of the function to be called
 * @param rw_rule relevant rewrite rule
 * @param currentInstr current instruction
 * @param Iiterator pointer to instructions iterator
 */
void InsertCallInstruction(Function* CalleeF, vector<Value *> args, RewriteRule rw_rule, Instruction &currentInstr, inst_iterator *Iiterator) {
	// Create new call instruction
	CallInst *newInstr = CallInst::Create(CalleeF, args);

	if(rw_rule.where == InstrumentPlacement::BEFORE) {
		// Insert before
		newInstr->insertBefore(&currentInstr);
		logger.log_insertion("before",CalleeF, &currentInstr);
	}
	else if(rw_rule.where == InstrumentPlacement::AFTER) {
		// Insert after
		newInstr->insertAfter(&currentInstr);
		logger.log_insertion("after", CalleeF, &currentInstr);
	}
	else if(rw_rule.where == InstrumentPlacement::REPLACE) {
		// TODO: Make the functions use the iterator instead of
		// the instruction then check this works
		// In the end we move the iterator to the newInst position
		// so we can safely remove the sequence of instructions being
		// replaced

		newInstr->insertAfter(&currentInstr);
		inst_iterator helper(*Iiterator);
		*Iiterator = ++helper;
		EraseInstructions(currentInstr, rw_rule.foundInstrs.size());
		logger.log_insertion(rw_rule.foundInstrs, rw_rule.newInstr.instruction);
	}
}


/**
 * Inserts an argument.
 * @param Irw_rule rewrite rule
 * @param I instruction
 * @param CalleeF function to be called
 * @param variables map of found parameters from config
 */
vector<Value *> InsertArgument(RewriteRule rw_rule, Instruction &I, Function* CalleeF, map <string, Value*> variables) {
	std::vector<Value *> args;
	unsigned i = 0;
	for (list<string>::iterator sit=rw_rule.newInstr.parameters.begin(); sit != rw_rule.newInstr.parameters.end(); ++sit) {

		if(i == rw_rule.newInstr.parameters.size() - 1) {
			break;
		}

		string arg = *sit;

		if(variables.find(arg) == variables.end()) {
			logger.write_info(arg);
			// NOTE: in future think also about other types than ConstantInt
			int argInt;
			try {
				argInt = stoi(arg);
				LLVMContext &Context = getGlobalContext();
				Value *intValue = ConstantInt::get(Type::getInt32Ty(Context), argInt);
				args.push_back(intValue);
			}
			catch (invalid_argument) {
				logger.write_error("Problem with instruction arguments: invalid argument.");
			}
			catch (out_of_range) {
				logger.write_error("Problem with instruction arguments: out of range.");
			}
		}
		else {
			unsigned argIndex = 0;
			for (Function::ArgumentListType::iterator sit=CalleeF->getArgumentList().begin(); sit != CalleeF->getArgumentList().end(); ++sit) {
				Value *argV = &*sit;

				if(i == argIndex) {
					if(argV->getType() != variables[arg]->getType()) {
						//TODO other types?
						if(!variables[arg]->getType()->isPtrOrPtrVectorTy()){
							args.push_back(variables[arg]);
						}else{
							CastInst *CastI = CastInst::CreatePointerCast(variables[arg], argV->getType());
							CastI->insertAfter(&I);
							args.push_back(CastI);
						}
					}
					else{
						args.push_back(variables[arg]);
					}
					break;
				}

				argIndex++;
			}
		}

		i++;
	}
	return args;
}

/**
 * Applies a rule.
 * @param M module
 * @param I current instruction
 * @param rw_rule rule to apply
 * @param variables map of found parameters form config
 * @param Iiterator pointer to instructions iterator
 * @return 1 if error
 */
int applyRule(Module &M, Instruction &currentInstr, RewriteRule rw_rule, map <string, Value*> variables, inst_iterator *Iiterator) {

	logger.write_info("Applying rule...");

	// Work just with call instructions for now...
	if(rw_rule.newInstr.instruction != "call") {
		logger.write_error("Not working with this instruction: " + rw_rule.newInstr.instruction);
		return 1;
	}

	// Get operands
	std::vector<Value *> args;
	Function *CalleeF = NULL;

	// Get name of function
	string param = *(--rw_rule.newInstr.parameters.end());
	CalleeF = M.getFunction(param);
	if (!CalleeF) {
		logger.write_error("Unknown function: " + param);
		return 1;
	}

	// Insert arguments
	args = InsertArgument(rw_rule, currentInstr, CalleeF, variables);

	// Insert new call instruction
	InsertCallInstruction(CalleeF, args, rw_rule, currentInstr, Iiterator);

	return 0;
}

/**
 * Checks if the operands of instruction match.
 * @param rwIns instruction from rewrite rule.
 * @param ins instruction to be checked.
 * @param variables map for remembering some parameters.
 * @return true if OK, false otherwise
 */
bool CheckOperands(InstrumentInstruction rwIns, Instruction* ins, map <string, Value*> &variables) {
	unsigned opIndex = 0;
	for (list<string>::iterator sit=rwIns.parameters.begin(); sit != rwIns.parameters.end(); ++sit) {
		string param = *sit;
		if(rwIns.parameters.size() == 1 && param=="*"){
			return true;
		}

		if(opIndex > ins->getNumOperands() - 1) {
			return false;
		}

		llvm::Value *op = ins->getOperand(opIndex);
		if(param[0] == '<' && param[param.size() - 1] == '>') {
			variables[param] = op;
		} else if(param != "*"
				  && param != (op->stripPointerCasts()->getName()).str()) {
				  // NOTE: we're comparing a name of the value, but the name
				  // is set only sometimes. Since we're now matching just CallInst
				  // it is OK, but it may not be OK in the future
			return false;
		}

		opIndex++;
	}

	return true;
}

/**
 * Runs all plugins for static analyses and decides, whether to
 * instrument or not.
 * @param condition condition that must be satisfied to instrument
 * @param variables
 * @return true if condition is ok, false otherwise
 */
bool checkAnalysis(list<string> condition, map<string, Value*> variables){
	// condition: first element is operator, other one or two elements
	// are variables, TODO do we need more than two variables?
	string conditionOp = condition.front();
	list<string>::iterator it = condition.begin();
	it++;
	string aName = *it;
	string bName = "";

	Value* aValue = (variables.find(aName))->second;
	Value* bValue = NULL;
	if(condition.size()>2){
		it++;
		bName = *it;
		bValue = (variables.find(bName))->second;
	}

	bool shouldInstrument = true;
	for (list<unique_ptr<InstrPlugin>>::const_iterator ci = plugins.begin(); ci != plugins.end(); ++ci){
		if(!Analyzer::shouldInstrument((*ci).get(), conditionOp, aValue, bValue)){
			shouldInstrument = false;
			break;
		}
	}
	return shouldInstrument;
}

/**
 * Checks if the given instruction should be instrumented.
 * @param ins instruction to be checked.
 * @param M module.
 * @param rw_config parsed rules to apply.
 * @param Iiterator pointer to instructions iterator
 * @return true if OK, false otherwise
 */
bool CheckInstruction(Instruction* ins, Module& M, Function* F, RewriterConfig rw_config, inst_iterator *Iiterator) {
		// iterate through rewrite rules
		for (list<RewriteRule>::iterator it=rw_config.begin(); it != rw_config.end(); ++it) {
			RewriteRule rw = *it;

			// check if this rule should be applied in this function
			string functionName = F->getName().str();

			if(rw.inFunction != "*" && rw.inFunction!=functionName)
				continue;

			// check sequence of instructions
			map <string, Value*> variables;
			bool instrument = false;
			Instruction* currentInstr = ins;

			for (list<InstrumentInstruction>::iterator iit=rw.foundInstrs.begin(); iit != rw.foundInstrs.end(); ++iit) {
				if(currentInstr == NULL) {
				    break;
				}

				InstrumentInstruction checkInstr = *iit;

				// check the name
				if(currentInstr->getOpcodeName() == checkInstr.instruction) {
					// check operands
					if(!CheckOperands(checkInstr, currentInstr, variables)) {
						break;
					}

					// check return value
					if(checkInstr.returnValue != "*") {
						if(checkInstr.returnValue[0] == '<'
						&& checkInstr.returnValue[checkInstr.returnValue.size() - 1] == '>') {
							variables[checkInstr.returnValue] = currentInstr;
						}
					}

					// load next instruction to be checked
					list<InstrumentInstruction>::iterator final_iter = rw.foundInstrs.end();
					--final_iter;
					if (iit != final_iter) {
						currentInstr = GetNextInstruction(ins);
					}
					else {
						instrument = true;
					}
				}
				else {
					break;
				}
			 }

	if(rw.foundInstrs.size() == 1){
		InstrumentInstruction allocaIns = rw.foundInstrs.front();
		if(!allocaIns.getSizeTo.empty()){
			LLVMContext &Context = getGlobalContext();
			variables[allocaIns.getSizeTo] = ConstantInt::get(Type::getInt64Ty(Context), getAllocatedSize(ins,&M));
		}

	}


			 // if all instructions match, try to instrument the code
			 if(instrument && checkAnalysis(rw.condition,variables)) {
				 	// try to apply rule
					Instruction *where;
				 	if(rw.where == InstrumentPlacement::BEFORE){
						where = ins;
					}
					else {
						// It is important in the REPLACE case that
						// we first place the new instruction after
						// the sequence
						where = currentInstr;
					}

					if(applyRule(M, *where, rw, variables, Iiterator) == 1) {
						logger.write_error("Cannot apply rule.");
						return false;
					}
			 }
		  }

	return true;
}

/**
 * Instruments given module with rules from json file.
 * @param M module to be instrumented.
 * @param rw_config parsed rules to apply.
 * @return true if OK, false otherwise
 */
bool instrumentModule(Module &M, RewriterConfig rw_config) {
   for (Module::iterator Fiterator = M.begin(), E = M.end(); Fiterator != E; ++Fiterator) {

	   // Do not instrument functions linked for instrumentation
	   string functionName = (&*Fiterator)->getName().str();

	   if(functionName.find("__INSTR_")!=string::npos ||
		  functionName.find("__VERIFIER_")!=string::npos) { //TODO just starts with
		   logger.write_info("Omitting function " + functionName + " from instrumentation.");
		   continue;
	   }

	   for (inst_iterator Iiterator = inst_begin(&*Fiterator), End = inst_end(&*Fiterator); Iiterator != End; ++Iiterator) {
		// This iterator may be replaced (by an iterator to the following
		// instruction) in the InsertCallInstruction function
			// Check if the instruction is relevant
		    if(!CheckInstruction(&*Iiterator, M, &*Fiterator, rw_config, &Iiterator)) return false;
		}
	 }

  // Write instrumented module into the output file
  ofstream out_file;
  out_file.open(outputName, ofstream::out | ofstream::trunc);
  raw_os_ostream rstream(out_file);

  WriteBitcodeToFile(&M, rstream);

  return true;
}



void loadPlugins(Rewriter rw, Module* module){
	for(list<string>::iterator it=rw.analysisPaths.begin(); it != rw.analysisPaths.end(); ++it){
		plugins.push_back(Analyzer::analyze(*it,module));
	}
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		usage(argv[0]);
		exit(1);
	}

	// TODO: Check for failure
	ifstream config_file;
	config_file.open(argv[1]);

	ifstream llvmir_file;
	llvmir_file.open(argv[2]);

	outputName = argv[3];

	// Parse json file
	Rewriter rw;
	try {
		rw.parseConfig(config_file);
	}
	catch (runtime_error ex){
		string exceptionString = "Error parsing configuration: ";
		logger.write_error(exceptionString.append(ex.what()));
		config_file.close();
		llvmir_file.close();
		return 1;
	}

	RewriterConfig rw_config = rw.getConfig();

	// Get module from LLVM file
	LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
#if (LLVM_VERSION_MINOR >= 6)
    std::unique_ptr<Module> _m = parseIRFile(argv[2], Err, Context);
    Module *m = _m.release();
#else
    Module *m = ParseIRFile(argv[2], Err, Context);
#endif
    if (!m) {
		logger.write_error("Error parsing .bc file.");
        Err.print(argv[0], errs());
        config_file.close();
		llvmir_file.close();
        return 1;
    }

	loadPlugins(rw,m);

    // Instrument
    bool resultOK = instrumentModule(*m, rw_config);

    config_file.close();
    llvmir_file.close();

    if(resultOK) {
		logger.write_info("DONE.");
		return 0;
	}
	else {
		logger.write_error("FAILED.");
		return 1;
	}
}
