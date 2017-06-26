#include "call_graph.hpp"
#include <llvm/IR/Function.h>
#include <list>

#if LLVM_VERSION_MAJOR >= 4 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 5)
#include "llvm/IR/InstIterator.h"
#else
#include "llvm/Support/InstIterator.h"
#endif

using namespace llvm;
using dg::analysis::pta::PSNode;

const int RESERVE_SIZE = 4;

CGNode::CGNode(const Function* callerF, int nodeId) {
	caller = callerF;
	id = id;
	calls.reserve(RESERVE_SIZE);
}

const Function* CGNode::getCaller() const {
	return caller;
}

int CGNode::getId() const {
	return id;
}

bool CGNode::containsCall(const Function* call) {
	for(auto node : calls) {
		
		if(node->getCaller() == call) {
			return true;
		}
	}
	return false;
}

CallGraph::CallGraph(Module &M, std::unique_ptr<dg::LLVMPointerAnalysis> &PTA) {
	lastId = 0;
	
	// Creates nodes from functions
	for (Module::iterator Fiterator = M.begin(), E = M.end(); Fiterator != E; ++Fiterator) {
		CGNode *newNode = new CGNode(&*Fiterator, lastId++);
		nodes.insert(std::pair<const Function*, CGNode*>(&*Fiterator, newNode));
	}

	// Go through functions
	for (Module::iterator Fiterator = M.begin(), E = M.end(); Fiterator != E; ++Fiterator) {
		// Look for call instructions
		for (inst_iterator Iiterator = inst_begin(*Fiterator); Iiterator != inst_end(*Fiterator); ++Iiterator){
			if (const CallInst *CI = dyn_cast<CallInst const>(&*Iiterator)){
				handleCallInst(PTA, &*Fiterator, CI);		
			}
		}
	}
}

void  CallGraph::BFS(const CGNode *currentNode, std::vector<bool> &visited) {	
	for(auto i : visited) {
		i = false;
	}
    
	std::list<const CGNode*> queue;
	visited[currentNode->getId()] = true;
	queue.push_back(currentNode);
    
	while (!queue.empty()) {
		currentNode = queue.front();
		queue.pop_front();
		for(auto node : currentNode->calls) {
			if(!visited[node->getId()]) {
				visited[node->getId()] = true;
				queue.push_back(node);
			}
		}
	}
}

bool CallGraph::containsCall(const Function* caller, const Function* callee) {
	for(auto node : nodes) {
		if(node.second->getCaller() == caller) {
			std::vector<bool> visited(RESERVE_SIZE);
			BFS(node.second, visited);
		}
	}
	return false;
}

bool CallGraph::containsDirectCall(const Function* caller, const Function* callee) {
	for(auto node : nodes) {
		if(node.second->getCaller() == caller) {
			return node.second->containsCall(callee);
		}
	}
	return false;
}

bool CallGraph::isRecursive(const Function* function) {
	return containsCall(function, function);
}

void CallGraph::handleCallInst(std::unique_ptr<dg::LLVMPointerAnalysis> &PTA, const Function *F,  const CallInst *CI) {
	CGNode* caller = (nodes.find(F))->second;

	// Store called functions for F in its vector
	const Value *CV = CI->getCalledValue()->stripPointerCasts();
	
	if (const Function *calledF = dyn_cast<Function>(CV)) {
		if(!containsDirectCall(F, calledF)) {
			caller->calls.push_back(nodes.find(calledF)->second);
		}
	} else {
		PSNode *psnode = PTA->getPointsTo(CV);
		for (auto& ptr : psnode->pointsTo) {
			Value *llvmValue = ptr.target->getUserData<llvm::Value>();
			if (const Function *calledF = dyn_cast<Function>(llvmValue)){
				if(!containsDirectCall(F, calledF)) {
					caller->calls.push_back(nodes.find(calledF)->second);
				}
			}
		}
	}
}


