#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "rewriter.hpp"
#include "json/json.h"

using namespace std;

void parseConditions(const Json::Value& conditions, std::list<Condition>& r_conditions) {
    for (const auto& condition : conditions){
        Condition r_condition;
        r_condition.name = condition["query"][0].asString();

        for (uint i = 1; i < condition["query"].size(); i++) {
            r_condition.arguments.push_back(condition["query"][i].asString());
        }

        for (uint i = 0; i < condition["expectedResult"].size(); i++) {
            r_condition.expectedValues.push_back(condition["expectedResult"][i].asString());
        }

        r_conditions.push_back(r_condition);
    }
}

BinOpType getType(const std::string& type) {
    if (type == "i16")
        return BinOpType::INT16;
    if (type == "i32")
        return BinOpType::INT32;
    if (type == "i64")
        return BinOpType::INT64;
    if (type == "i8")
        return BinOpType::INT8;

    return BinOpType::NBOP;
}

void parseRule(const Json::Value& rule, RewriteRule& r) {
    // Get findInstructions
    for (const auto& findInstruction : rule["findInstructions"]) {
        InstrumentInstruction instr;
        instr.returnValue = findInstruction["returnValue"].asString();
        instr.instruction = findInstruction["instruction"].asString();
        for (const auto& operand : findInstruction["operands"]) {
            instr.parameters.push_back(operand.asString());
        }
        instr.getSizeTo = findInstruction["getTypeSize"].asString();
        instr.type = getType(findInstruction["type"].asString());
        instr.getDestType = findInstruction["getDestType"].asString();

        for (const auto& info : findInstruction["getPointerInfo"]) {
            instr.getPointerInfoTo.push_back(info.asString());
        }

        instr.stripInboundsOffsets = findInstruction["stripInboundsOffsets"].asString();
        r.foundInstrs.push_back(instr);
    }

    // Get newInstruction
    r.newInstr.returnValue = rule["newInstruction"]["returnValue"].asString();
    r.newInstr.instruction = rule["newInstruction"]["instruction"].asString();
    for (const auto& op : rule["newInstruction"]["operands"]) {
        r.newInstr.parameters.push_back(op.asString());
    }

    // Get placement, in and remember field
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

    // Get conditions
    parseConditions(rule["conditions"], r.conditions);

    for (const auto& setFlag : rule["setFlags"]) {
        r.setFlags.insert(Flag(setFlag[0].asString(), setFlag[1].asString()));
    }
}

void parsePhase(const Json::Value& phase, Phase& r_phase) {
    // Load rewrite rules for instructions
    for (const auto& rule : phase["instructionRules"]) {
        RewriteRule rw_rule;
        parseRule(rule, rw_rule);
        r_phase.config.push_back(rw_rule);
    }
}

void parseGlobalRule(const Json::Value& globalRule, GlobalVarsRule& rw_globals_rule) {
    // Get rule for global variables
    rw_globals_rule.globalVar.globalVariable = globalRule["findGlobals"]["globalVariable"].asString();
    rw_globals_rule.globalVar.getSizeTo = globalRule["findGlobals"]["getTypeSize"].asString();

    // Get conditions
    parseConditions(globalRule["conditions"], rw_globals_rule.conditions);

    rw_globals_rule.newInstr.returnValue = globalRule["newInstruction"]["returnValue"].asString();
    rw_globals_rule.newInstr.instruction = globalRule["newInstruction"]["instruction"].asString();

    for (auto operand : globalRule["newInstruction"]["operands"]) {
        rw_globals_rule.newInstr.parameters.push_back(operand.asString());
    }

    rw_globals_rule.inFunction = globalRule["in"].asString();
}

void Rewriter::parseConfig(ifstream &config_file) {
    Json::Value json_rules;
    bool parsingSuccessful;

#if (JSONCPP_VERSION_MINOR < 8 || (JSONCPP_VERSION_MINOR == 8 && JSONCPP_VERSION_PATCH < 1))
    Json::Reader reader;
    parsingSuccessful = reader.parse(config_file, json_rules);
    if (!parsingSuccessful) {
        cerr  << "Failed to parse configuration\n"
              << reader.getFormattedErrorMessages();
        throw runtime_error("Config parsing failure.");
    }

#else
    Json::CharReaderBuilder rbuilder;
    rbuilder["collectComments"] = false;
    std::string errs;
    parsingSuccessful = Json::parseFromStream(rbuilder, config_file, &json_rules, &errs);
    if (!parsingSuccessful) {
        cerr  << "Failed to parse configuration\n"
              << errs;
        throw runtime_error("Config parsing failure.");
    }

#endif

    // Load paths to analyses
    for (const auto& analysis : json_rules["analyses"]) {
        this->analysisPaths.push_back(analysis.asString());
    }

    // Load flags
    for (const auto& flag : json_rules["flags"]) {
        this->flags.insert(Flag(flag.asString(), ""));
    }

    // Load phases
    for (const auto& phase : json_rules["phases"]) {
        Phase rw_phase;
        parsePhase(phase, rw_phase);
        this->phases.push_back(rw_phase);
    }

    for (const auto& globalRule : json_rules["globalVariablesRules"]) {
        GlobalVarsRule rw_globals_rule;
        parseGlobalRule(globalRule, rw_globals_rule);
        this->globalVarsRules.push_back(rw_globals_rule);
    }
}

const Phases& Rewriter::getPhases() {
    return this->phases;
}

const GlobalVarsRules& Rewriter::getGlobalsConfig() {
    return this->globalVarsRules;
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

