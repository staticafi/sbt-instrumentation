#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "../lib/rewriter.hpp"
#include "json/json.h"

using namespace std;

void Rewriter::parseConfig(ifstream &config_file) {
	Json::Value json_rules;
	Json::Reader reader;
	bool parsingSuccessful = reader.parse(config_file, json_rules);
	if (!parsingSuccessful)	{
		cerr  << "Failed to parse configuration\n"
			  << reader.getFormattedErrorMessages();
		throw runtime_error("Config parsing failure.");
	}

	// TODO catch exceptions here

	RewriterConfig rw_config;
	for (uint i = 0; i < json_rules["rules"].size(); ++i) {
		RewriteRule r;

		// TODO make function from this
		for (uint k = 0; k < json_rules["rules"][i]["findInstructions"].size(); ++k) {
			InstrumentInstruction instr;
			instr.returnValue = json_rules["rules"][i]["findInstructions"][k]["returnValue"].asString();
			instr.instruction = json_rules["rules"][i]["findInstructions"][k]["instruction"].asString();
			for (uint j = 0; j < json_rules["rules"][i]["findInstructions"][k]["operands"].size(); ++j) {
				instr.parameters.push_back(json_rules["rules"][i]["findInstructions"][k]["operands"][j].asString());
			}
			r.foundInstrs.push_back(instr);
		}



		r.newInstr.returnValue = json_rules["rules"][i]["newInstruction"]["returnValue"].asString();
		r.newInstr.instruction = json_rules["rules"][i]["newInstruction"]["instruction"].asString();
		for (uint j = 0; j < json_rules["rules"][i]["newInstruction"]["operands"].size(); ++j) {
			r.newInstr.parameters.push_back(json_rules["rules"][i]["newInstruction"]["operands"][j].asString());
		}

		if (json_rules["rules"][i]["where"] == "before") {
			r.where = InstrumentPlacement::BEFORE;
		}
		else if (json_rules["rules"][i]["where"] == "after") {
			r.where = InstrumentPlacement::AFTER;
		}
		else if (json_rules["rules"][i]["where"] == "replace") {
			r.where = InstrumentPlacement::REPLACE;
		}

		r.inFunction = json_rules["rules"][i]["in"].asString();

		r.analysisPath = json_rules["rules"][i]["analysis"].asString();

		rw_config.push_back(r);
	}

	this->config = rw_config;
}

RewriterConfig Rewriter::getConfig() {
	return this->config;
}





