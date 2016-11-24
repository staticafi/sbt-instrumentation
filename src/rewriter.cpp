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
	
	// load paths to analyses
	for(uint i = 0; i < json_rules["analyses"].size(); ++i){
		this->analysisPaths.push_back(json_rules["analyses"][i].asString());
	}

	RewriterConfig rw_config;

	// load rewrite rules for instructions
	for (uint i = 0; i < json_rules["instructionRules"].size(); ++i) {
		RewriteRule r;

		// TODO make function from this
		// Get findInstructions
		for (uint k = 0; k < json_rules["instructionRules"][i]["findInstructions"].size(); ++k) {
			InstrumentInstruction instr;
			instr.returnValue = json_rules["instructionRules"][i]["findInstructions"][k]["returnValue"].asString();
			instr.instruction = json_rules["instructionRules"][i]["findInstructions"][k]["instruction"].asString();
			for (uint j = 0; j < json_rules["instructionRules"][i]["findInstructions"][k]["operands"].size(); ++j) {
				instr.parameters.push_back(json_rules["instructionRules"][i]["findInstructions"][k]["operands"][j].asString());
			}
			instr.getSizeTo = json_rules["instructionRules"][i]["findInstructions"][k]["getSizeTo"].asString();
			instr.stripInboundsOffsets = json_rules["instructionRules"][i]["findInstructions"][k]["stripInboundsOffsets"].asString();
			r.foundInstrs.push_back(instr);
		}

		// Get newInstruction
		r.newInstr.returnValue = json_rules["instructionRules"][i]["newInstruction"]["returnValue"].asString();
		r.newInstr.instruction = json_rules["instructionRules"][i]["newInstruction"]["instruction"].asString();
		for (uint j = 0; j < json_rules["instructionRules"][i]["newInstruction"]["operands"].size(); ++j) {
			r.newInstr.parameters.push_back(json_rules["instructionRules"][i]["newInstruction"]["operands"][j].asString());
		}

		if (json_rules["instructionRules"][i]["where"] == "before") {
			r.where = InstrumentPlacement::BEFORE;
		}
		else if (json_rules["instructionRules"][i]["where"] == "after") {
			r.where = InstrumentPlacement::AFTER;
		}
		else if (json_rules["instructionRules"][i]["where"] == "replace") {
			r.where = InstrumentPlacement::REPLACE;
		}

		r.inFunction = json_rules["instructionRules"][i]["in"].asString();

		for(uint j = 0; j < json_rules["instructionRules"][i]["condition"].size(); ++j){
			r.condition.push_back(json_rules["instructionRules"][i]["condition"][j].asString());
		}

		rw_config.push_back(r);
	}

	this->config = rw_config;
	
	GlobalVarsRule rw_globals_rule;
	
	// Get rule for global variables
	rw_globals_rule.globalVar.globalVariable = json_rules["globalVariablesRule"]["findGlobals"]["globalVariable"].asString();
	rw_globals_rule.globalVar.getSizeTo = json_rules["globalVariablesRule"]["findGlobals"]["getSizeTo"].asString();
	
	for(uint j = 0; j < json_rules["globalVariablesRule"]["condition"].size(); ++j){
		rw_globals_rule.condition.push_back(json_rules["globalVariablesRule"]["condition"][j].asString());
	}
	
	rw_globals_rule.newInstr.returnValue = json_rules["globalVariablesRule"]["newInstruction"]["returnValue"].asString();
	rw_globals_rule.newInstr.instruction = json_rules["globalVariablesRule"]["newInstruction"]["instruction"].asString();
	
	for (uint j = 0; j < json_rules["globalVariablesRule"]["newInstruction"]["operands"].size(); ++j) {
		rw_globals_rule.newInstr.parameters.push_back(json_rules["globalVariablesRule"]["newInstruction"]["operands"][j].asString());
	}
	
	rw_globals_rule.inFunction = json_rules["globalVariablesRule"]["in"].asString();
	
	this->globalVarsRule = rw_globals_rule;
}

RewriterConfig Rewriter::getConfig() {
	return this->config;
}

GlobalVarsRule Rewriter::getGlobalsConfig() {
	return this->globalVarsRule;
}






