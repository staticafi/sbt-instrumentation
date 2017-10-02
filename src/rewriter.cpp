#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "rewriter.hpp"
#include "json/json.h"

using namespace std;

void parseRule(const Json::Value& rule, RewriteRule& r) {
    // Get findInstructions 
    for (const auto& findInstruction : rule["findInstructions"]) {
        InstrumentInstruction instr;
        instr.returnValue = findInstruction["returnValue"].asString();
        instr.instruction = findInstruction["instruction"].asString();
        for (const auto& operand : findInstruction["operands"]) {
            instr.parameters.push_back(operand.asString());
        }
        instr.getSizeTo = findInstruction["getSizeTo"].asString();
        instr.stripInboundsOffsets = findInstruction["stripInboundsOffsets"].asString();
        r.foundInstrs.push_back(instr);
    }

    // Get newInstruction
    r.newInstr.returnValue = rule["newInstruction"]["returnValue"].asString();
    r.newInstr.instruction = rule["newInstruction"]["instruction"].asString();
    for (const auto& op : rule["newInstruction"]["operands"]) {
        r.newInstr.parameters.push_back(op.asString());
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
    r.remember = rule["remember"].asString();

    // TODO extract function
    for (const auto& condition : rule["conditions"]){
        Condition r_condition;
        r_condition.name = condition[0].asString();
        for (uint i = 1; i < condition.size(); i++) {
            r_condition.arguments.push_back(condition[i].asString());
        }
        r.conditions.push_back(r_condition);
    }

    for (const auto& setFlag : rule["setFlags"]){
        r.setFlags.insert(Flag(setFlag[0].asString(), setFlag[1].asString()));
    }
}

void parsePhase(const Json::Value& phase, Phase& r_phase) {
    // load rewrite rules for instructions
    for (const auto& rule : phase["instructionRules"]) {
        RewriteRule rw_rule;
        parseRule(rule, rw_rule);
        r_phase.config.push_back(rw_rule);
    }
}

void Rewriter::parseConfig(ifstream &config_file) {
    Json::Value json_rules;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(config_file, json_rules);
    if (!parsingSuccessful) {
        cerr  << "Failed to parse configuration\n"
              << reader.getFormattedErrorMessages();
        throw runtime_error("Config parsing failure.");
    }

    // TODO catch exceptions here

    // load paths to analyses
    for(const auto& analysis : json_rules["analyses"]){
        this->analysisPaths.push_back(analysis.asString());
    }

    // load flags
    for (const auto& flag : json_rules["flags"]) {
        this->flags.insert(Flag(flag.asString(), ""));
    }
    
    // load phases
    for (const auto& phase : json_rules["phases"]) {
        Phase rw_phase;
        parsePhase(phase, rw_phase);
        this->phases.push_back(rw_phase);
    }

    GlobalVarsRule rw_globals_rule;

    // Get rule for global variables
    rw_globals_rule.globalVar.globalVariable = json_rules["globalVariablesRule"]["findGlobals"]["globalVariable"].asString();
    rw_globals_rule.globalVar.getSizeTo = json_rules["globalVariablesRule"]["findGlobals"]["getSizeTo"].asString();

    for (auto condition : json_rules["globalVariablesRule"]["conditions"]){
        Condition r_condition;
        r_condition.name = condition[0].asString();
        for (uint i = 1; i < condition.size(); i++) {
            r_condition.arguments.push_back(condition[i].asString());
        }
        rw_globals_rule.conditions.push_back(r_condition);
    }

    rw_globals_rule.newInstr.returnValue = json_rules["globalVariablesRule"]["newInstruction"]["returnValue"].asString();
    rw_globals_rule.newInstr.instruction = json_rules["globalVariablesRule"]["newInstruction"]["instruction"].asString();

    for (auto operand : json_rules["globalVariablesRule"]["newInstruction"]["operands"]) {
        rw_globals_rule.newInstr.parameters.push_back(operand.asString());
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

bool Rewriter::isFlag(string name) {
    auto search = this->flags.find(name);
    return search != this->flags.end();
}

void Rewriter::setFlag(string name, string value) {
    auto search = this->flags.find(name); 
    if (search != this->flags.end())
            search->second = value;
}

string Rewriter::getFlagValue(string name) {
    auto search = this->flags.find(name);
    if (search != this->flags.end())
        return search->second;

    return "";
}

