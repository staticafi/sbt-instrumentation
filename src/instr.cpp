#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <set>
#include <tuple>

#include <llvm/IR/Constants.h>
#include "llvm/Linker/Linker.h"
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

#include "llvm_instr.hpp"
#include "rewriter.hpp"
#include "instr_log.hpp"
#include "instr_analyzer.hpp"
#include "points_to_plugin.hpp"

#include "git-version.h"

using namespace llvm;
using namespace std;

/* Gather statistics about instrumentation. */
struct Statistics {
    std::map<const llvm::Function *, unsigned> inserted_calls;
    std::map<const std::string, unsigned> suppresed_instr;
} statistics;

Logger logger("log.txt");

void usage(char *name) {
    cerr << "Usage: " << name << " <config.json> <IR to be instrumented> <IR with definitions> <outputFileName> <options>" << endl;
    cerr << "Options:" << endl;
    cerr << "--version     Prints the git version." << endl;
    cerr << "--no-linking  Disables linking of definitions of instrumentation functions." << endl;
}

/**
 * Gets points to plugin if available.
 * @param instr LLVMInstrumentation object
 */
void getPointsToPlugin(LLVMInstrumentation& instr) {
    for (auto& plugin : instr.plugins) {
        if (plugin->getName() == "PointsTo") {
            instr.ppPlugin = static_cast<PointsToPlugin*>(plugin.get());
        }
    }
}

/**
 * Get info about minimum and maximum allocated memory to which given pointer points to.
 * @param I instruction.
 * @param ins LLVMInstrumentation object.
 * @return pointer, offset and size of allocated memory to which pointer points to.
 */
std::tuple<llvm::Value*, uint64_t, uint64_t, uint64_t, uint64_t>
    getPointerInfoMinMax(Instruction *I, LLVMInstrumentation& instr)
{
    Value* op;
    if (const StoreInst *SI = dyn_cast<StoreInst>(I)) {
        op = SI->getOperand(1);
    }
    else if (const LoadInst *LI = dyn_cast<LoadInst>(I)) {
        op = LI->getOperand(0);
    }
    else {
        return std::make_tuple(nullptr, 0, 0, 0, 0);
    }

    if (instr.ppPlugin)
        return instr.ppPlugin->getPInfoMinMax(op, instr.rememberedPTSets);

    return std::make_tuple(nullptr, 0, 0, 0, 0);
}

/**
 * Get info about allocated memory to which given pointer points to.
 * @param I instruction.
 * @param ins LLVMInstrumentation object.
 * @param min get info for the object with minimal space left (size - offset) and minimal offset
 * @return pointer, offset and size of allocated memory to which pointer points to.
 */
std::tuple<llvm::Value*, uint64_t, uint64_t> getPointerInfo(Instruction *I, const LLVMInstrumentation& instr,
                                                            bool min = false)
{
    Value* op;
    if (const StoreInst *SI = dyn_cast<StoreInst>(I)) {
        op = SI->getOperand(1);
    }
    else if (const LoadInst *LI = dyn_cast<LoadInst>(I)) {
        op = LI->getOperand(0);
    }
    else {
        return std::make_tuple(nullptr, 0, 0);
    }

    if (instr.ppPlugin && !min) {
        return instr.ppPlugin->getPointerInfo(op);
    } else if (instr.ppPlugin) {
        return instr.ppPlugin->getPInfoMin(op);
    }

    return std::make_tuple(nullptr, 0, 0);
}

/**
 * Get size of allocated type.
 * @param I instruction.
 * @param M module.
 * @return size of allocated type.
 */
uint64_t getAllocatedSize(Instruction *I, const Module& M) {
    DataLayout* DL = new DataLayout(&M);

    Type* Ty;

    if (const AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
        Ty = AI->getAllocatedType();
    }
    else if (const StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Ty = SI->getOperand(0)->getType();
    }
    else if (const LoadInst *LI = dyn_cast<LoadInst>(I)) {
        Ty = LI->getType();
    }
    else {
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

/**
 * Get information corresponding to getPointerInfo* fields.
 * @param variables map of config variables
 * @param iIns instruction rule
 * @param I instruction.
 * @param ins LLVMInstrumentation object.
 * @return true if ok, false for instrumentation fail
 */
bool getPointerInfos(Variables& variables, const InstrumentInstruction& iIns,
                        Instruction *ins, LLVMInstrumentation& instr)
{
    if (iIns.getPointerInfoTo.size() == 3) {
        std::tuple<llvm::Value*, uint64_t, uint64_t> pointerInfo = getPointerInfo(ins, instr);
        // Do not apply this rule, if there was no relevant answer from pointer analysis
        if (!std::get<0>(pointerInfo))
             return false;
        variables[iIns.getPointerInfoTo.front()] = std::get<0>(pointerInfo);
        variables[*(std::next(iIns.getPointerInfoTo.begin(), 1))] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                        std::get<1>(pointerInfo));

        variables[iIns.getPointerInfoTo.back()] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                            std::get<2>(pointerInfo));
     }

    if (iIns.getPointerInfoMinTo.size() == 3) {
        std::tuple<llvm::Value*, uint64_t, uint64_t> pointerInfo = getPointerInfo(ins, instr, true);
        // Do not apply this rule, if there was no relevant answer from pointer analysis
        if (!std::get<0>(pointerInfo))
            return false;
        variables[iIns.getPointerInfoMinTo.front()] = std::get<0>(pointerInfo);
        variables[*(std::next(iIns.getPointerInfoMinTo.begin(), 1))] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                           std::get<1>(pointerInfo));
        variables[iIns.getPointerInfoMinTo.back()] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                            std::get<2>(pointerInfo));
    }

    if (iIns.getPInfoMinMaxTo.size() == 5) {
        std::tuple<llvm::Value*, uint64_t, uint64_t, uint64_t, uint64_t> pointerInfo = getPointerInfoMinMax(ins, instr);
        // Do not apply this rule, if there was no relevant answer from pointer analysis
        if (!std::get<0>(pointerInfo))
            return false;
        variables[iIns.getPInfoMinMaxTo.front()] = std::get<0>(pointerInfo);
        variables[*(std::next(iIns.getPInfoMinMaxTo.begin(), 1))] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                           std::get<1>(pointerInfo));
        variables[*(std::next(iIns.getPInfoMinMaxTo.begin(), 2))] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                           std::get<2>(pointerInfo));
        variables[*(std::next(iIns.getPInfoMinMaxTo.begin(), 3))] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                           std::get<3>(pointerInfo));
        variables[iIns.getPInfoMinMaxTo.back()] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                            std::get<4>(pointerInfo));
    }


    return true;
}

/**
 * Gets destination integer type of cast instruction.
 * @param I cast instruction
 * @return integer type
*/
int getDestType(const llvm::Instruction *I) {
    auto* castInst = dyn_cast<CastInst>(I);
    if (!castInst)
        return -1;

    const Type* t = castInst->getDestTy();

    if (t->isIntegerTy(16))
        return 16;
    if (t->isIntegerTy(32))
        return 32;
    if (t->isIntegerTy(64))
        return 64;
    if (t->isIntegerTy(8))
        return 8;

    return -1;
}
/**
 * Compare type from config with type of instruction.
 * @param I instruction to check
 * @param type type of binary operation
 * @return true if types match, false otherwise
*/
bool compareType(const llvm::Instruction *I, BinOpType type) {
    const Type* t = I->getType();

    if (type == BinOpType::INT16 && t->isIntegerTy(16))
        return true;
    if (type == BinOpType::INT32 && t->isIntegerTy(32))
        return true;
    if (type == BinOpType::INT64 && t->isIntegerTy(64))
        return true;
    if (type == BinOpType::INT8 && t->isIntegerTy(8))
        return true;

    return false;
}

/** Clone metadata from one instruction to another.
 * If i1 does not contain any metadata, then the instruction
 * that is closest to i1 is picked (we prefer the one that is after
 * and if there is none, then use the closest one before).
 *
 * @param i1 the first instruction
 * @param i2 the second instruction without any metadata
 */
void cloneMetadata(const llvm::Instruction *i1, llvm::Instruction *i2) {
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
Instruction* getNextInstruction(Instruction* ins) {
    BasicBlock::iterator I(ins);
    if (++I == ins->getParent()->end())
        return nullptr;
    return &*I;
}

/**
 * Returns the previous instruction before the one specified.
 * @param ins specified instruction
 * @return previous instruction or null, if ins is the first one.
 */
Instruction* getPreviousInstruction(Instruction* ins) {
    BasicBlock::iterator I(ins);
    if (--I == ins->getParent()->begin())
        return nullptr;
    return &*I;
}

/**
 * Inserts new call instruction.
 * @param I first instruction to be removed
 * @param count number of instructions to be removed
 */
void eraseInstructions(Instruction* I, int count) {
    Instruction* currentInstr = I;
    for (int i = 0; i < count; i++) {
        if (!currentInstr) {
            return;
        }
        Instruction* prevInstr = getPreviousInstruction(currentInstr);
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
void insertCallInstruction(Function* CalleeF, vector<Value *> args,
        RewriteRule rw_rule, Instruction *currentInstr,
        inst_iterator *Iiterator) {
    // update statistics
    ++statistics.inserted_calls[CalleeF];

    // Create new call instruction
    CallInst *newInstr = CallInst::Create(CalleeF, args);

    // duplicate the metadata of the instruction for which we
    // instrument the code because some passes (e.g. inliner) can
    // break the code when there's an instruction without metadata
    // when all other instructions have metadata
    cloneMetadata(currentInstr, newInstr);

    if (rw_rule.where == InstrumentPlacement::BEFORE) {
        // Insert before
        newInstr->insertBefore(currentInstr);
        logger.log_insertion("before", CalleeF, currentInstr);
    }
    else if (rw_rule.where == InstrumentPlacement::AFTER) {
        // Insert after
        newInstr->insertAfter(currentInstr);
        logger.log_insertion("after", CalleeF, currentInstr);
    }
    else if (rw_rule.where == InstrumentPlacement::REPLACE) {
        // In the end we move the iterator to the newInst position
        // so we can safely remove the sequence of instructions being
        // replaced
        newInstr->insertAfter(currentInstr);
        inst_iterator helper(*Iiterator);
        *Iiterator = ++helper;
        eraseInstructions(currentInstr, rw_rule.foundInstrs.size());
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
void insertCallInstruction(Function* CalleeF, vector<Value *> args,
                           Instruction *currentInstr)
{
    // update statistics
    ++statistics.inserted_calls[CalleeF];
    // Create new call instruction
    CallInst *newInstr = CallInst::Create(CalleeF, args);

    // Duplicate the metadata of the instruction for which we
    // instrument the code, some passes (e.g. inliner) can
    // break the code when there's an instruction without metadata
    // when all other instructions have metadata
    cloneMetadata(currentInstr, newInstr);

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
tuple<vector<Value *>, Instruction*> insertArgument(InstrumentInstruction rw_newInstr, Instruction *I,
        Function* CalleeF, const Variables& variables, InstrumentPlacement where)
{
    std::vector<Value *> args;
    unsigned i = 0;
    Instruction* nI = I;
    for (const string& arg : rw_newInstr.parameters) {

        if (i == rw_newInstr.parameters.size() - 1) {
            break;
        }

        auto var = variables.find(arg);
        if (var == variables.end()) {
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
            for (auto& arg : CalleeF->args()) {
                Value *argV = &arg;

                if (i == argIndex) {
                    if (argV->getType() != var->second->getType()) {
                        if (!var->second->getType()->isPtrOrPtrVectorTy() && !var->second->getType()->isIntegerTy()) {
                            args.push_back(var->second);
                        } else {
                            CastInst *CastI;
                            if (var->second->getType()->isPtrOrPtrVectorTy()) {
                                CastI = CastInst::CreatePointerCast(var->second, argV->getType());
                            } else {
                                CastI = CastInst::CreateIntegerCast(var->second, argV->getType(), true);
                            }

                            if (Instruction *Inst = dyn_cast<Instruction>(var->second))
                                cloneMetadata(Inst, CastI);

                            if (where == InstrumentPlacement::BEFORE) {
                                // We want to insert before I, that is:
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
                                // We want to insert after I, that is:
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
                    } else {
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
                                       const std::string& name)
{
    llvm::Constant *cF = I.module.getFunction(name);
    if (!cF) {
        // Get the function from the module with definitions
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
 * @return false if there was an error, true otherwise
 */
bool applyRule(LLVMInstrumentation& instr, Instruction *currentInstr, RewriteRule rw_rule,
        const Variables& variables, inst_iterator *Iiterator)
{
    logger.write_info("Applying rule...");

    // Work just with call instructions for now...
    if (rw_rule.newInstr.instruction != "call") {
        logger.write_error("Not working with this instruction: " + rw_rule.newInstr.instruction);
        return false;
    }

    // Get operands
    std::vector<Value *> args;

    // Get name of function
    const string& param = *(--rw_rule.newInstr.parameters.end());
    Function *CalleeF = getOrInsertFunc(instr, param);
    if (!CalleeF) {
        logger.write_error("Unknown function: " + param);
        return false;
    }

    // Insert arguments
    tuple<vector<Value*>, Instruction*> argsTuple = insertArgument(rw_rule.newInstr, currentInstr,
                                                        CalleeF, variables, rw_rule.where);
    args = get<0>(argsTuple);

    // Insert new call instruction
    insertCallInstruction(CalleeF, args, rw_rule, get<1>(argsTuple), Iiterator);

    return true;
}

/**
 * Applies a rule for global variables.
 * @param instr instrumentation object
 * @param I current instruction
 * @param rw_newInstr rule to apply - new instruction
 * @param variables map of found parameters form config
 * @return false if there was an error, true otherwise
 */
bool applyRule(LLVMInstrumentation& instr, Instruction *currentInstr, InstrumentInstruction rw_newInstr,
        const Variables& variables)
{
    logger.write_info("Applying rule for global variable...");

    // Work just with call instructions
    if (rw_newInstr.instruction != "call") {
        logger.write_error("Not working with this instruction: " + rw_newInstr.instruction);
        return false;
    }

    // Get operands
    std::vector<Value *> args;

    // Get name of function
    const string& param = *(--rw_newInstr.parameters.end());
    Function *CalleeF = getOrInsertFunc(instr, param);
    if (!CalleeF) {
        logger.write_error("Unknown function: " + param);
        return false;
    }

    // Insert arguments
    tuple<vector<Value*>, Instruction*> argsTuple = insertArgument(rw_newInstr, currentInstr,
                                                        CalleeF, variables, InstrumentPlacement::BEFORE);
    args = get<0>(argsTuple);

    // Insert new call instruction
    insertCallInstruction(CalleeF, args, get<1>(argsTuple));


    return true;
}

/**
 * Checks if the operands of instruction match.
 * @param rwIns instruction from rewrite rule.
 * @param ins instruction to be checked.
 * @param variables map for remembering some parameters.
 * @return true if OK, false otherwise
 */
bool checkOperands(InstrumentInstruction rwIns, Instruction* ins, Variables& variables) {
    unsigned opIndex = 0;

    for(const string& param : rwIns.parameters) {
        if (rwIns.parameters.size() == 1 && param=="*") {
            return true;
        }

        if(opIndex > ins->getNumOperands() - 1) {
            return false;
        }

        llvm::Value *op = ins->getOperand(opIndex);
        if (param[0] == '<' && param[param.size() - 1] == '>') {
            if (rwIns.stripInboundsOffsets != param) {
                variables[param] = op;
            } else {
                variables[param] = op->stripInBoundsOffsets();
            }
        } else if (param != "*"
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
        if (expV == value)
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
bool checkAnalysis(LLVMInstrumentation& instr, const Condition& condition, const Variables& variables) {
    if (condition.name == "")
        return true;

    list<Value*> parameters;
    for (const auto& arg : condition.arguments) {
        auto search = variables.find(arg);
        if (search != variables.end()) {
            parameters.push_back(search->second);
        }
        else {
            // Wrong parameters passed to the condition,
            // condition is not satisifed, do not instrument
            return false;
        }
    }

    for (auto& plugin : instr.plugins) {
        if (Analyzer::shouldInstrument(instr.rememberedValues, plugin.get(), condition, parameters)) {
            // Some plugin told us that we should instrument
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
        if (instr.rewriter.isFlag(condition.name)) {
            if (!checkFlag(condition, instr.rewriter)) {
                return false;
            }
        }
        else if (!checkAnalysis(instr, condition, variables)) {
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
void rememberValues(string name, LLVMInstrumentation& instr, Variables variables, const RewriteRule& rw) {
    auto search = variables.find(name);
    if (search != variables.end()) {
        std::string calledFunction = rw.newInstr.parameters.back();
        instr.rememberedValues.push_back(std::make_pair(search->second, calledFunction));
    }
}

/**
 * Adds ptset values that should be remembered into
 * instr's list.
 * @param name name of the value to be remembered
 * @param instr instrumentation object
 * @param variables list of variables
**/
void rememberPTSet(string name, LLVMInstrumentation& instr, Variables variables, const RewriteRule& rw) {
    auto search = variables.find(name);
    std::string calledFunction = rw.newInstr.parameters.back();
    if (search != variables.end() && calledFunction != "__INSTR_check_bounds_min_max") {
        bool containsUnknown = instr.ppPlugin->getPointsTo(search->second, instr.rememberedPTSets);
        if (containsUnknown)
            instr.rememberedUnknown = true;
    }
}

/**
 * Checks if the given instruction should be instrumented.
 * @param ins instruction to be checked.
 * @param rw_config parsed rules to apply.
 * @param Iiterator pointer to instructions iterator
 * @param instr instrumentation object
 * @return true if OK, false otherwise
 */
bool checkInstruction(Instruction* ins, Function* F, RewriterConfig rw_config, inst_iterator *Iiterator, LLVMInstrumentation& instr) {
    // Iterate through rewrite rules
    for (RewriteRule& rw : rw_config) {
        // Check if this rule should be applied in this function
        string functionName = F->getName().str();

        if (rw.inFunction != "*" && rw.inFunction != functionName)
            continue;

        // Check sequence of instructions
        Variables variables;
        bool instrument = false;
        Instruction* currentInstr = ins;
        for (list<InstrumentInstruction>::iterator iit=rw.foundInstrs.begin(); iit != rw.foundInstrs.end(); ++iit) {
            if (currentInstr == nullptr) {
                break;
            }

            InstrumentInstruction checkInstr = *iit;

            // Check the name
            if (currentInstr->getOpcodeName() == checkInstr.instruction) {
                // Check operands
                if (!checkOperands(checkInstr, currentInstr, variables)) {
                    break;
                }

                if (!checkInstr.getDestType.empty() &&
                      !checkInstr.getDestType.empty()) {
                    int size = getDestType(currentInstr);
                    if (size == -1)
                        break;
                    variables[checkInstr.getDestType] = ConstantInt::get(
                                        Type::getInt32Ty(instr.module.getContext()), size);
                }

                if (checkInstr.type != BinOpType::NBOP &&
                      !compareType(currentInstr, checkInstr.type)) {
                    break;
                }

                // Check return value
                if (checkInstr.returnValue != "*") {
                    if (checkInstr.returnValue[0] == '<'
                            && checkInstr.returnValue[checkInstr.returnValue.size() - 1] == '>') {
                        variables[checkInstr.returnValue] = currentInstr;
                    }
                }

                // Load next instruction to be checked
                list<InstrumentInstruction>::iterator final_iter = rw.foundInstrs.end();
                --final_iter;
                if (iit != final_iter) {
                    currentInstr = getNextInstruction(ins);
                }
                else {
                    instrument = true;
                }
            }
            else {
                break;
            }
        }

        // If all instructions match and conditions are satisfied
        // try to instrument the code
        if (instrument) {
            InstrumentInstruction iIns = rw.foundInstrs.front();

            if (!iIns.getSizeTo.empty()) {
                variables[iIns.getSizeTo] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()), getAllocatedSize(ins, instr.module));
            }

            if (!checkConditions(rw.conditions, instr, variables)) {
                const string& func = *(--rw.newInstr.parameters.end());
                ++statistics.suppresed_instr[func];
                continue;
            }

            if (rw.foundInstrs.size() == 1) {
                if (!getPointerInfos(variables, iIns, ins, instr))
                    return false;
            }

            // Set flags
            setFlags(rw, instr.rewriter);

            // Remember values that should be remembered
            rememberValues(rw.remember, instr, variables, rw);
            rememberPTSet(rw.rememberPTSet, instr, variables, rw);

            // Try to apply rule
            Instruction *where;
            if (rw.where == InstrumentPlacement::BEFORE) {
                where = ins;
            }
            else {
                // It is important in the REPLACE case that
                // we first place the new instruction after
                // the sequence
                where = currentInstr;
            }

            if (!applyRule(instr, where, rw, variables, Iiterator)) {
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
uint64_t getGlobalVarSize(GlobalVariable* GV, const Module& M) {

    DataLayout* DL = new DataLayout(&M);

    Type* Ty = GV->getType()->getElementType();

    if (!Ty->isSized())
        return 0;

    uint64_t size = DL->getTypeAllocSize(Ty);

    delete DL;

    return size;
}

/**
 * Instruments global variable if they should be instrumented according to the given rule.
 * @param instr LLVMInstrumentation object
 * @param g_rule a rule to be applied
 * @return true if instrumentation of global variables was done without problems, false otherwise
 */
bool instrumentGlobal(LLVMInstrumentation& instr, const GlobalVarsRule& g_rule) {
    // If there is no rule for global variables, do not try to instrument
    if (g_rule.inFunction.empty() || g_rule.globalVar.globalVariable.empty())
        return true;

    // Iterate through global variables
    Module::global_iterator GI = instr.module.global_begin(), GE = instr.module.global_end();
    for ( ; GI != GE; ++GI) {
        GlobalVariable *GV = dyn_cast<GlobalVariable>(GI);
        if (!GV)
            continue;

        if (g_rule.inFunction == "*") {
            return false;
            logger.write_error("Rule for global variables can be inserted only to a specific function!");
        }
        else {
            Function* F = getOrInsertFunc(instr, g_rule.inFunction);
            // Get operands of new instruction
            map <string, Value*> variables;

            if (g_rule.globalVar.globalVariable != "*")
                variables[g_rule.globalVar.globalVariable] = GV;
            if (!g_rule.globalVar.getSizeTo.empty()) {
                variables[g_rule.globalVar.getSizeTo] = ConstantInt::get(Type::getInt64Ty(instr.module.getContext()),
                                                                             getGlobalVarSize(GV, instr.module));
            }

            // Check the conditions
            bool satisfied = true;
            for (auto condition : g_rule.conditions) {
                if (!checkAnalysis(instr, condition, variables)) {
                    satisfied = false;
                    break;
                }
            }

            // Try to instrument the code
            if (satisfied) {
                // Try to apply rule
                inst_iterator IIterator = inst_begin(F);
                Instruction *firstI = &*IIterator;
                if (!applyRule(instr, firstI, g_rule.newInstr, variables)) {
                    logger.write_error("Cannot apply rule.");
                    return false;
                }
            }
        }
    }

    return true;
}

/**
 * Instruments global variables if they should be instrumented.
 * @param instr LLVMInstrumentation object.
 * @param phase current phase.
 * @return true if instrumentation of global variables was done without problems, false otherwise
 */
bool instrumentGlobals(LLVMInstrumentation& instr, const Phase& phase) {
    for (const auto& g_rule : phase.gconfig) {
        if (!instrumentGlobal(instr, g_rule))
            return false;
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
bool instrumentEntryPoints(LLVMInstrumentation& instr, Function* F, RewriterConfig rw_config) {
    if (F->isDeclaration())
        return true;
    for (RewriteRule& rw : rw_config) {

        // Check type of the rule
        if (rw.where != InstrumentPlacement::ENTRY)
            continue;
        // Check if the function should be instrumented
        string functionName = F->getName().str();
        if (rw.inFunction != "*" && rw.inFunction != functionName)
            continue;

        // Get name of a function to be instrumented
        const string& param = *(--rw.newInstr.parameters.end());
        Function *CalleeF = getOrInsertFunc(instr, param);
        if (!CalleeF) {
            logger.write_error("Unknown function: " + param);
            return false;
        }

        // Create new call instruction
        std::vector<Value *> args;
        CallInst *newInstr = CallInst::Create(CalleeF, args);

        // Insert at the beginning of function
        Instruction* firstInstr = (&*(F->begin()))->getFirstNonPHIOrDbg();
        if (firstInstr == nullptr)
            continue;
        newInstr->insertBefore(firstInstr);
        cloneMetadata(firstInstr, newInstr);

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
bool instrumentReturns(LLVMInstrumentation& instr, Function* F, RewriterConfig rw_config) {
    for (RewriteRule& rw : rw_config) {

        // Check type of the rule
        if (rw.where != InstrumentPlacement::RETURN)
            continue;
        // Check whether the function should be instrumented
        string functionName = F->getName().str();
        if (rw.inFunction != "*" && rw.inFunction != functionName)
            continue;

        // Get name of a function to be instrumented
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
                cloneMetadata(termInst, newInstr);
                logger.write_info("Inserting instruction at the end of function " + functionName);
            }
        }

        if (!inserted) {
            llvm::errs() << *F << "\n";
            assert(0 && "Did not find any return");
            abort();
        }
    }
    return true;
}

/**
 * Finds out whether the given function is reachable from main.
 * @param instr LLVMInstrumentation object
*/
void setReachableFunctions(LLVMInstrumentation& instr) {
    Function* main = instr.module.getFunction("main");
    if (main) {
        instr.ppPlugin->getReachableFunctions(instr.reachableFunctions, main);
    }
}

/**
 * Finds out whether the given function is reachable from main.
 * @param f function
 * @param instr LLVMInstrumentation object
 * @return true if the function is reachable from main, false otherwise
*/
bool isReachable(const Function& f, LLVMInstrumentation& instr) {
    for (auto& reachableF : instr.reachableFunctions) {
        if (reachableF == &f) {
            return true;
        }
    }

    return false;
}

/**
 * Runs one phase of instrumentation rules.
 * @param instr instrumentation object
 * @param phase current phase of instrumentation.
 * @return true if instrumentation was completed without problems, false otherwise
 */
bool runPhase(LLVMInstrumentation& instr, const Phase& phase) {

    // Instrument instructions in functions
    for (Module::iterator Fiterator = instr.module.begin(), E = instr.module.end(); Fiterator != E; ++Fiterator) {
        if (Fiterator->isDeclaration())
            continue;

        // Do not instrument functions linked for instrumentation
        string functionName = Fiterator->getName().str();

        if (Fiterator->getName().startswith("__INSTR_") ||
                Fiterator->getName().startswith("__VERIFIER_")) {
            logger.write_info("Omitting function " + functionName + " from instrumentation.");
            continue;
        }

        // If we have info from points-to plugin, do not
        // instrument functions that are not reachable from main
        if (instr.ppPlugin && functionName != "main" && !isReachable((*Fiterator), instr)) {
            logger.write_info("Omitting function " + functionName + " from instrumentation, not reachable from main.");
            continue;
        }

        if (!instrumentEntryPoints(instr, (&*Fiterator), phase.config))
            return false;
        if (!instrumentReturns(instr, (&*Fiterator), phase.config))
            return false;

        for (inst_iterator Iiterator = inst_begin(&*Fiterator), End = inst_end(&*Fiterator); Iiterator != End; ++Iiterator) {
            // This iterator may be replaced (by an iterator to the following
            // instruction) in the insertCallInstruction function
            // Check if the instruction is relevant
            if (!checkInstruction(&*Iiterator, (&*Fiterator), phase.config, &Iiterator, instr))
                return false;
        }
    }

    // Instrument global variables
    if (!instrumentGlobals(instr, phase))
        return false;

    return true;
}

/**
 * Instruments given module with rules from json file.
 * @param instr instrumentation object
 * @return true if instrumentation was done without problems, false otherwise
 */
bool instrumentModule(LLVMInstrumentation& instr) {
    logger.write_info("Starting instrumentation.");

    // Get points-to plugin
    getPointsToPlugin(instr);

    // Get reachable functions if points to plugin is available
    if (instr.ppPlugin) {
        setReachableFunctions(instr);
    }

    Phases rw_phases = instr.rewriter.getPhases();

    int i = 0;
    for (const auto& phase : rw_phases) {
        i++;
        logger.write_info("Start of the " + std::to_string(i) + ". phase.");
        if (!runPhase(instr, phase))
            return false;

        logger.write_info("End of the " + std::to_string(i) + ". phase.");
    }

#if ((LLVM_VERSION_MAJOR >= 4) || (LLVM_VERSION_MINOR >= 5))
    if (llvm::verifyModule(instr.module, &llvm::errs()))
#else
    if (llvm::verifyModule(instr.module, llvm::PrintMessageAction))
#endif
        return false;

    return true;
}

void saveModule(LLVMInstrumentation& instr) {
    // Write instrumented module into the output file
    std::ofstream ofs(instr.outputName);
    llvm::raw_os_ostream ostream(ofs);

    // Write the module
    errs() << "Saving the instrumented module to: " << instr.outputName << "\n";
    logger.write_info("Saving the instrumented module to: " + instr.outputName);
    llvm::WriteBitcodeToFile(&instr.module, ostream);
}

/**
 * Loads all plugins.
 * @param instr instrumentation object
 * @return true if plugins were succesfully loaded,
 * false, otherwise
 */
bool loadPlugins(LLVMInstrumentation& instr) {
    if (instr.rewriter.analysisPaths.size() == 0) {
        logger.write_info("No plugin specified.");
        return true;
    }

    for (const string& path : instr.rewriter.analysisPaths) {
        auto plugin = Analyzer::analyze(path, &instr.module);
        if (plugin) {
            logger.write_info("Plugin " + plugin->getName() + " loaded.");
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

static void dumpStatistics() {
    // dump statistics about instrumented module
    logger.write_info("Number of inserted calls:", true /* stdout */);
    for (auto& it : statistics.inserted_calls) {
        std::string funcName = it.first->getName().str();
        std::string msg = "  " + std::to_string(it.second) +
                          " of " + funcName;
        auto supp = statistics.suppresed_instr.find(funcName);
        if (supp != statistics.suppresed_instr.end()) {
            msg += " (blocked " + std::to_string(supp->second) + ")";
            statistics.suppresed_instr.erase(supp);
        }
        logger.write_info(msg, true /* stdout */);
    }
    // dump even the calls that were completely suppressed
    for (auto& it : statistics.suppresed_instr) {
        std::string msg = "  0 of " + it.first +
                          " (blocked " + std::to_string(it.second) + ")";
        logger.write_info(msg, true /* stdout */);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", GIT_VERSION);
        return 0;
    }

    if (argc < 5) {
        usage(argv[0]);
        exit(1);
    }

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

    dumpStatistics();

    config_file.close();
    llvmir_file.close();

    // If option --no-linking is present, do not link definitions
    // of instrumentation functions
    if (argc > 5 && strcmp(argv[5], "--no-linking") == 0 && resultOK) {
        saveModule(instr);
        logger.write_info("DONE.");
        return 0;
    }

    if (resultOK) {
        logger.write_info("DONE.");

        // Link instrumentation functions
        logger.write_info("Linking instrumentation functions...");
        bool linkNOK = Linker::linkModules(*module.get(), std::move(defModule));

        if (linkNOK) {
            logger.write_error("LINKING FAILED.");
            return 1;
        }

        saveModule(instr);
        logger.write_info("DONE.");
        return 0;
    }
    else {
        logger.write_error("FAILED.");
        return 1;
    }
}
