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

bool Analyzer::shouldInstrument(const list<std::pair<llvm::Value*, std::string>> & rememberedValues,
                                std::vector<llvm::Value*>& rememberedPTSets, InstrPlugin* plugin,
                                const Condition &condition, const list<llvm::Value*>& parameters)
{

    string answer;

    if (condition.name == "isRemembered") {
        assert(parameters.size() == 1);
        if (!plugin->supports("pointsTo"))
            return false;

        for (auto& v : rememberedValues) {
            answer = plugin->query("pointsTo", {v.first, *(parameters.begin())});
            for (const auto& expV : condition.expectedValues)
            if (answer == expV)
                return true;
        }
        return false;
    }

    if (condition.name == "isRemembered+") {
        assert(parameters.size() == 1);

        for (auto* v : rememberedPTSets) {
            if (*(parameters.begin()) == v)
                return true;
        }
        return false;
    }

    std::vector<llvm::Value *> operands(parameters.begin(), parameters.end());

    if (plugin->supports(condition.name)) {
        answer =  plugin->query(condition.name, operands);
        for (const auto& expV : condition.expectedValues) {
            if (answer == expV)
                return true;
        }
    }

	return false;
}

