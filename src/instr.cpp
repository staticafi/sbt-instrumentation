#include "rewriter.hpp"
#include "../include/json/json.h"
#include <iostream>
#include <fstream>
#include <exception>

#include "llvm/IR/DataLayout.h"
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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

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
		if (json_rules[i]["where"] == "after") {
			r.where = InstrumentPlacement::AFTER;
		}
		if (json_rules[i]["where"] == "replace") {
			r.where = InstrumentPlacement::REPLACE;
		}

		rw_config.push_back(r);
	}

	return rw_config;
}

void usage(char *name) {
	cerr << "Usage: " << name << " <config.json> <llvm IR>" << endl; // TODO
}

namespace {
  class Instrument : public ModulePass {
    public:
      static char ID;

      Instrument() : ModulePass(ID) {}

      virtual bool runOnModule(Module &M);

    //private:
    //  void findInitFuns(Module &M);
  };
}

static RegisterPass<Instrument> INSTR("instrument",
                                 "Instrument the code.");
char Instrument::ID;


bool Instrument::runOnModule(Module &M) {
  
 
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

	RewriterConfig rw_config = parse_config(config_file);
	//Rewriter rw(rw_config);


	return 0;
}
