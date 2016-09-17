#include "../lib/instr_analyzer.hpp"
#include "../lib/instr_plugin.hpp"
#include <fstream>
#include <string>
#include <dlfcn.h>

using namespace std;

bool Analyzer::shouldInstrument(const string &path, llvm::Module* module){

	if(path.empty()){
		return true;
	}

	void* handle = dlopen(path.c_str(), RTLD_LAZY);

	InstrPlugin* (*create)();
	void (*destroy)(InstrPlugin*);

	create = (InstrPlugin* (*)())dlsym(handle, "create_object");
	destroy = (void (*)(InstrPlugin*))dlsym(handle, "destroy_object");

	InstrPlugin* plugin = (InstrPlugin*)create();
	bool analyze = plugin->Analyze(module);
	destroy(plugin);

	return analyze;
}



