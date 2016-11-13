#include "instr_analyzer.hpp"
#include <fstream>
#include <string>
#include <iostream>

#include <llvm/Support/DynamicLibrary.h>

using namespace std;

unique_ptr<InstrPlugin> Analyzer::analyze(const string &path, llvm::Module* module){

	if(path.empty())
		return nullptr;

    auto DL = llvm::sys::DynamicLibrary::getPermanentLibrary(path.c_str());

	if (!DL.isValid()) {
        cerr << "Cannot open library: " << path << endl;
        return nullptr;
    }

	InstrPlugin* (*create)(llvm::Module*);
    void *symbol = DL.getAddressOfSymbol("create_object");
    if (!symbol) {
        cerr << "Cannot load symbol 'create_object' from " << path << endl;
        return nullptr;
    }

	create = reinterpret_cast<InstrPlugin *(*)(llvm::Module*)>(symbol);
	unique_ptr<InstrPlugin> plugin(create(module));

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



