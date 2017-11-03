#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Pass.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>

#if ((LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR < 5))
 #include <llvm/Analysis/Verifier.h>
#else // >= 3.5
 #include <llvm/IR/Verifier.h>
#endif

#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif

#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 5)
#include "llvm/IR/InstIterator.h"
#else
#include "llvm/Support/InstIterator.h"
#endif

#include "rewriter.hpp"
#include "instr_log.hpp"
#include "instr_analyzer.hpp"
#include "points_to_plugin.hpp"

#include "git-version.h"

using namespace llvm;
using namespace std;


typedef map<string, Value*> Variables;

// TODO extract this to a separate file
class LLVMInstrumentation {
    public:
        Module& module;
        Module& definitionsModule;
        list<unique_ptr<InstrPlugin>> plugins;
        string outputName;
        list<Value*> rememberedValues;
        Variables variables;
        Rewriter rewriter;

        LLVMInstrumentation(Module& m, Module& dm)
          : module(m), definitionsModule(dm) {}
};


Logger logger("log.txt");

void usage(char *name) {

	cerr << "Usage: " << name << " <config.json> <IR to be instrumented> <IR with definitions> <outputFileName> <options>" << endl;
}

/**
 * Get info about allocated memory to which given pointer points to.
 * @param I instruction.
 * @param ins LLVMInstrumentation object.
 * @return address and size of allocated memory to which pointer points to.
 */
std::pair<llvm::Value*, uint64_t> getPointerInfo(Instruction *I, const LLVMInstrumentation& instr){
    Value* op;
    if(const StoreInst *SI = dyn_cast<StoreInst>(I)){
        op = SI->getOperand(1);
    }
    else if(const LoadInst *LI = dyn_cast<LoadInst>(I)){
        op = LI->getOperand(0);
    }
    else {
        return std::make_pair(nullptr, 0);
    }

    for (auto& plugin : instr.plugins) {
        if(plugin->getName() == "PointsTo") {
            PointsToPlugin *pp = static_cast<PointsToPlugin*>(plugin.get());
            return pp->getPointerInfo(op);
        }
    }

    return std::make_pair(nullptr, 0);
}

/**
 * Get size of allocated type.
 * @param I instruction.
 * @param M module.
 * @return size of allocated type.
 */
uint64_t getAllocatedSize(Instruction *I, const Module& M){
    DataLayout* DL = new DataLayout(&M);

    Type* Ty;

    if(const AllocaInst *AI = dyn_cast<AllocaInst>(I)){
        Ty = AI->getAllocatedType();
    }
    else if(const StoreInst *SI = dyn_cast<StoreInst>(I)){
        Ty = SI->getOperand(0)->getType();
    }
    else if(const LoadInst *LI = dyn_cast<LoadInst>(I)){
        Ty = LI->getType();
    }
    else{
        delete DL;
        return 0;
    }

    if(!Ty->isSized()) {
        delete DL;
        return 0;
    }

    uint64_t size = DL->getTypeAllocSize(Ty);

    delete DL;

    return size;
}

/** Clone metadata from one instruction to another.
 * If i1 does not contain any metadata, then the instruction
 * that is closest to i1 is picked (we prefer the one that is after
 * and if there is none, then use the closest one before).
 *
 * @param i1 the first instruction
 * @param i2 the second instruction without any metadata
 */
void CloneMetadata(const llvm::Instruction *i1, llvm::Instruction *i2)
{
    if (i1->hasMetadata()) {
        i2->setDebugLoc(i1->getDebugLoc());
        return;
    }

    const llvm::Instruction *metadataI = nullptr;
    bool after = false;
    for (const llvm::Instruction& I : *i1->getParent()) {
        if (&I == i1) {
            after = true;
            continue;
        }

        if (I.hasMetadata()) {
            // store every "last" instruction with metadata,
            // so that in the case that we won't find anything
            // after i1, we can use metadata that are the closest
            // "before" i1
            metadataI = &I;
            if (after)
                break;
        }
    }

    assert(metadataI && "Did not find dbg in any instruction of a block");
    i2->setDebugLoc(metadataI->getDebugLoc());
}

/**
 * Returns the next instruction after the one specified.
 * @param ins specified instruction
 * @return next instruction or null, if ins is the last one.
 */
Instruction* GetNextInstruction(Instruction* ins) {
    BasicBlock::iterator I(ins);
    if (++I == ins->getParent()->end())
        return NULL;
    return &*I;
}

/**
 * Returns the previous instruction before the one specified.
 * @param ins specified instruction
 * @return previous instruction or null, if ins is the first one.
 */
Instruction* GetPreviousInstruction(Instruction* ins) {
    BasicBlock::iterator I(ins);
    if (--I == ins->getParent()->begin())
        return NULL;
    return &*I;
}

/**
 * Inserts new call instruction.
 * @param I first instruction to be removed
 * @param count number of instructions to be removed
 */
void EraseInstructions(Instruction* I, int count) {
    Instruction* currentInstr = I;
    for (int i = 0; i < count; i++) {
        if (!currentInstr){
            return;
        }
        Instruction* prevInstr = GetPreviousInstruction(currentInstr);
        currentInstr->eraseFromParent();
        currentInstr = prevInstr;
    }

}

/**
 * Inserts new call instruction.
 * @param CalleeF function to be called
 * @param args arguments of the function to be called
 * @param rw_rule relevant rewrite rule
 * @param currentInstr current instruction
 * @param Iiterator pointer to instructions iterator
 */
void InsertCallInstruction(Function* CalleeF, vector<Value *> args,
        RewriteRule rw_rule, Instruction *currentInstr,
        inst_iterator *Iiterator) {
    // Create new call instruction
    CallInst *newInstr = CallInst::Create(CalleeF, args);

    // duplicate the metadata of the instruction for which we
    // instrument the code because some passes (e.g. inliner) can
    // break the code when there's an instruction without metadata
    // when all other instructions have metadata
    CloneMetadata(currentInstr, newInstr);

    if(rw_rule.where == InstrumentPlacement::BEFORE) {
        // Insert before
        newInstr->insertBefore(currentInstr);
        logger.log_insertion("before", CalleeF, currentInstr);
    }
    else if(rw_rule.where == InstrumentPlacement::AFTER) {
        // Insert after
        newInstr->insertAfter(currentInstr);
        logger.log_insertion("after", CalleeF, currentInstr);
    }
    else if(rw_rule.where == InstrumentPlacement::REPLACE) {
        // TODO: Make the functions use the iterator instead of
        // the instruction then check this works
        // In the end we move the iterator to the newInst position
        // so we can safely remove the sequence of instructions being
        // replaced
        newInstr->insertAfter(currentInstr);
        inst_iterator helper(*Iiterator);
        *Iiterator = ++helper;
        EraseInstructions(currentInstr, rw_rule.foundInstrs.size());
        logger.log_insertion(rw_rule.foundInstrs, rw_rule.newInstr.instruction);
    } else {
        assert("Invalid position for inserting");
        abort();
    }
}

/**
 * Inserts new call instruction for globals.
 * @param CalleeF function to be called
 * @param args arguments of the function to be called
 * @param currentInstr current instruction
 */
void InsertCallInstruction(Function* CalleeF, vector<Value *> args,
        Instruction *currentInstr) {
    // Create new call instruction
    CallInst *newInstr = CallInst::Create(CalleeF, args);

    // duplicate the metadata of the instruction for which we
    // instrument the code, some passes (e.g. inliner) can
    // break the code when there's an instruction without metadata
    // when all other instructions have metadata
    if (currentInstr->hasMetadata()) {
        CloneMetadata(currentInstr, newInstr);
    } else if (const DISubprogram *DS = currentInstr->getParent()->getParent()->getSubprogram()) {
        // no metadata? then it is going to be the instrumentation
        // of alloca or such at the beggining of function,
        // so just add debug loc of the beginning of the function
        newInstr->setDebugLoc(DebugLoc::get(DS->getLine(), 0, DS));
    }

    // Insert before
    newInstr->insertBefore(currentInstr);
    logger.log_insertion("before", CalleeF, currentInstr);
}

/**
 * Inserts an argument.
 * @param rw_newInstr rewrite rule - new instruction
 * @param I instruction
 * @param CalleeF function to be called
 * @param variables map of found parameters from config
 * @param where position of the placement of the new instruction
 * @return a vector of arguments for the call that is to be inserted
 *         and a pointer to the instruction after/before the new call
 *         is going to be inserted (it is either I or some newly added
 *         argument)
 */
tuple<vector<Value *>, Instruction*> InsertArgument(InstrumentInstruction rw_newInstr, Instruction *I,
        Function* CalleeF, const Variables& variables, InstrumentPlacement where) {
    std::vector<Value *> args;
    unsigned i = 0;
    Instruction* nI = I;
    for (const string& arg : rw_newInstr.parameters) {

        if (i == rw_newInstr.parameters.size() - 1) {
            break;
        }

        auto var = variables.find(arg);
        if (var == variables.end()) {
            // NOTE: in future think also about other types than ConstantInt
            int argInt;
            try {
                argInt = stoi(arg);
                Value *intValue = ConstantInt::get(Type::getInt32Ty(I->getContext()), argInt);
                args.push_back(intValue);
            } catch (invalid_argument) {
                logger.write_error("Problem with instruction arguments: invalid argument.");
            } catch (out_of_range) {
                logger.write_error("Problem with instruction arguments: out of range.");
            }
        } else {
            unsigned argIndex = 0;
            for (Function::ArgumentListType::iterator sit=CalleeF->getArgumentList().begin(); sit != CalleeF->getArgumentList().end(); ++sit) {
                Value *argV = &*sit;

                if (i == argIndex) {
                    if (argV->getType() != var->second->getType()) {
                        //TODO other types?
                        if (!var->second->getType()->isPtrOrPtrVectorTy() && !var->second->getType()->isIntegerTy()) {
                            args.push_back(var->second);
                        } else {
                            CastInst *CastI;
                            if (var->second->getType()->isPtrOrPtrVectorTy()) {
                                CastI = CastInst::CreatePointerCast(var->second, argV->getType());
                            } else {
                                CastI = CastInst::CreateIntegerCast(var->second, argV->getType(), true); //TODO do something about signed argument
                            }

                            if (Instruction *Inst = dyn_cast<Instruction>(var->second))
                                CloneMetadata(Inst, CastI);

                            if (where == InstrumentPlacement::BEFORE) {
                                // we want to insert before I, that is:
                                // %c = cast ...
                                // newInstr
                                // I
                                //
                                // NOTE that we do not set nI in this case,
                                // so that the new instruction that we will insert
                                // is inserted before I (and after all arguments
                                // we added here)
                                CastI->insertBefore(I);
                            } else {
                                // we want to insert after I, that is:
                                // I
                                // %c = cast ...
                                // newInstr
                                //
                                // --> we must update the nI, so that the new
                                // instruction is inserted after the arguments
                                CastI->insertAfter(nI);
                                nI = CastI;
                            }
                            args.push_back(CastI);
                        }
                    } else{
                        args.push_back(var->second);
                    }
                    break;
                }

                argIndex++;
            }
        }

        i++;
    }
    return make_tuple(args,nI);
}

static llvm::Function *getOrInsertFunc(LLVMInstrumentation& I,
                                       const std::string& name) {
    llvm::Constant *cF = I.module.getFunction(name);
    if (!cF) {
        // get the function from the module with definitions
        // so that we can copy it
        Function *defF = I.definitionsModule.getFunction(name);
        if (!defF) {
            llvm::errs() << "Failed finding the definition of a function " << name << "\n";
            return nullptr;
        }

        return cast<Function>(I.module.getOrInsertFunction(name, defF->getFunctionType()));
    } else
        return cast<Function>(cF);
}

/**
 * Applies a rule.
 * @param instr instrumentation object
 * @param currentInstr current instruction
 * @param rw_rule rule to apply
 * @param variables map of found parameters form config
 * @param Iiterator pointer to instructions iterator
 * @return 1 if error
 */
int applyRule(LLVMInstrumentation& instr, Instruction *currentInstr, RewriteRule rw_rule,
        const Variables& variables, inst_iterator *Iiterator) {
    logger.write_info("Applying rule...");

    // Work just with call instructions for now...
    if(rw_rule.newInstr.instruction != "call") {
        logger.write_error("Not working with this instruction: " + rw_rule.newInstr.instruction);
        return 1;
    }

    // Get operands
    std::vector<Value *> args;

    // Get name of function
    const string& param = *(--rw_rule.newInstr.parameters.end());
    Function *CalleeF = getOrInsertFunc(instr, param);
    if (!CalleeF) {
        logger.write_error("Unknown function: " + param);
        return 1;
    }

    // Insert arguments
    tuple<vector<Value*>, Instruction*> argsTuple = InsertArgument(rw_rule.newInstr, currentInstr, CalleeF, variables, rw_rule.where);
    args = get<0>(argsTuple);

    // Insert new call instruction
    InsertCallInstruction(CalleeF, args, rw_rule, get<1>(argsTuple), Iiterator);

    return 0;
}

/**
 * Applies a rule for global variables.
 * @param instr instrumentation object
 * @param I current instruction
 * @param rw_newInstr rule to apply - new instruction
 * @param variables map of found parameters form config
 * @return 1 if error
 */
int applyRule(LLVMInstrumentation& instr, Instruction *currentInstr, InstrumentInstruction rw_newInstr,
        const Variables& variables) {
    logger.write_info("Applying rule for global variable...");

    // Work just with call instructions for now...
    if(rw_newInstr.instruction != "call") {
        logger.write_error("Not working with this instruction: " + rw_newInstr.instruction);
        return 1;
    }

    // Get operands
    std::vector<Value *> args;

    // Get name of function
    const string& param = *(--rw_newInstr.parameters.end());
    Function *CalleeF = getOrInsertFunc(instr, param);
    if (!CalleeF) {
        logger.write_error("Unknown function: " + param);
        return 1;
    }

    // Insert arguments
    tuple<vector<Value*>, Instruction*> argsTuple = InsertArgument(rw_newInstr, currentInstr, CalleeF, variables, InstrumentPlacement::BEFORE);
    args = get<0>(argsTuple);

    // Insert new call instruction
    InsertCallInstruction(CalleeF, args, get<1>(argsTuple));


    return 0;
}

/**
 * Checks if the operands of instruction match.
 * @param rwIns instruction from rewrite rule.
 * @param ins instruction to be checked.
 * @param variables map for remembering some parameters.
 * @return true if OK, false otherwise
 */
bool CheckOperands(InstrumentInstruction rwIns, Instruction* ins, Variables& variables) {
    unsigned opIndex = 0;

    for(const string& param : rwIns.parameters){
        if(rwIns.parameters.size() == 1 && param=="*"){
            return true;
        }

        if(opIndex > ins->getNumOperands() - 1) {
            return false;
        }

        llvm::Value *op = ins->getOperand(opIndex);
        if(param[0] == '<' && param[param.size() - 1] == '>') {
            if(rwIns.stripInboundsOffsets != param){
                variables[param] = op;
            } else {
                variables[param] = op->stripInBoundsOffsets();
            }
        } else if(param != "*"
                && param != (op->stripPointerCasts()->getName()).str()) {
            // NOTE: we're comparing a name of the value, but the name
            // is set only sometimes. Since we're now matching just CallInst
            // it is OK, but it may not be OK in the future
            return false;
        }

        opIndex++;
    }

    return true;
}

/**
 * Checks whether the given flag is set as they should be.
 * @param condition condition to be satisfied
 * @param rewriter rewriter
 * @return true if satisfied, false otherwise
**/
bool checkFlag(Condition condition, Rewriter rewriter) {
    string value = rewriter.getFlagValue(condition.name);
    for (const auto& expV : condition.expectedValues) {
        if(expV == value)
            return true;
    }
    return false;
}

/**
 * Runs all plugins for static analyses and decides, whether to
 * instrument or not.
 * @param instr instrumentation object
 * @param condition condition that must be satisfied to instrument
 * @param variables
 * @return true if condition is ok, false otherwise
 */
bool checkAnalysis(LLVMInstrumentation& instr, const Condition& condition, const Variables& variables){
    // condition: first element is operator, other one or two elements
    // are variables, TODO do we need more than two variables?
    if(condition.name == "")
        return true;

    list<string>::const_iterator it = condition.arguments.begin();
    string aName = *it;
    string bName = "";

    // get first argument
    Value* aValue = NULL;
    auto search = variables.find(aName);
    if(search != variables.end()) {
        aValue = search->second;
    } else {
        return true;
    }

    // get second argument
    Value* bValue = NULL;
    if(condition.arguments.size()>1){
        it++;
        bName = *it;
        auto search = variables.find(bName);
        if(search != variables.end()) {
            bValue = search->second;
        } else {
            return true;
        }
    }

    for(auto& plugin : instr.plugins){
        if(Analyzer::shouldInstrument(instr.rememberedValues, plugin.get(), condition, aValue, bValue)){
            // some plugin told us that we should instrument
            return true;
        }
    }

    // no plugin told us that we should instrument
    return false;
}

/**
 * Checks whether the conditions are satisfied for this rule.
 * @param conditions list of conditions
 * @param instr instrumentation object
 * @param variables list of variables
 * @return true if conditions are satisfied, false otherwise
 **/
bool checkConditions(const std::list<Condition>& conditions, LLVMInstrumentation& instr, const Variables& variables) {
    // check the conditions
    for (const auto& condition : conditions) {
        if(instr.rewriter.isFlag(condition.name)) {
            if(!checkFlag(condition, instr.rewriter)) {
                return false;
            }
        }
        else if(!checkAnalysis(instr, condition, variables)) {
            return false;
        }
    }
    return true;
}

/**
 * Sets flags according to the given rule.
 * @param rule instrumentation rule
 * @param rewriter rewriter
**/
void setFlags(const RewriteRule& rule, Rewriter& rewriter) {
    for (const auto& flag : rule.setFlags) {
        rewriter.setFlag(flag.first, flag.second);
    }
}

/**
 * Adds values that should be remembered into
 * instr's list.
 * @param name name of the value to be remembered
 * @param instr instrumentation object
 * @param variables list of variables
**/
void rememberValues(string name, LLVMInstrumentation& instr, Variables variables) {
    auto search = variables.find(name);
    if(search != variables.end()) {
        instr.rememberedValues.push_back(search->second);
    }
    // TODO else branch?
}

/**
 * Checks if the given instruction should be instrumented.
 * @param ins instruction to be checked.
 * @param rw_config parsed rules to apply.
 * @param Iiterator pointer to instructions iterator
 * @param instr instrumentation object
 * @return true if OK, false otherwise
 */
bool CheckInstruction(Instruction* ins, Function* F, RewriterConfig rw_config, inst_iterator *Iiterator, LLVMInstrumentation& instr) {
    // iterate through rewrite rules
    for (RewriteRule& rw : rw_config){
        // check if this rule should be applied in this function
        string functionName = F->getName().str();

        if(rw.inFunction != "*" && rw.inFunction != functionName)
            continue;

        // check sequence of instructions
        Variables variables;
        bool instrument = false;
        Instruction* currentInstr = ins;
        for (list<InstrumentInstruction>::iterator iit=rw.foundInstrs.begin(); iit != rw.foundInstrs.end(); ++iit) {
            if(currentInstr == NULL) {
                break;
            }

            InstrumentInstruction checkInstr = *iit;

            // check the name
            if(currentInstr->getOpcodeName() == checkInstr.instruction) {
                // check operands
                if(!CheckOperands(checkInstr, currentInstr, variables)) {
                    break;
                }

                // check return value
                if(checkInstr.returnValue != "*") {
                    if(checkInstr.returnValue[0] == '<'
                            && checkInstr.returnValue[checkInstr.returnValue.size() - 1] == '>') {
                        variables[checkInstr.returnValue] = currentInstr;
                    }
                }

                // load next instruction to be checked
                list<InstrumentInstruction>::iterator final_iter = rw.foundInstrs.end();
                --final_iter;
                if (iit != final_iter) {
                    currentInstr = GetNextInstruction(ins);
                }
                else {
                    instrument = true;
                }
            }
            else {
                break;
            }
        }

        if(rw.foundInstrs.size() == 1){
            InstrumentInstruction iIns = rw.foundInstrs.front();
            if(!iIns.getSizeTo.empty()){
                variables[iIns.getSizeTo] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                     getAllocatedSize(ins, instr.module));
            }

            if(iIns.getPointerInfoTo.size() == 2) {
                std::pair<llvm::Value*, uint64_t> pointerInfo = getPointerInfo(ins, instr);
                // Do not apply this rule, if there was no relevant answer from pointer analysis
                if(!pointerInfo.first) instrument = false;
                variables[iIns.getPointerInfoTo.front()] = pointerInfo.first,
                variables[iIns.getPointerInfoTo.back()] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                            pointerInfo.second);

            }
        }

        // if all instructions match and conditions are satisfied
        // try to instrument the code
        if(instrument && checkConditions(rw.conditions, instr, variables)) {
            InstrumentInstruction iIns = rw.foundInstrs.front();

            // set flags
            setFlags(rw, instr.rewriter);

            // remember values that should be remembered
            rememberValues(rw.remember, instr, variables);

            // try to apply rule
            Instruction *where;
            if(rw.where == InstrumentPlacement::BEFORE){
                where = ins;
            }
            else {
                // It is important in the REPLACE case that
                // we first place the new instruction after
                // the sequence
                where = currentInstr;
            }

            if(applyRule(instr, where, rw, variables, Iiterator) == 1) {
                logger.write_error("Cannot apply rule.");
                return false;
            }
        }
    }
    return true;
}

/**
 * Get size of global variable.
 * @param GV global variable.
 * @param M module.
 * @return size of global variable.
 */
uint64_t getGlobalVarSize(GlobalVariable* GV, const Module& M){

    DataLayout* DL = new DataLayout(&M);

    Type* Ty = GV->getType()->getElementType();

    if(!Ty->isSized())
        return 0;

    uint64_t size = DL->getTypeAllocSize(Ty);

    delete DL;

    return size;
}


/**
 * Instruments global variable if they should be instrumented.
 * @param M module.
 * @return true if instrumentation of global variables was done without problems, false otherwise
 */
bool InstrumentGlobals(LLVMInstrumentation& instr) {
    GlobalVarsRule rw_globals = instr.rewriter.getGlobalsConfig();

    // If there is no rule for global variables, do not try to instrument
    if(rw_globals.inFunction.empty() || rw_globals.globalVar.globalVariable.empty()) // TODO this is not very nice
        return true;

    // Iterate through global variables
    Module::global_iterator GI = instr.module.global_begin(), GE = instr.module.global_end();
    for ( ; GI != GE; ++GI) {
        GlobalVariable *GV = dyn_cast<GlobalVariable>(GI);
        if (!GV) continue;

        if(rw_globals.inFunction == "*"){
            //TODO
            return false;
            logger.write_error("Rule for global variables for instrumenting to all function not supported yet.");
        }
        else{
            Function* F = getOrInsertFunc(instr, rw_globals.inFunction);
            // Get operands of new instruction
            map <string, Value*> variables;

            if(rw_globals.globalVar.globalVariable != "*")
                variables[rw_globals.globalVar.globalVariable] = GV;
            if(rw_globals.globalVar.globalVariable != "*"){
                variables[rw_globals.globalVar.getSizeTo] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()), getGlobalVarSize(GV, instr.module));
            }

            // Check the conditions
            bool satisfied = true;
            for (auto condition : rw_globals.conditions) {
                if(!checkAnalysis(instr, condition, variables)) {
                    satisfied = false;
                    break;
                }
            }

            // Try to instrument the code
            if(satisfied) {
                // Try to apply rule
                inst_iterator IIterator = inst_begin(F);
                Instruction *firstI = &*IIterator; //TODO
                if(applyRule(instr, firstI, rw_globals.newInstr, variables) == 1) {
                    logger.write_error("Cannot apply rule.");
                    return false;
                }
            }
        }
    }

    return true;
}

/**
 * Instruments new insruction at the entry of the given function.
 * @param M module
 * @param F function to be instrumented
 * @param rw_config set of rules
 * @return true if instrumented, false otherwise
 */
bool InstrumentEntryPoint(LLVMInstrumentation& instr, Function* F, RewriterConfig rw_config){
    if(F->isDeclaration()) return true;
    for (RewriteRule& rw : rw_config) {

        // Check type of the rule
        if(rw.where != InstrumentPlacement::ENTRY) continue;
        // Check if the function should be instrumented
        string functionName = F->getName().str();
        if(rw.inFunction != "*" && rw.inFunction!=functionName) continue;

        // Get name of function
        const string& param = *(--rw.newInstr.parameters.end());
        Function *CalleeF = getOrInsertFunc(instr, param);
        if (!CalleeF) {
            logger.write_error("Unknown function: " + param);
            return false;
        }

        // Create new call instruction
        std::vector<Value *> args;
        CallInst *newInstr = CallInst::Create(CalleeF, args);

        //Insert at the beginning of function
        Instruction* firstInstr = (&*(F->begin()))->getFirstNonPHIOrDbg();
        if(firstInstr == NULL) continue; // TODO check this properly
        newInstr->insertBefore(firstInstr);
        CloneMetadata(firstInstr, newInstr);

        logger.write_info("Inserting instruction at the beginning of function " + functionName);
    }

    return true;
}

/**
 * Instruments new instruction before all return instructions in a given
 * function.
 * @param M module
 * @param F function to be instrumented
 * @param rw_config set fo rules
 * @return true if instrumented, false otherwise
 */
bool InstrumentReturns(LLVMInstrumentation& instr, Function* F, RewriterConfig rw_config){
    for (RewriteRule& rw : rw_config) {

        // Check type of the rule
        if(rw.where != InstrumentPlacement::RETURN) continue;
        // Check whether the function should be instrumented
        string functionName = F->getName().str();
        if(rw.inFunction != "*" && rw.inFunction!=functionName)    continue;

        // Get name of function
        const string& param = *(--rw.newInstr.parameters.end());
        Function *CalleeF = getOrInsertFunc(instr, param);
        if (!CalleeF) {
            logger.write_error("Unknown function: " + param);
            return false;
        }

        // Create new call instruction
        std::vector<Value *> args;
        CallInst *newInstr = CallInst::Create(CalleeF, args);

        bool inserted = false;
        for (auto& block : *F) {
            if (isa<ReturnInst>(block.getTerminator())) {
                llvm::Instruction *termInst = block.getTerminator();
                newInstr->insertBefore(termInst);
                inserted = true;
                CloneMetadata(termInst, newInstr);
                logger.write_info("Inserting instruction at the beginning of function " + functionName);
            }
        }

        if (!inserted) {
            F->dump();
            assert(0 && "Did not find any return");
            abort();
        }
    }
    return true;
}

/**
 * Finds out whether the given function is reachable from main.
 * @param f function
 * @param instr LLVMInstrumentation object
 * @return true if the function is reachable from main, false otherwise
*/
bool isReachable(const Function& f, LLVMInstrumentation& instr) {
    for (auto& plugin : instr.plugins) {
        if(plugin->getName() == "PointsTo") {
            PointsToPlugin *pp = static_cast<PointsToPlugin*>(plugin.get());
            Function* main = instr.module.getFunction("main");
            if(main) {
                return pp->isReachableFunction(*main, f);
            } else {
                return true;
            }
        }
    }

    return true;
}

/**
 * Runs one phase of instrumentation rules.
 * @param instr instrumentation object
 * @param phase current phase of instrumentation.
 * @return true if instrumentation was completed without problems, false otherwise
 */
bool RunPhase(LLVMInstrumentation& instr, const Phase& phase) {
    // Instrument instructions in functions
    for (Module::iterator Fiterator = instr.module.begin(), E = instr.module.end(); Fiterator != E; ++Fiterator) {
        if (Fiterator->isDeclaration())
          continue;

        // Do not instrument functions linked for instrumentation
        string functionName = (&*Fiterator)->getName().str();

        if(functionName.find("__INSTR_")!=string::npos ||
                functionName.find("__VERIFIER_")!=string::npos) { //TODO just starts with
            logger.write_info("Omitting function " + functionName + " from instrumentation.");
            continue;
        }

        // If we have info from points-to plugin, do not
        // instrument functions that are not reachable from main
      /*  if (!isReachable((*Fiterator), instr) && functionName != "main") {
            logger.write_info("Omitting function " + functionName + " from instrumentation, not reachable from main.");
            continue;
        }*/

        if(!InstrumentEntryPoint(instr, (&*Fiterator), phase.config)) return false;
        if(!InstrumentReturns(instr, (&*Fiterator), phase.config)) return false;

        for (inst_iterator Iiterator = inst_begin(&*Fiterator), End = inst_end(&*Fiterator); Iiterator != End; ++Iiterator) {
            // This iterator may be replaced (by an iterator to the following
            // instruction) in the InsertCallInstruction function
            // Check if the instruction is relevant
            if(!CheckInstruction(&*Iiterator, (&*Fiterator), phase.config, &Iiterator, instr)) return false;
        }
    }

    return true;
}

/**
 * Instruments given module with rules from json file.
 * @param instr instrumentation object
 * @return true if instrumentation was done without problems, false otherwise
 */
bool instrumentModule(LLVMInstrumentation& instr) {
    logger.write_info("Starting instrumentation.");

    Phases rw_phases = instr.rewriter.getPhases();

    for (const auto& phase : rw_phases) {
        if(!RunPhase(instr, phase))
            return false;
    }

    // Instrument global variables
    if(!InstrumentGlobals(instr)) return false;

#if ((LLVM_VERSION_MAJOR >= 4) || (LLVM_VERSION_MINOR >= 5))
    if (llvm::verifyModule(instr.module, &llvm::errs()))
#else
    if (llvm::verifyModule(instr.module, llvm::PrintMessageAction))
#endif
        return false;

    // Write instrumented module into the output file
    std::ofstream ofs(instr.outputName);
    llvm::raw_os_ostream ostream(ofs);

    // write the module
    errs() << "Saving the instrumented module to: " << instr.outputName << "\n";
    llvm::WriteBitcodeToFile(&instr.module, ostream);

    return true;
}

/**
 * Loads all plugins.
 * @param instr instrumentation object
 * @return true if plugins were succesfully loaded,
 * false, otherwise
 */
bool loadPlugins(LLVMInstrumentation& instr) {
    if(instr.rewriter.analysisPaths.size() == 0) {
        logger.write_info("No plugin specified.");
        return true;
    }

    for(const string& path : instr.rewriter.analysisPaths) {
        auto plugin = Analyzer::analyze(path, &instr.module);
        if (plugin) {
            instr.plugins.push_back(std::move(plugin));
        }
        else {
            logger.write_error("Failed loading plugin " + path);
            cout <<"Failed loading plugin: " << path << endl;
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[]) {
    if(argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", GIT_VERSION);
        return 0;
    }

    if (argc < 5) {
        usage(argv[0]);
        exit(1);
    }

    // TODO: Check for failure
    ifstream config_file;
    config_file.open(argv[1]);

    ifstream llvmir_file;
    llvmir_file.open(argv[2]);

    // Parse json file
    logger.write_info("Parsing configuration...");
    Rewriter rw;
    try {
        rw.parseConfig(config_file);
    }
    catch (runtime_error ex){
        string exceptionString = "Error parsing configuration: ";
        logger.write_error(exceptionString.append(ex.what()));
        config_file.close();
        llvmir_file.close();
        return 1;
    }

    // Get module from LLVM file
    LLVMContext Context;
    SMDiagnostic Err;
#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 6)
    std::unique_ptr<Module> module = parseIRFile(argv[2], Err, Context);
    std::unique_ptr<Module> defModule = parseIRFile(argv[3], Err, Context);
#else
    std::unique_ptr<Module> module = std::unique_ptr<Module>(ParseIRFile(argv[2], Err, Context));
    std::unique_ptr<Module> defModule = std::unique_ptr<Module>(ParseIRFile(argv[3], Err, Context));
#endif
    if (!module) {
        logger.write_error("Error parsing .bc file.");
        Err.print(argv[0], errs());
        config_file.close();
        llvmir_file.close();
        return 1;
    }

    if (!defModule) {
        logger.write_error("Error parsing .bc file with definitions.");
        Err.print(argv[0], errs());
        config_file.close();
        llvmir_file.close();
        return 1;
    }


    LLVMInstrumentation instr(*module.get(), *defModule.get());
    instr.rewriter = std::move(rw);
    instr.outputName = argv[4];

    logger.write_info("Loading plugins...");
    if (!loadPlugins(instr))
        return 1;

    // Instrument
    bool resultOK = instrumentModule(instr);

    config_file.close();
    llvmir_file.close();

    if(resultOK) {
        logger.write_info("DONE.");
        return 0;
    }
    else {
        logger.write_error("FAILED.");
        return 1;
    }
}
