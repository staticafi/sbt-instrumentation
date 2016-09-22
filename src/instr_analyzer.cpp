#include "../lib/instr_analyzer.hpp"
#include <fstream>
#include <string>
#include <dlfcn.h>


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

bool Analyzer::shouldInstrument(InstrPlugin* plugin, const string &condition, llvm::Value* a, llvm::Value* b){

	if(condition.compare("!constant")){
		return !(plugin->isConstant(a));
	}

	if(condition.compare("!=")){
		return !(plugin->isEqual(a,b));
	} else if(condition.compare("==")){
		return !(plugin->isNotEqual(a,b));
	} else if(condition.compare("<")){
		return !(plugin->greaterOrEqual(a,b));
	} else if(condition.compare(">")){
		return !(plugin->lessOrEqual(a,b));
	} else if(condition.compare("<=")){
		return !(plugin->greaterThan(a,b));
	} else if(condition.compare(">=")){
		return !(plugin->lessThan(a,b));
	}

	return true;
}



