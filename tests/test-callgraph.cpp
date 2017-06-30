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

LLVMContext C;

Module* getModule(std::string fileName) {
	
	// Get module from LLVM file
	SMDiagnostic Err;

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
	REQUIRE(cg.containsDirectCall(main, recursive));

	INFO("recursive function calls itself");
	REQUIRE(cg.containsDirectCall(recursive, recursive));

	INFO("recursive function does not call main function");
	REQUIRE(!cg.containsDirectCall(recursive, main));

	INFO("main is not recursive");
	REQUIRE(!cg.containsDirectCall(main, main));
}

TEST_CASE( "function_pointers01", "[callgraph]" ) {

	Module *m = getModule("sources/function_pointers01.ll");
	std::unique_ptr<dg::LLVMPointerAnalysis> PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(m));
	PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
	CallGraph cg(*m, PTA);
	
	Function *main = m->getFunction("main");
	Function *sum = m->getFunction("sum");
	
	INFO("main function calls sum function");
	REQUIRE(cg.containsDirectCall(main, sum));

	INFO("sum function is not recursive");
	REQUIRE(!cg.containsDirectCall(sum, sum));

	INFO("sum function does not call main function");
	REQUIRE(!cg.containsDirectCall(sum, main));
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
	REQUIRE(cg.containsDirectCall(main, call_foo));

	INFO("main function does not call foo function directly");
	REQUIRE(!cg.containsDirectCall(main, foo));

	INFO("main function calls foo function indirectly");
	REQUIRE(cg.containsCall(main, foo));

	INFO("call_foo function is not recursive");
	REQUIRE(!cg.containsDirectCall(call_foo, call_foo));

	INFO("call_foo function calls foo function");
	REQUIRE(cg.containsDirectCall(call_foo, foo));

	INFO("foo function is recursive");
	REQUIRE(cg.isRecursive(foo));

	INFO("foo function does not call call_foo function");
	REQUIRE(!cg.containsDirectCall(foo, call_foo));
}

TEST_CASE( "indirect_calls01", "[callgraph]" ) {

	Module *m = getModule("sources/indirect_calls01.ll");
	std::unique_ptr<dg::LLVMPointerAnalysis> PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(m));
	PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
	CallGraph cg(*m, PTA);
	
	Function *main = m->getFunction("main");
	Function *call_foo1 = m->getFunction("call_foo1");
	Function *foo1 = m->getFunction("foo1");
	Function *foo2 = m->getFunction("foo2");
	Function *foo3 = m->getFunction("foo3");
	
	INFO("main function calls call_foo1 function indirectly");
	REQUIRE(cg.containsCall(main, call_foo1));

	INFO("main function does not call foo2 function directly");
	REQUIRE(!cg.containsDirectCall(main, foo2));

	INFO("main function calls foo2 function indirectly");
	REQUIRE(cg.containsCall(main, foo2));

	INFO("main function calls foo3 function indirectly");
	REQUIRE(cg.containsCall(main, foo3));

	INFO("main function does not call foo3 directly");
	REQUIRE(!cg.containsDirectCall(main, foo3));

	INFO("call_foo1 function is not recursive");
	REQUIRE(!cg.isRecursive(call_foo1));

	INFO("call_foo1 function calls foo1 function");
	REQUIRE(cg.containsCall(call_foo1, foo1));

	INFO("foo3 function is recursive");
	REQUIRE(cg.isRecursive(foo3));

	INFO("foo1 function does not call call_foo function");
	REQUIRE(!cg.containsCall(foo1, call_foo1));

	INFO("foo1 function calls foo2 function");
	REQUIRE(cg.containsCall(foo1, foo2));
	
	INFO("foo1 function calls foo3 function");
	REQUIRE(cg.containsCall(foo1, foo3));
}

TEST_CASE( "indirect_calls02", "[callgraph]" ) {

	Module *m = getModule("sources/indirect_calls02.ll");
	std::unique_ptr<dg::LLVMPointerAnalysis> PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(m));
	PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
	CallGraph cg(*m, PTA);
	
	Function *main = m->getFunction("main");
	Function *foo1 = m->getFunction("foo1");
	Function *foo2 = m->getFunction("foo2");
	Function *foo3 = m->getFunction("foo3");
	
	INFO("main function does not call foo2 function directly");
	REQUIRE(!cg.containsDirectCall(main, foo2));

	INFO("main function calls foo2 function indirectly");
	REQUIRE(cg.containsCall(main, foo2));

	INFO("foo1 function calls foo2 function indirectly");
	REQUIRE(cg.containsCall(foo1, foo2));

	INFO("foo1 function does not call foo2 function directly");
	REQUIRE(!cg.containsDirectCall(foo1, foo2));

	INFO("foo1 function calls foo2 function indirectly");
	REQUIRE(cg.containsCall(foo1, foo2));

	INFO("foo3 function is recursive");
	REQUIRE(cg.isRecursive(foo3));

	INFO("foo3 function calls foo2 function");
	REQUIRE(cg.containsCall(foo3, foo2));
	
	INFO("foo2 function calls foo3 function");
	REQUIRE(cg.containsCall(foo2, foo3));
	
	INFO("foo2 function does not call foo1 function");
	REQUIRE(!cg.containsCall(foo2, foo1));
}
