#include "rewriter.hpp"
#include "instr_log.cpp"
#include "../include/json.h"
#include <iostream>
#include <fstream>
#include <exception>

#include "llvm/Pass.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/ReaderWriter.h"
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
#include "llvm/IR/Constants.h"
#include "llvm-c/BitWriter.h"

#include <memory>
#include <string>

#define uint unsigned int

using namespace llvm;

using namespace std;

Logger logger("log.txt");

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

int getOpcode(const string OpName) {
	
   // Terminators
   if (OpName == "ret")    return Instruction::Ret;
   if (OpName == "br")     return Instruction::Br;
   if (OpName == "switch") return Instruction::Switch;
   if (OpName == "indirectbr") return Instruction::IndirectBr;
   if (OpName == "invoke") return Instruction::Invoke;
   if (OpName == "resume") return Instruction::Resume;
   if (OpName == "unreachable") return Instruction::Unreachable;
   if (OpName == "cleanupret") return Instruction::CleanupRet;
   if (OpName == "catchret") return Instruction::CatchRet;
   if (OpName == "catchpad") return Instruction::CatchPad;
   if (OpName == "catchswitch") return Instruction::CatchSwitch;
 
   // Standard binary operators...
   if (OpName == "add")  return Instruction::Add;
   if (OpName == "fadd") return Instruction::FAdd;
   if (OpName == "sub")  return Instruction::Sub;
   if (OpName == "fsub") return Instruction::FSub;
   if (OpName == "mul")  return Instruction::Mul;
   if (OpName == "fmul") return Instruction::FMul;
   if (OpName == "udiv") return Instruction::UDiv;
   if (OpName == "sdiv") return Instruction::SDiv;
   if (OpName == "fdiv") return Instruction::FDiv;
   if (OpName == "urem") return Instruction::URem;
   if (OpName == "srem") return Instruction::SRem;
   if (OpName == "frem") return Instruction::FRem;
 
   // Logical operators...
   if (OpName == "and") return Instruction::And;
   if (OpName == "or" ) return Instruction::Or;
   if (OpName == "xor") return Instruction::Xor;
 
   // Memory instructions...
   if (OpName == "alloca") 		  return Instruction::Alloca;
   if (OpName == "load")          return Instruction::Load;
   if (OpName == "store")         return Instruction::Store;
   if (OpName == "cmpxchg") 	  return Instruction::AtomicCmpXchg;
   if (OpName == "atomicrmw")     return Instruction::AtomicRMW;
   if (OpName == "fence")         return Instruction::Fence;
   if (OpName == "getelementptr") return Instruction::GetElementPtr;
 
   // Convert instructions...
   if (OpName == "trunc")         return Instruction::Trunc;
   if (OpName == "zext")          return Instruction::ZExt;
   if (OpName == "sext")          return Instruction::SExt;
   if (OpName == "fptrunc")       return Instruction::FPTrunc;
   if (OpName == "fpext")         return Instruction::FPExt;
   if (OpName == "fptoui")        return Instruction::FPToUI;
   if (OpName == "fptosi")        return Instruction::FPToSI;
   if (OpName == "uitofp")        return Instruction::UIToFP;
   if (OpName == "sitofp")        return Instruction::SIToFP;
   if (OpName == "inttoptr")      return Instruction::IntToPtr;
   if (OpName == "ptrtoint")      return Instruction::PtrToInt;
   if (OpName == "bitcast")       return Instruction::BitCast;
   if (OpName == "addrspacecast") return Instruction::AddrSpaceCast;
   
   // Other instructions...
   if (OpName == "icmp")           return Instruction::ICmp;
   if (OpName == "fcmp")           return Instruction::FCmp;
   if (OpName == "phi")            return Instruction::PHI;
   if (OpName == "select")         return Instruction::Select;
   if (OpName == "call")           return Instruction::Call;
   if (OpName == "shl")            return Instruction::Shl;
   if (OpName == "lshr")            return Instruction::LShr;
   if (OpName == "ashr")           return Instruction::AShr;
   if (OpName == "va_arg")         return Instruction::VAArg;
   if (OpName == "extractelement") return Instruction::ExtractElement;
   if (OpName == "insertelement")  return Instruction::InsertElement;
   if (OpName == "shufflevector")  return Instruction::ShuffleVector;
   if (OpName == "extractvalue")   return Instruction::ExtractValue;
   if (OpName == "insertvalue")    return Instruction::InsertValue;
   if (OpName == "landingpad")     return Instruction::LandingPad;
   if (OpName == "cleanuppad")     return Instruction::CleanupPad;
 
   return -1;
 }

/**
 * Applies rule.
 */
int applyRule(Module &M, Instruction &I, RewriteRule rw_rule, map <string, Value*> variables) {
	logger.write_info("Applying rule...");
	cout << "applying" << endl; //TODO remove

	// Create new instruction
	// Get opcode
	/*int opCode = getOpcode(rw_rule.newInstr.instruction);
	if(opCode < 0) {
		cerr << "Unknown instruction: " << rw_rule.newInstr.instruction << endl;
		logger.write_error("Unknown instruction: " + rw_rule.newInstr.instruction);
		return 1;
	}*/
	
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
			//TODO Whoops, what now? Change config json?
		}
		else {
			LLVMContext &Context = getGlobalContext();
			Value *One = ConstantInt::get(Type::getInt32Ty(Context), 1);
			args.push_back(One);
			//args.push_back(variables[param]);
		}
		
		i++;
	}
	
	CallInst *newInst = CallInst::Create(CalleeF, args);
	   
	if(rw_rule.where == InstrumentPlacement::BEFORE) {
		logger.write_info("Inserting " + rw_rule.newInstr.instruction + " before " + rw_rule.foundInstr.instruction);
		//newInst->insertBefore(&I);
	}
	else if(rw_rule.where == InstrumentPlacement::AFTER) {
		logger.write_info("Inserting " + rw_rule.newInstr.instruction + " after " + rw_rule.foundInstr.instruction);
		//newInst->insertAfter(&I);
	}
	else if(rw_rule.where == InstrumentPlacement::REPLACE) {
		logger.write_info("Replacing " + rw_rule.foundInstr.instruction + " with " + rw_rule.newInstr.instruction);
		newInst->insertBefore(&I);
		I.eraseFromParent();
	}
	
	return 0;
}

/**
 * Instruments given module with rules from json file.
 */
bool runOnModule(Module &M, RewriterConfig rw_config) {
   for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
	   
	   // Do not instrument functions linked for instrumentation
	   string functionName = (F->getName()).str();
	   if(functionName.find("__INSTR_")!=string::npos) { //TODO just starts with
		   logger.write_info("Omitting function " + functionName + " from instrumentation.");
		   cout << "Omitting function " << functionName << " from instrumentation." << endl; // TODO remove
		   continue;
	   }
	   
	   for (inst_iterator I = inst_begin(&*F), End = inst_end(&*F); I != End; ++I) {
		    Instruction *ins = &*I; // get instruction
		    
			// iterate through rewrite rules
			for (list<RewriteRule>::iterator it=rw_config.begin(); it != rw_config.end(); ++it) {
				RewriteRule rw = *it; 
				
				// if instruction from rewrite rule is the same as current instruction
				if(ins->getOpcodeName() == rw.foundInstr.instruction) {
					
					map <string, Value*> variables;
					
					// check operands
					int opIndex = 0;
					bool apply = true;
					for (list<string>::iterator sit=rw.foundInstr.parameters.begin(); sit != rw.foundInstr.parameters.end(); ++sit) {
						string param = *sit; 
						
						if(param != "!s" && param != "!n" && param != (ins->getOperand(opIndex)->getName()).str()) {
							apply = false;
							break;
						}
						
						if(param[0] == '<' && param[param.size() - 1] == '>') {
							variables[param] = ins->getOperand(opIndex);
							cout << (ins->getOperand(1)->getName()).str() << endl;
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
						//TODO
						return false;
					}
										
					// check if instruction is a call
					//if (CallInst *ci = dyn_cast<CallInst>(ins)) {
						//Funcllvm/lib/Bitcode/Writer/BitcodeWriter.cpption *f = ci->getCalledFunction();
						//if (f == NULL) { 
							//Value* v = ci->getCalledValue();
							//f = dyn_cast<Function>(v->stripPointerCasts());
							//if (f == NULL)
							//{
								//continue; 
							//}
						//}
						//string toMatch = f->getName().data();
						//cout << toMatch << endl;
						//applyRule(*ins, rw);	

					//}
				}
					
		    }	
		}
	 }
 
  // Write instrumented module into the output file

  ofstream out_file;
  out_file.open("out.bc", ofstream::out | ofstream::trunc);
  raw_os_ostream rstream(out_file);

  llvm::WriteBitcodeToFile(&M, rstream); 
  
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
