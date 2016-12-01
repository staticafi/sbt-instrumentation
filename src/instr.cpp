#include "rewriter.hpp"
#include "instr_log.hpp"
#include "instr_analyzer.hpp"
#include <iostream>
#include <fstream>
#include <exception>

#include <llvm/Pass.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DebugInfoMetadata.h>

#if (LLVM_VERSION_MINOR >= 5)
#include "llvm/IR/InstIterator.h"
#else
#include "llvm/Support/InstIterator.h"
#endif
#include <llvm/Support/SourceMgr.h>

//#include "llvm-c/BitWriter.h"

#include <memory>
#include <string>

#define uint unsigned int

using namespace llvm;

using namespace std;

Logger logger("log.txt");
list<unique_ptr<InstrPlugin>> plugins;

string outputName;

void usage(char *name) {
	cerr << "Usage: " << name << " <config.json> <llvm IR> <outputFileName>" << endl;
}

uint64_t getAllocatedSize(Instruction *I, Module* M){
	DataLayout* DL = new DataLayout(M);

	const AllocaInst *AI = dyn_cast<AllocaInst>(I);

	if(!AI)
		return 0;

	Type* Ty = AI->getAllocatedType();

	if(!Ty->isSized())
		return 0;

	uint64_t size = DL->getTypeAllocSize(Ty);

	delete DL;

	return size;
}

/** Clone metadata from one instruction to another
 * @param i1 the first instruction
 * @param i2 the second instruction without any metadata
*/
void CloneMetadata(const llvm::Instruction *i1, llvm::Instruction *i2)
{
    if (!i1->hasMetadata())
        return;

    assert(!i2->hasMetadata());
    llvm::SmallVector< std::pair< unsigned, llvm::MDNode * >, 2> mds;
    i1->getAllMetadata(mds);

    for (const auto& it : mds) {
        i2->setMetadata(it.first, it.second->clone().release());
    }
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

	if(rw_rule.where == InstrumentPlacement::BEFORE) {
		// Insert before
		newInstr->insertBefore(currentInstr);
		logger.log_insertion("before",CalleeF, currentInstr);
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
	}
}

/**
 * Inserts an argument.
 * @param Irw_rule rewrite rule
 * @param I instruction
 * @param CalleeF function to be called
 * @param variables map of found parameters from config
 * @return a vector of arguments for the call that is to be inserted
 *         and a pointer to the instruction after/before the new call
 *         is going to be inserted (it is either I or some newly added
 *         argument)
 */
tuple<vector<Value *>, Instruction*> InsertArgument(RewriteRule rw_rule, Instruction *I,
                                                    Function* CalleeF, const map <string, Value*>& variables) {
	std::vector<Value *> args;
	unsigned i = 0;
	Instruction* nI = I;
	for(const string& arg : rw_rule.newInstr.parameters){

		if(i == rw_rule.newInstr.parameters.size() - 1) {
			break;
		}

        auto var = variables.find(arg);
		if(var == variables.end()) {
			// NOTE: in future think also about other types than ConstantInt
			int argInt;
			try {
				argInt = stoi(arg);
				LLVMContext &Context = getGlobalContext();
				Value *intValue = ConstantInt::get(Type::getInt32Ty(Context), argInt);
				args.push_back(intValue);
			}
			catch (invalid_argument) {
				logger.write_error("Problem with instruction arguments: invalid argument.");
			}
			catch (out_of_range) {
				logger.write_error("Problem with instruction arguments: out of range.");
			}
		}
		else {
			unsigned argIndex = 0;
			for (Function::ArgumentListType::iterator sit=CalleeF->getArgumentList().begin(); sit != CalleeF->getArgumentList().end(); ++sit) {
				Value *argV = &*sit;

				if(i == argIndex) {
					if(argV->getType() != var->second->getType()) {
						//TODO other types?
						if(!var->second->getType()->isPtrOrPtrVectorTy()){
							args.push_back(var->second);
						}else{
							CastInst *CastI = CastInst::CreatePointerCast(var->second, argV->getType());
                            if (Instruction *Inst = dyn_cast<Instruction>(var->second))
                                CloneMetadata(Inst, CastI);
                            if(rw_rule.where == InstrumentPlacement::BEFORE) {
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
                                CastI->insertAfter(I);
							    nI = CastI;
                            }
							args.push_back(CastI);
						}
					}
					else{
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

/**
 * Applies a rule.
 * @param M module
 * @param I current instruction
 * @param rw_rule rule to apply
 * @param variables map of found parameters form config
 * @param Iiterator pointer to instructions iterator
 * @return 1 if error
 */
int applyRule(Module &M, Instruction *currentInstr, RewriteRule rw_rule,
              const map <string, Value*>& variables, inst_iterator *Iiterator) {
	logger.write_info("Applying rule...");

	// Work just with call instructions for now...
	if(rw_rule.newInstr.instruction != "call") {
		logger.write_error("Not working with this instruction: " + rw_rule.newInstr.instruction);
		return 1;
	}

	// Get operands
	std::vector<Value *> args;
	Function *CalleeF = NULL;

	// Get name of function
	string param = *(--rw_rule.newInstr.parameters.end());
	CalleeF = M.getFunction(param);
	if (!CalleeF) {
		logger.write_error("Unknown function: " + param);
		return 1;
	}

	// Insert arguments
	tuple<vector<Value*>, Instruction*> argsTuple = InsertArgument(rw_rule, currentInstr, CalleeF, variables);
	args = get<0>(argsTuple);

	// Insert new call instruction
	InsertCallInstruction(CalleeF, args, rw_rule, get<1>(argsTuple), Iiterator);

	return 0;
}

/**
 * Checks if the operands of instruction match.
 * @param rwIns instruction from rewrite rule.
 * @param ins instruction to be checked.
 * @param variables map for remembering some parameters.
 * @return true if OK, false otherwise
 */
bool CheckOperands(InstrumentInstruction rwIns, Instruction* ins, map <string, Value*> &variables) {
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
 * Runs all plugins for static analyses and decides, whether to
 * instrument or not.
 * @param condition condition that must be satisfied to instrument
 * @param variables
 * @return true if condition is ok, false otherwise
 */
bool checkAnalysis(list<string> condition, const map<string, Value*>& variables){
	// condition: first element is operator, other one or two elements
	// are variables, TODO do we need more than two variables?
	if(condition.empty())
		return true;

	string conditionOp = condition.front();
	list<string>::iterator it = condition.begin();
	it++;
	string aName = *it;
	string bName = "";

	Value* aValue = (variables.find(aName))->second;
	Value* bValue = NULL;
	if(condition.size()>2){
		it++;
		bName = *it;
		bValue = (variables.find(bName))->second;
	}

	for(auto& plugin : plugins){
		if(!Analyzer::shouldInstrument(plugin.get(), conditionOp, aValue, bValue)){
            // some plugin told us that we should not instrument
			return false;
		}
	}

    // all analyses told that we should instrument
	return true;
}

/**
 * Checks if the given instruction should be instrumented.
 * @param ins instruction to be checked.
 * @param M module.
 * @param rw_config parsed rules to apply.
 * @param Iiterator pointer to instructions iterator
 * @return true if OK, false otherwise
 */
bool CheckInstruction(Instruction* ins, Module& M, Function* F, RewriterConfig rw_config, inst_iterator *Iiterator) {
	// iterate through rewrite rules
	for (RewriteRule& rw : rw_config){
		// check if this rule should be applied in this function
		string functionName = F->getName().str();

		if(rw.inFunction != "*" && rw.inFunction!=functionName)
			continue;

		// check sequence of instructions
		map <string, Value*> variables;
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
			InstrumentInstruction allocaIns = rw.foundInstrs.front();
			if(!allocaIns.getSizeTo.empty()){
				LLVMContext &Context = getGlobalContext();
				variables[allocaIns.getSizeTo] = ConstantInt::get(Type::getInt64Ty(Context), getAllocatedSize(ins,&M));
			}

		}


		// if all instructions match, try to instrument the code
		if(instrument && checkAnalysis(rw.condition,variables)) {
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

			if(applyRule(M, where, rw, variables, Iiterator) == 1) {
				logger.write_error("Cannot apply rule.");
				return false;
			}
		}
	}

	return true;
}

/**
 * Instruments given module with rules from json file.
 * @param M module to be instrumented.
 * @param rw_config parsed rules to apply.
 * @return true if OK, false otherwise
 */
bool instrumentModule(Module &M, RewriterConfig rw_config) {
	for (Module::iterator Fiterator = M.begin(), E = M.end(); Fiterator != E; ++Fiterator) {

		// Do not instrument functions linked for instrumentation
		string functionName = (&*Fiterator)->getName().str();

		if(functionName.find("__INSTR_")!=string::npos ||
		   functionName.find("__VERIFIER_")!=string::npos) { //TODO just starts with
			logger.write_info("Omitting function " + functionName + " from instrumentation.");
			continue;
		}

		for (inst_iterator Iiterator = inst_begin(&*Fiterator), End = inst_end(&*Fiterator); Iiterator != End; ++Iiterator) {
			// This iterator may be replaced (by an iterator to the following
			// instruction) in the InsertCallInstruction function
			// Check if the instruction is relevant
			if(!CheckInstruction(&*Iiterator, M, &*Fiterator, rw_config, &Iiterator)) return false;
		}
	}

	// Write instrumented module into the output file
	ofstream out_file;
	out_file.open(outputName, ofstream::out | ofstream::trunc);
	raw_os_ostream rstream(out_file);

	WriteBitcodeToFile(&M, rstream);

	return true;
}


/**
 * Loads all plugins.
 * @param rw Rules from config file.
 * @param module Module to be instrumented.
 */
void loadPlugins(Rewriter rw, Module* module) {
	for(const string& path : rw.analysisPaths) {
		auto plugin = Analyzer::analyze(path, module);
		if (plugin)
			plugins.push_back(std::move(plugin));
		else
			cout <<"Failed loading plugin: " << path << endl;
	}
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		usage(argv[0]);
		exit(1);
	}

	// TODO: Check for failure
	ifstream config_file;
	config_file.open(argv[1]);

	ifstream llvmir_file;
	llvmir_file.open(argv[2]);

	outputName = argv[3];

	// Parse json file
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

	RewriterConfig rw_config = rw.getConfig();

	// Get module from LLVM file
	LLVMContext &Context = getGlobalContext();
	SMDiagnostic Err;
#if (LLVM_VERSION_MINOR >= 6)
	std::unique_ptr<Module> _m = parseIRFile(argv[2], Err, Context);
	Module *m = _m.release();
#else
	Module *m = ParseIRFile(argv[2], Err, Context);
#endif
	if (!m) {
		logger.write_error("Error parsing .bc file.");
		Err.print(argv[0], errs());
		config_file.close();
		llvmir_file.close();
		return 1;
	}

	loadPlugins(rw,m);

	// Instrument
	bool resultOK = instrumentModule(*m, rw_config);

	delete m;
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
