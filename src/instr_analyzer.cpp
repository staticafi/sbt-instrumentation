#include "instr_analyzer.hpp"
#include <fstream>
#include <string>
#include <iostream>

#include <llvm/Support/DynamicLibrary.h>

using namespace std;

unique_ptr<InstrPlugin> Analyzer::analyze(const string &path, llvm::Module* module){

    if(path.empty())
		return nullptr;

    std::string err;
    auto DL = llvm::sys::DynamicLibrary::getPermanentLibrary(path.c_str(), &err);

    if (!DL.isValid()) {
        cerr << "Cannot open library: " << path << "\n";
        cerr << err << endl;
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

// we should instrument only if the condition may not hold
bool Analyzer::shouldInstrument(const list<llvm::Value*>& rememberedValues, InstrPlugin* plugin, 
                                const string &condition, llvm::Value* a, llvm::Value* b) {

    // we are told to instrument only when
    // the value is null, so check if the value
    // can be null.
    if(condition == "null") {
        return plugin->isNull(a);
    } else if (condition == "constant") {
        return plugin->isConstant(a);
    } else if (condition == "isValidPointer") {
        return plugin->isValidPointer(a, b);
    } else if (condition == "!isValidPointer") {
        return !plugin->isValidPointer(a, b);
    } else if (condition == "isRemembered") {
        for (auto v : rememberedValues) {
            if (plugin->isEqual(v, a))
                return true;
        }
        return false;
    }

    /* TODO
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
    */

	return true;
}

