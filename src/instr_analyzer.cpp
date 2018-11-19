#include "instr_analyzer.hpp"
#include "instr_log.hpp"

#include <fstream>
#include <string>
#include <iostream>
#include <set>

#include <llvm/Support/DynamicLibrary.h>

using namespace std;

unique_ptr<InstrPlugin> Analyzer::analyze(const string &path,
                                          llvm::Module* module)
{

    if (path.empty())
		return nullptr;

    string err;
    auto DL = llvm::sys::DynamicLibrary::getPermanentLibrary(path.c_str(),
                                                             &err);

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

bool Analyzer::shouldInstrument(const RememberedValues& rememberedValues,
                                const ValuesVector& rememberedPTSets,
                                InstrPlugin* plugin,
                                const Condition &condition,
                                const ValuesVector& parameters,
                                Logger& logger)
{

    string answer;

    if (condition.name == "isRemembered") {
        assert(parameters.size() == 1);
        if (!plugin->supports("pointsTo"))
            return false;

        for (const auto& v : rememberedValues) {
            answer = plugin->query("pointsTo",
                                   {v.first, *(parameters.begin())});
            for (const auto& expV : condition.expectedValues)
            if (answer == expV)
                return true;
        }
        return false;
    }

    if (condition.name == "pointsToRemembered") {
        assert(parameters.size() == 1);
        if (!plugin->supports("pointsToSetsOverlap"))
            return false;

        for (const auto& v : rememberedValues) {
            answer = plugin->query("pointsToSetsOverlap",
                                   {v.first, *(parameters.begin())});
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

    static std::set<std::pair<InstrPlugin *, const std::string>> unsupported;
    static std::set<std::string> none_supports;
    bool issupported = false;
    if (plugin->supports(condition.name)) {
        issupported = true;
        answer =  plugin->query(condition.name, operands);
        for (const auto& expV : condition.expectedValues) {
            if (answer == expV)
                return true;
        }
    } else {
        if (unsupported.insert(std::make_pair(plugin, condition.name)).second) {
            logger.write_info("Plugin " + plugin->getName() +
                              " does not support query '" + condition.name + "'.");
        }
    }

    if (issupported == false && none_supports.insert(condition.name).second) {
        logger.write_error("No plugin supports the query '" + condition.name + "'. "
                           "Every condition with this query will be false!");
    }

	return false;
}

