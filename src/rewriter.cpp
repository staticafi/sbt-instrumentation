#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "rewriter.hpp"
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

    // load phases
    for (auto phase : json_rules["phases"]) {
        Phase r_phase;

        // load rewrite rules for instructions
        for (auto rule : phase["instructionRules"]) {
            RewriteRule r;

            // TODO make function from this
            // Get findInstructions
            for (auto findInstruction : rule["findInstructions"]) {
                InstrumentInstruction instr;
                instr.returnValue = findInstruction["returnValue"].asString();
                instr.instruction = findInstruction["instruction"].asString();
                for (auto operand : findInstruction["operands"]) {
                    instr.parameters.push_back(operand.asString());
                }
                instr.getSizeTo = findInstruction["getSizeTo"].asString();
                instr.stripInboundsOffsets = findInstruction["stripInboundsOffsets"].asString();
                r.foundInstrs.push_back(instr);
            }

            // Get newInstruction
            r.newInstr.returnValue = rule["newInstruction"]["returnValue"].asString();
            r.newInstr.instruction = rule["newInstruction"]["instruction"].asString();
            for (uint j = 0; j < rule["newInstruction"]["operands"].size(); ++j) {
                r.newInstr.parameters.push_back(rule["newInstruction"]["operands"][j].asString());
            }

            if (rule["where"] == "before") {
                r.where = InstrumentPlacement::BEFORE;
            }
            else if (rule["where"] == "after") {
                r.where = InstrumentPlacement::AFTER;
            }
            else if (rule["where"] == "replace") {
                r.where = InstrumentPlacement::REPLACE;
            }
            else if (rule["where"] == "return") {
                r.where = InstrumentPlacement::RETURN;
            }
            else if (rule["where"] == "entry") {
                r.where = InstrumentPlacement::ENTRY;
            }

            r.inFunction = rule["in"].asString();

            for(auto condition : rule["condition"]){
                r.condition.push_back(condition.asString());
            }

            r_phase.config.push_back(r);
        }
        this->phases.push_back(r_phase);
    }

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

const Phases& Rewriter::getPhases() {
	return this->phases;
}

const GlobalVarsRule& Rewriter::getGlobalsConfig() {
	return this->globalVarsRule;
}
