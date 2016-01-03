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

		for (uint j = 0; j < json_rules[i]["from"].size(); ++j) {
			r.from.push_back(json_rules[i]["from"][j].asString());
		}
		for (uint j = 0; j < json_rules[i]["to"].size(); ++j) {
			r.to.push_back(json_rules[i]["to"][j].asString());
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

/*namespace {
  class Instrument : public ModulePass {
    public:
      static char ID;
      static RewriterConfig Config;

      Instrument() : ModulePass(ID) {}

      virtual bool runOnModule(Module &M);

    //private:
    //  void findInitFuns(Module &M);
  };
}

static RegisterPass<Instrument> INSTR("inst",
                                 "Instrument the code.");
char Instrument::ID;


bool Instrument::runOnModule(Module &M) {
   for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
	   for (inst_iterator I = inst_begin(&*F), E = inst_end(&*F); I != E;) {
			cerr << &*I << endl;
			}	
	 }
 
  return true;
}*/

/*Module *load_module(ifstream &stream, LLVMContext &context)
{
  if(!stream)
  {
    cerr << "error after open stream" << endl;
    return 0;
  }

  // load bitcode
  string ir((istreambuf_iterator<char>(stream)), (istreambuf_iterator<char>()));

  // parse it
  SMDiagnostic error;
  MemoryBufferRef mbf(MemoryBuffer::getMemBuffer(StringRef(ir))),
  Module *module = parseIR(&mbf, error, context);

  if(!module)
  {
    string what;
    raw_string_ostream os(what);
    error.print("error after ParseIR()", os);
    cerr << what;
  } // end if

  return module;
}*/


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

	RewriterConfig rw_config = parse_config(config_file);
	//Rewriter rw(rw_config);
	
	LLVMContext &Context = getGlobalContext();
    SMDiagnostic Err;
    auto Mod = parseIRFile(argv[2], Err, Context);

    if (!Mod) {
        Err.print(argv[0], errs());
        return 1;
    }




	return 0;
}
