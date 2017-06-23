#include <string>

#include "catch.hpp"
#include "../analyses/call_graph.hpp"

#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowInsensitive.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>

using namespace llvm;

Module* getModule(std::string fileName) {
	
	// Get module from LLVM file
	SMDiagnostic Err;
	LLVMContext C;

#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 6)
	std::unique_ptr<Module> _m = parseIRFile(fileName, Err, C);
	Module *m = _m.release();
#else
	Module *m = ParseIRFile(fileName, Err, C);
#endif

	return m;
}

TEST_CASE( "recursive01", "[callgraph]" ) {

	Module *m = getModule("sources/recursive01.ll");
	std::unique_ptr<dg::LLVMPointerAnalysis> PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(m));
	PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
	CallGraph cg(*m, PTA);
	
	Function *main = m->getFunction("main");
	Function *recursive = m->getFunction("recursive");
	
	INFO("main function calls recursive function");
	REQUIRE(cg.containsCall(main, recursive));

	INFO("recursive function calls itself");
	REQUIRE(cg.containsCall(recursive, recursive));

	INFO("recursive function does not call main function");
	REQUIRE(!cg.containsCall(recursive, main));

	INFO("main is not recursive");
	REQUIRE(!cg.containsCall(main, main));
}

TEST_CASE( "function_pointers01", "[callgraph]" ) {

	Module *m = getModule("sources/function_pointers01.ll");
	std::unique_ptr<dg::LLVMPointerAnalysis> PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(m));
	PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
	CallGraph cg(*m, PTA);
	
	Function *main = m->getFunction("main");
	Function *sum = m->getFunction("sum");
	
	INFO("main function calls sum function");
	REQUIRE(cg.containsCall(main, sum));

	INFO("sum function is not recursive");
	REQUIRE(!cg.containsCall(sum, sum));

	INFO("sum function does not call main function");
	REQUIRE(!cg.containsCall(sum, main));
}

TEST_CASE( "recursive02", "[callgraph]" ) {

	Module *m = getModule("sources/recursive02.ll");
	std::unique_ptr<dg::LLVMPointerAnalysis> PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(m));
	PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
	CallGraph cg(*m, PTA);
	
	Function *main = m->getFunction("main");
	Function *call_foo = m->getFunction("call_foo");
	Function *foo = m->getFunction("foo");
	
	INFO("main function calls call_foo function");
	REQUIRE(cg.containsCall(main, call_foo));

	INFO("main function does not call foo function");
	REQUIRE(!cg.containsCall(main, foo));

	INFO("call_foo function is not recursive");
	REQUIRE(!cg.containsCall(call_foo, call_foo));

	INFO("call_foo function calls foo function");
	REQUIRE(cg.containsCall(call_foo, foo));

	INFO("foo function is recursive");
	REQUIRE(cg.containsCall(foo, foo));

	INFO("foo function does not call call_foo function");
	REQUIRE(!cg.containsCall(foo, call_foo));
}
