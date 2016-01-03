#include "rewriter.hpp"
#include "../include/json.h"
#include <iostream>
#include <fstream>
#include <exception>

/* #include "llvm/IR/DataLayout.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h" */

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/FunctionInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Instruction.h"
#include <memory>
#include <string>

#define uint unsigned int

using namespace llvm;

using namespace std;

/** 
  Parses json file with configuration.
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
 * Applies rule.
 */
void applyRule(Instruction &I, RewriteRule rw_rule) {
	cout << "applying" << endl; //TODO
	
	// create new instruction
	   
	if(rw_rule.where == InstrumentPlacement::BEFORE) {
		//TODO insert before
	}
	else if(rw_rule.where == InstrumentPlacement::AFTER) {
		//TODO insert after
	}
	else if(rw_rule.where == InstrumentPlacement::REPLACE) {
		//TODO replace
	}

}

/**
 * Instruments given module with rules from json file.
 */
bool runOnModule(Module &M, RewriterConfig rw_config) {
   for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
	   for (inst_iterator I = inst_begin(&*F), End = inst_end(&*F); I != End; ++I) {
		    Instruction *ins = &*I; // get instruction
	
			// iterate through rewrite rules
			for (list<RewriteRule>::iterator it=rw_config.begin(); it != rw_config.end(); ++it) {
				RewriteRule rw = *it; 
				
				// if instruction from rewrite rule is the same as current instruction
				if(ins->getOpcodeName() == rw.foundInstr.instruction) {
					
					cout<<(ins->getOperand(0)->getName()).str()<<endl;
					map <string, Value*> variables;
					
					if(rw.foundInstr.returnValue != "%n" && rw.foundInstr.returnValue != "%s") {
						if(rw.foundInstr.returnValue[0] == '<' && rw.foundInstr.returnValue[rw.foundInstr.returnValue.size() - 1] == '>') {
							variables[rw.foundInstr.returnValue] = ins;
						}
					}
					
					// TODO check parameters
					for (list<string>::iterator sit=rw.foundInstr.parameters.begin(); sit != rw.foundInstr.parameters.end(); ++sit) {
						string param = *sit; 
						if(param[0] == '<' && param[param.size() - 1] == '>') {
							//variables[param] = ins->getOperand(i);
						}
					}
					
					//applyRule(*ins, rw);
					// check if instruction is a call
					if (CallInst *ci = dyn_cast<CallInst>(ins)) {
						Function *f = ci->getCalledFunction();
						if (f == NULL) { 
							Value* v = ci->getCalledValue();
							f = dyn_cast<Function>(v->stripPointerCasts());
							if (f == NULL)
							{
								continue; 
							}
						}
						string toMatch = f->getName().data();
						applyRule(*ins, rw);	
						cout << ins->getNumOperands() << endl;
						cout << (ins->getOperand(0)->getName()).str() << endl;
						cout << (ins->getOperand(1)->getName()).str() << endl;
					}
				}
					
		    }	
		}
	 }
 
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
	RewriterConfig rw_config = parse_config(config_file);
	//Rewriter rw(rw_config);
	
	// Get module from LLVM file
	LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
    Module* m = parseIRFile(argv[2], Err, Context).release();

    if (!m) {
        Err.print(argv[0], errs());
        return 1;
    }
    
    // Instrument
    bool resultOK = runOnModule(*m, rw_config);
    
    //TODO fail
    if(resultOK) {
		return 0;
	}
	else {
		return 1;
	}
    
}
