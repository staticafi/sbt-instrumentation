#include "instr_analyzer.hpp"
#include <fstream>
#include <string>
#include <iostream>

#include <llvm/Support/DynamicLibrary.h>

using namespace std;

unique_ptr<InstrPlugin> Analyzer::analyze(const string &path, llvm::Module* module) {

    if (path.empty())
		return nullptr;

    string err;
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

bool Analyzer::shouldInstrument(const list<llvm::Value*>& rememberedValues, InstrPlugin* plugin,
                                const Condition &condition, const list<llvm::Value*>& parameters) {

    string answer;

    list<llvm::Value*>::const_iterator it = parameters.begin();

	// NOTE: This needs to be changed if a query with more than two parameters is added
    // get first argument
    llvm::Value* a = *it;

    // Get second argument if present
    llvm::Value* b = nullptr;
    if (condition.arguments.size()>1){
        it++;
        b = *it;
	}

    // We are told to instrument only when
    // the value is null, so check if the value
    // can be null.
    if (condition.name == "isNull") {
        answer = plugin->isNull(a);
    } else if (condition.name == "isConstant") {
        answer = plugin->isConstant(a);
    } else if (condition.name == "isValidPointer") {
        answer = plugin->isValidPointer(a, b);
    } else if (condition.name == "isRemembered") {
        for (auto v : rememberedValues) {
            answer = plugin->pointsTo(v, a);
            for (const auto& expV : condition.expectedValues)
            if (answer == expV)
                return true;
        }
        return false;
    } else if (condition.name == "hasKnownSize") {
        answer = plugin->hasKnownSize(a);
    }

    for (const auto& expV : condition.expectedValues) {
        if (answer == expV)
            return true;
    }

	return false;
}

