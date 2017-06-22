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

	#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 6)
	std::unique_ptr<Module> _m = parseIRFile(fileName, Err, getGlobalContext());
	Module *m = _m.release();
	#else
	Module *m = ParseIRFile(fileName, Err, getGlobalContext());
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
	
	SECTION("main function calls recursive function") {
		REQUIRE(cg.containsCall(main, recursive));
	}

	SECTION("recursive function calls itself") {
		REQUIRE(cg.containsCall(recursive, recursive));
	}

	SECTION("recursive function does not call main function") {
		REQUIRE(!cg.containsCall(recursive, main));
	}
}
