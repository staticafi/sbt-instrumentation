#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "../lib/rewriter.hpp"
#include "../include/json.h"

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
		r.foundInstr.returnValue = json_rules["rules"][i]["findInstruction"]["returnValue"].asString();
		r.foundInstr.instruction = json_rules["rules"][i]["findInstruction"]["instruction"].asString();
		for (uint j = 0; j < json_rules["rules"][i]["findInstruction"]["operands"].size(); ++j) {
			r.foundInstr.parameters.push_back(json_rules["rules"][i]["findInstruction"]["operands"][j].asString());
		}
		for (uint j = 0; j < json_rules["rules"][i]["findInstruction"]["arguments"].size(); ++j) {
			r.foundInstr.arguments.push_back(json_rules["rules"][i]["findInstruction"]["arguments"][j].asString());
		}
		
		r.newInstr.returnValue = json_rules["rules"][i]["newInstruction"]["returnValue"].asString();
		r.newInstr.instruction = json_rules["rules"][i]["newInstruction"]["instruction"].asString();
		for (uint j = 0; j < json_rules["rules"][i]["newInstruction"]["operands"].size(); ++j) {
			r.newInstr.parameters.push_back(json_rules["rules"][i]["newInstruction"]["operands"][j].asString());
		}		
		for (uint j = 0; j < json_rules["rules"][i]["newInstruction"]["arguments"].size(); ++j) {
			r.newInstr.arguments.push_back(json_rules["rules"][i]["newInstruction"]["arguments"][j].asString());
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

		rw_config.push_back(r);
	}

	this->config = rw_config;
	this->cFile = json_rules["file"].asString();
}

std::string Rewriter::CFileName() {
	return this->cFile;
}

RewriterConfig Rewriter::getConfig() {
	return this->config;
}





