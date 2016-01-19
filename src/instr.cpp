#include "rewriter.hpp"
#include "instr_log.cpp"
#include "../include/json.h"
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
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/SourceMgr.h>
/*
#include >llvm/Support/DiagnosticInfo.h"
#include >llvm/IR/FunctionInfo.h"
#include >llvm/Support/Endian.h"
#include "llvm/Support/MemoryBuffer.h"
*/

//#include "llvm-c/BitWriter.h"

#include <memory>
#include <string>

#define uint unsigned int

using namespace llvm;

using namespace std;

Logger logger("log.txt");

 /** 
 * Parses json file with configuration.
 * @param config_file stream with opened config file.
 * @return parsed configuration
 * @see RewriterConfig
 */
RewriterConfig parse_config(ifstream &config_file) {
	Json::Value json_rules;
	Json::Reader reader;
	bool parsingSuccessful = reader.parse(config_file, json_rules);
	if (!parsingSuccessful)	{
		cerr  << "Failed to parse configuration\n"
			  << reader.getFormattedErrorMessages();
		throw runtime_error("Config parsing failure.");
	}

	// TODO catch exceptions here

	RewriterConfig rw_config;
	for (uint i = 0; i < json_rules.size(); ++i) {
		RewriteRule r;

		// TODO make function from this
		r.foundInstr.returnValue = json_rules[i]["findInstruction"]["returnValue"].asString();
		r.foundInstr.instruction = json_rules[i]["findInstruction"]["instruction"].asString();
		for (uint j = 0; j < json_rules[i]["findInstruction"]["operands"].size(); ++j) {
			r.foundInstr.parameters.push_back(json_rules[i]["findInstruction"]["operands"][j].asString());
		}
		
		r.newInstr.returnValue = json_rules[i]["newInstruction"]["returnValue"].asString();
		r.newInstr.instruction = json_rules[i]["newInstruction"]["instruction"].asString();

		for (uint j = 0; j < json_rules[i]["newInstruction"]["operands"].size(); ++j) {
			r.newInstr.parameters.push_back(json_rules[i]["newInstruction"]["operands"][j].asString());
		}

		if (json_rules[i]["where"] == "before") { 
			r.where = InstrumentPlacement::BEFORE;
		}
		else if (json_rules[i]["where"] == "after") {
			r.where = InstrumentPlacement::AFTER;
		}
		else if (json_rules[i]["where"] == "replace") {
			r.where = InstrumentPlacement::REPLACE;
		}

		rw_config.push_back(r);
	}

	return rw_config;
}

void usage(char *name) {
	cerr << "Usage: " << name << " <config.json> <llvm IR>" << endl; // TODO
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
			string name = ci->getCalledFunction()->getName().str();
			logger.write_info("Inserting " + newCall + " " +  where + " " + foundInstrOpName + " " + name);
		}
	}
	else {
		logger.write_info("Inserting " + newCall + " " +  where + " " + foundInstrOpName);
	}
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
	cout << "Applying..." << endl; //TODO remove


	// Work just with call instructions for now...
	if(rw_rule.newInstr.instruction != "call") {
		cerr << "Not working with this instruction: " << rw_rule.newInstr.instruction << endl;
		logger.write_error("Not working with this instruction: " + rw_rule.newInstr.instruction);
		return 1;
	}
	
	// Get operands
	std::vector<Value *> args;
	int i = 0;
	Function *CalleeF = NULL;
	for (list<string>::iterator sit=rw_rule.newInstr.parameters.begin(); sit != rw_rule.newInstr.parameters.end(); ++sit) {
		string param = *sit; 
		
		if(rw_rule.newInstr.instruction == "call" && i == 0) {
			 CalleeF = M.getFunction(param);
			 if (!CalleeF) {
				cerr << "Unknown function " << param << endl;
				logger.write_error("Unknown function: " + param);	
				return 1;
			 }
			 i++;
			 continue;
		}
		
		if(variables.find(param) == variables.end()) {
			// TODO Whoops, what now? Change config json? Allow just integers?
			int paramInt;
			try {
				paramInt = stoi(param);
				LLVMContext &Context = getGlobalContext();
				Value *intValue = ConstantInt::get(Type::getInt32Ty(Context), paramInt);
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
			args.push_back(variables[param]);
		}
		
		i++;
	}
	
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

	return 0;
}


bool CheckInstruction(Instruction* ins, Module& M,RewriterConfig rw_config) {
			// iterate through rewrite rules
			for (list<RewriteRule>::iterator it=rw_config.begin(); it != rw_config.end(); ++it) {
				RewriteRule rw = *it; 
				
				// if instruction from rewrite rule is the same as current instruction
				if(ins->getOpcodeName() == rw.foundInstr.instruction) {
					map <string, Value*> variables;
					
					// check operands
					unsigned opIndex = 0;
					unsigned argIndex = 0;
					bool apply = true;
					for (list<string>::iterator sit=rw.foundInstr.parameters.begin(); sit != rw.foundInstr.parameters.end(); ++sit) {
						string param = *sit; 
						
						// Do we need arguments of called function?
						if(strcmp(ins->getOpcodeName(),"call") == 0 && ins->getNumOperands() - 1 < opIndex) {
							if (CallInst *ci = dyn_cast<CallInst>(ins)) { //TODO what if this fails
								if(ci->getNumArgOperands() > argIndex && param[0] == '<' && param[param.size() - 1] == '>') {
									variables[param] = ci->getArgOperand(argIndex);									
								}
							}
							argIndex++;
						}
						else if(param != "!s" && param != "!n" && param != (ins->getOperand(opIndex)->getName()).str()) {
							apply = false;
							break;
						}
						else if(param[0] == '<' && param[param.size() - 1] == '>') {
							variables[param] = ins->getOperand(opIndex);
						}
						opIndex++;
					}
									
					if(!apply)
						continue;
					
					// check result value
					if(rw.foundInstr.returnValue != "!n" && rw.foundInstr.returnValue != "!s") {
						if(rw.foundInstr.returnValue[0] == '<' && rw.foundInstr.returnValue[rw.foundInstr.returnValue.size() - 1] == '>') {
							variables[rw.foundInstr.returnValue] = ins;
						}
					}					

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
bool runOnModule(Module &M, RewriterConfig rw_config) {
   for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
	   
	   // Do not instrument functions linked for instrumentation
	   string functionName = GetNameOfFunction(&*F);
	   if(functionName.find("__INSTR_")!=string::npos) { //TODO just starts with
		   logger.write_info("Omitting function " + functionName + " from instrumentation.");
		   cout << "Omitting function " << functionName << " from instrumentation." << endl; // TODO remove
		   continue;
	   }
	   
	   for (inst_iterator I = inst_begin(&*F), End = inst_end(&*F); I != End; ++I) {
			// Check if the instruction is relevant
		    if(!CheckInstruction(&*I, M,rw_config)) return false;
		}
	 }
 
  // Write instrumented module into the output file

  ofstream out_file;
  out_file.open("out.ll", ofstream::out | ofstream::trunc);
  raw_os_ostream rstream(out_file);
 
  M.print(rstream, NULL);

  //llvm::WriteBitcodeToFile(&M, rstream); 
  
  return true;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	// TODO: Check for failure
	ifstream config_file;
	config_file.open(argv[1]);

	ifstream llvmir_file;
	llvmir_file.open(argv[2]);

	// Parse json file
	RewriterConfig rw_config;
	try {
		rw_config = parse_config(config_file);
	}
	catch (runtime_error ex){ //TODO exceptions in c++?
		string exceptionString = "Error parsing configuration: ";
		logger.write_error(exceptionString.append(ex.what()));
		return 1;
	}
			
	// Get module from LLVM file
	LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
    Module* m = ParseIRFile(argv[2], Err, Context);
    if (!m) {
		logger.write_error("Error parsing .bc file.");
        Err.print(argv[0], errs());
        return 1;
    }
    
    // Instrument
    bool resultOK = runOnModule(*m, rw_config);
    
    if(resultOK) {
		logger.write_info("DONE.");
		return 0;
	}
	else {
		logger.write_error("Exiting with error...");
		return 1;
	}   
}