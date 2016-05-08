#include "../lib/rewriter.hpp"
#include "../lib/instr_log.hpp"
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
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#if (LLVM_VERSION_MINOR >= 8)
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

string outputName;

void usage(char *name) {
	cerr << "Usage: " << name << " <config.json> <llvm IR> <outputFileName>" << endl; 
}

/**
 * Gets name of the function.
 * @param f function
 * @return name of the function
 */
string GetNameOfFunction(Function* f) {
	return f->getName().str(); //TODO does it always have a name? 
}

/**
 * Writes log about inserting new call instruction.
 * @param where before/after
 * @param calledFunction function from inserted call
 * @param foundInstr found instruction for instrumentation
 */
void LogInsertion(string where, Function* calledFunction, Instruction* foundInstr) {
	string newCall = GetNameOfFunction(calledFunction);
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

			logger.write_info("Inserting " + newCall + " " +  where + " " +
			                  foundInstrOpName + " " + name);
		}
	}
	else {
		logger.write_info("Inserting " + newCall + " " +  where + " " + foundInstrOpName);
	}
}

/**
 * Inserts new call instruction.
 * @param CalleeF function to be called
 * @param args arguments of the function to be called
 * @param rw_rule relevant rewrite rule
 * @param I current instruction
 */
void InsertCallInstruction(Function* CalleeF, vector<Value *> args, RewriteRule rw_rule, Instruction &I) {
	// Create new call instruction
	CallInst *newInst = CallInst::Create(CalleeF, args);

	if(rw_rule.where == InstrumentPlacement::BEFORE) {
		// Insert before
		LogInsertion("before",CalleeF, &I);
		newInst->insertBefore(&I);
	}
	else if(rw_rule.where == InstrumentPlacement::AFTER) {
		// Insert after
		LogInsertion("after", CalleeF, &I);
		newInst->insertAfter(&I);
	}
	else if(rw_rule.where == InstrumentPlacement::REPLACE) {
		// Replace
		logger.write_info("Replacing " + rw_rule.foundInstr.instruction + " with " + rw_rule.newInstr.instruction);
		newInst->insertBefore(&I);
		I.eraseFromParent();
	}
}

/**
 * Inserts argument.
 * @param Irw_rule rewrite rule
 * @param I instruction
 * @param CalleeF function to be called
 * @param variables map of found parameters form config
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
			// TODO Whoops, what now? Change config json? Allow just integers?
			int argInt;
			try {
				argInt = stoi(arg);
				LLVMContext &Context = getGlobalContext();
				Value *intValue = ConstantInt::get(Type::getInt32Ty(Context), argInt);
				args.push_back(intValue);
			}
			catch (invalid_argument) {
				// TODO what now?
			}
			catch (out_of_range) {
				// TODO what now
			}
		}
		else {	
			unsigned argIndex = 0;
			for (Function::ArgumentListType::iterator sit=CalleeF->getArgumentList().begin(); sit != CalleeF->getArgumentList().end(); ++sit) {
				Value *argV = &*sit; 
												
				if(i == argIndex) {
					if(argV->getType() != variables[arg]->getType()) {
						//TODO other types?
						CastInst *CastI = CastInst::CreatePointerCast(variables[arg], argV->getType());
						CastI->insertBefore(&I);
						args.push_back(CastI);
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
 * @return 1 if error
 */
int applyRule(Module &M, Instruction &I, RewriteRule rw_rule, map <string, Value*> variables) {
	logger.write_info("Applying rule...");

	// Work just with call instructions for now...
	if(rw_rule.newInstr.instruction != "call") {
		cerr << "Not working with this instruction: " << rw_rule.newInstr.instruction << endl;
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
		cerr << "Unknown function " << param << endl;
		logger.write_error("Unknown function: " + param);	
		return 1;
	}
	
	// Insert arguments
	args = InsertArgument(rw_rule, I, CalleeF, variables);	
	
	// Insert new call instruction
	InsertCallInstruction(CalleeF, args, rw_rule, I);

	return 0;
}

/**
 * Checks if the operands of instruction match.
 * @param rw rewrite rule.
 * @param ins instruction to be checked.
 * @param variables map for remembering some parameters.
 * @return true if OK, false otherwise
 */
bool CheckOperands(RewriteRule rw, Instruction* ins, map <string, Value*> &variables) {
	unsigned opIndex = 0;
	bool apply = true;
	for (list<string>::iterator sit=rw.foundInstr.parameters.begin(); sit != rw.foundInstr.parameters.end(); ++sit) {
		string param = *sit; 
						
		if(opIndex > ins->getNumOperands() - 1) {
			apply = false;
			break;
		}

		llvm::Value *op = ins->getOperand(opIndex);
		if(param[0] == '<' && param[param.size() - 1] == '>') {
			variables[param] = op;
		} else if(param != "*"
				  && param != (op->stripPointerCasts()->getName()).str()) {
				  // NOTE: we're comparing a name of the value, but the name
				  // is set only sometimes. Since we're now matching just CallInst
				  // it is OK, but it may not be OK in the future
			apply = false;
			break;
		}

		opIndex++;
	}

	return apply;
}

/**
 * Checks if the given instruction should be instrumented.
 * @param ins instruction to be checked.
 * @param M module.
 * @param rw_config parsed rules to apply.
 * @return true if OK, false otherwise
 */
bool CheckInstruction(Instruction* ins, Module& M, Function* F, RewriterConfig rw_config) {
		// iterate through rewrite rules
		for (list<RewriteRule>::iterator it=rw_config.begin(); it != rw_config.end(); ++it) {
			RewriteRule rw = *it; 
			
			// check if this rule should be applied in this function
			string functionName = GetNameOfFunction(F);
			
			if(rw.inFunction != "*" && rw.inFunction!=functionName)
				continue;
			
			// if instruction from rewrite rule is the same as current instruction
			if(ins->getOpcodeName() == rw.foundInstr.instruction) {
				map <string, Value*> variables;
					
				// check operands															
				if(!CheckOperands(rw, ins, variables)) {
					continue;
                }
					
				// check return value
				if(rw.foundInstr.returnValue != "*") {
					if(rw.foundInstr.returnValue[0] == '<' && rw.foundInstr.returnValue[rw.foundInstr.returnValue.size() - 1] == '>') {
						variables[rw.foundInstr.returnValue] = ins;
					}
				}					

				// try to apply rule
				if(applyRule(M,*ins, rw, variables) == 1) {
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
   for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
	   	   						
	   // Do not instrument functions linked for instrumentation
	   string functionName = GetNameOfFunction(&*F);

	   if(functionName.find("__INSTR_")!=string::npos) { //TODO just starts with
		   logger.write_info("Omitting function " + functionName + " from instrumentation.");
		   continue;
	   }
	   
	   for (inst_iterator I = inst_begin(&*F), End = inst_end(&*F); I != End; ++I) {
			// Check if the instruction is relevant
		    if(!CheckInstruction(&*I, M, &*F, rw_config)) return false;
		}
	 }

  // Write instrumented module into the output file
  ofstream out_file;
  out_file.open(outputName, ofstream::out | ofstream::trunc);
  raw_os_ostream rstream(out_file);
 
  //M.print(rstream, NULL);

  WriteBitcodeToFile(&M, rstream); 
  
  return true;
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
		return 1;
	}
	
	RewriterConfig rw_config = rw.getConfig();
			
	// Get module from LLVM file
	LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
#if (LLVM_VERSION_MINOR >= 8)
    std::unique_ptr<Module> _m = parseIRFile(argv[2], Err, Context);
    Module *m = _m.release();
#else
    Module *m = ParseIRFile(argv[2], Err, Context);
#endif
    if (!m) {
		logger.write_error("Error parsing .bc file.");
        Err.print(argv[0], errs());
        return 1;
    }
    
    // Instrument
    bool resultOK = instrumentModule(*m, rw_config);
    
    if(resultOK) {
		logger.write_info("DONE.");
		return 0;
	}
	else {
		logger.write_error("FAILED.");
		return 1;
	}   
}
