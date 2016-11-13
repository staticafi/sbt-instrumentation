#include "instr_analyzer.hpp"
#include <fstream>
#include <string>
#include <dlfcn.h>
#include <iostream>

using namespace std;

unique_ptr<InstrPlugin> Analyzer::analyze(const string &path, llvm::Module* module){

	if(path.empty()){
		return NULL;
	}
	void* handle = dlopen(path.c_str(), RTLD_LAZY);

	if (!handle) {
        cerr << "Cannot open library: " << dlerror() << endl;
        return NULL;
    }

	InstrPlugin* (*create)(llvm::Module*);
	dlerror();
	create = reinterpret_cast<InstrPlugin *(*)(llvm::Module*)>(dlsym(handle, "create_object"));

    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        cerr << "Cannot load symbol 'create_object': " << dlsym_error << endl;
        dlclose(handle);
        return NULL;
    }

	unique_ptr<InstrPlugin> plugin(create(module));
	dlclose(handle);

	return plugin;
}

bool Analyzer::shouldInstrument(InstrPlugin* plugin, const string &condition, llvm::Value* a, llvm::Value* b){

	if(condition.compare("!constant")){
		return !(plugin->isConstant(a));
	}
	if(condition.compare("!null")){
		return !(plugin->isNull(a));
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



