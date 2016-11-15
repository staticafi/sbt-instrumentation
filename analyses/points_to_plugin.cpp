#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include "instr_plugin.hpp"
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowInsensitive.h"

using dg::analysis::pta::PSNode;
class PointsToPlugin : public InstrPlugin
{
 private:
  std::unique_ptr<dg::LLVMPointerAnalysis> PTA;

 public:
  bool isNull(llvm::Value* a) {
      if (!a->getType()->isPointerTy())
          // null must be a pointer
          return false;

      // need to have the PTA
      assert(PTA);
	  PSNode *psnode = PTA->getPointsTo(a);
      if (!psnode || psnode->pointsTo.empty()) {
          llvm::errs() << "No points-to for " << *a << "\n";
          // we know nothing, it may be null
          return true;
      }

	  for (const auto& ptr : psnode->pointsTo) {
          // unknown pointer can be null too
          if (ptr.isNull() || ptr.isUnknown())
              return true;
      }

      // a can not be null
	  return false;
  }

  bool isValidPointer(llvm::Value* a) {
      if (!a->getType()->isPointerTy())
          // null must be a pointer
          return false;

      // need to have the PTA
      assert(PTA);
	  PSNode *psnode = PTA->getPointsTo(a);
      if (!psnode || psnode->pointsTo.empty()) {
          llvm::errs() << "No points-to for " << *a << "\n";
          // we know nothing, it may be invalid
          return false;
      }

	  for (const auto& ptr : psnode->pointsTo) {
          // unknown pointer and null are not invalid
          if (ptr.isNull() || ptr.isUnknown())
              return false;

            // if the offset is unknown, that the pointer
            // may point after the end of allocated memory
            if (ptr.offset.isUnknown())
                return false;
      }

      // this pointer is valid
	  return true;
  }



  bool isEqual(llvm::Value* a, llvm::Value* b){
	  if(PTA){
		  PSNode *psnode = PTA->getPointsTo(a);
		  for (auto& ptr : psnode->pointsTo) {
			  llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
			  if(llvmVal == b) return true;
		  }
	  }
	  else{
		  return true; // a may point to b
	  }

	  return false;
  }

  bool isNotEqual(llvm::Value* a, llvm::Value* b){
	  if(PTA){
		  PSNode *psnode = PTA->getPointsTo(a);
		  for (auto& ptr : psnode->pointsTo) {
			  llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
			  if(llvmVal == b) return false;
		  }
	  }
	  else{
		  return true; // we do not know whether a points to b
	  }

	  return true;
  }

  bool greaterThan(llvm::Value* a, llvm::Value* b){
	  if(PTA){
		  //TODO
	  }
	  return true;
  }

  bool lessThan(llvm::Value* a, llvm::Value* b){
	  if(PTA){
		  //TODO
	  }
	  return true;
  }

  bool lessOrEqual(llvm::Value* a, llvm::Value* b){
	  if(PTA){
		  //TODO
	  }
	  return true;
  }

  bool greaterOrEqual(llvm::Value* a, llvm::Value* b){
	  if(PTA){
		  //TODO
	  }
	  return true;
  }

  PointsToPlugin(llvm::Module* module) {
      llvm::errs() << "Running points-to analysis...\n";
	  PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(module));
	  PTA->run<dg::analysis::pta::PointsToFlowInsensitive>();
  }
};

extern "C" InstrPlugin* create_object(llvm::Module* module){
    return new PointsToPlugin(module);
}
