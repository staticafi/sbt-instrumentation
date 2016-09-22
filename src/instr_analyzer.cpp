#include "../lib/instr_analyzer.hpp"
#include <fstream>
#include <string>
#include <dlfcn.h>

#include <llvm/IR/Module.h>

using namespace std;

InstrPlugin* Analyzer::analyze(const string &path, llvm::Module* module){

	if(path.empty()){
		return NULL;
	}

	void* handle = dlopen(path.c_str(), RTLD_LAZY);

	InstrPlugin* (*create)(llvm::Module* module);
	void (*destroy)(InstrPlugin*);

	create = (InstrPlugin* (*)(llvm::Module*))dlsym(handle, "create_object");
	//destroy = (void (*)(InstrPlugin*))dlsym(handle, "destroy_object");

	InstrPlugin* plugin = (InstrPlugin*)create(module);

	return plugin;
}



