#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include "instr_plugin.hpp"
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowSensitive.h"
#include "call_graph.hpp"

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

    bool isValidPointer(llvm::Value* a, llvm::Value *len) {
        if (!a->getType()->isPointerTy())
            // null must be a pointer
            return false;
            
        uint64_t size = 0;
        if (llvm::ConstantInt *C = llvm::dyn_cast<llvm::ConstantInt>(len)) {
            size = C->getLimitedValue();
            // if the size cannot be expressed as an uint64_t,
            // say we do not know
            if (size == ~((uint64_t) 0))
                return false;
                
           // the offset is concrete number, fall-through
        } else {
           // we do not know anything with variable length
           return false;
        }
        
        assert(size > 0 && size < ~((uint64_t) 0));
        
        // need to have the PTA
        assert(PTA);
        PSNode *psnode = PTA->getPointsTo(a);
        if (!psnode || psnode->pointsTo.empty()) {
            llvm::errs() << "No points-to for " << *a << "\n";
            // we know nothing, it may be invalid
            return false;
        }
        
        for (const auto& ptr : psnode->pointsTo) {
            // unknown pointer and null are invalid
            if (ptr.isNull() || ptr.isUnknown())
                return false;
               
            // the memory this pointer points-to was invalidated
            if (ptr.isInvalidated())
                return false;
                
            // if the offset is unknown, than the pointer
            // may point after the end of allocated memory
            if (ptr.offset.isUnknown())
                return false;

            // if the offset + size > the size of allocated memory,
            // then this can be invalid operation. Check it so that
            // we won't overflow, that is, first ensure that psnode->size <= size
            // and than use this fact and equality with ptr.offset + size > psnode->size)
            if (size > ptr.target->getSize()
                || *ptr.offset > ptr.target->getSize() - size) {
                return false;
            }
            
            if (llvm::Instruction *I = llvm::dyn_cast<llvm::Instruction>(a)) {
                if(llvm::Instruction *Iptr = llvm::dyn_cast<llvm::Instruction>(ptr.target->getUserData<llvm::Value>())) {
                    llvm::Function *F = I->getParent()->getParent();
                    //if (Iptr->getParent()->getParent() != F || !F->doesNotRecurse()) {
                    CallGraph cg(*(I->getModule()), PTA);
                    if(Iptr->getParent()->getParent() != F || cg.isRecursive(F))
                        return false;
                } else {
                    llvm::errs() << "In bound pointer for non-allocated memory: " << *a << "\n";
                }
            } else {
                llvm::errs() << "In bound pointer for non-allocated memory: " << *a << "\n";
            }
        }
        
        // this pointer is valid
        return true;
    }
    
    bool isEqual(llvm::Value* a, llvm::Value* b) {
        if(PTA) {
            PSNode *psnode = PTA->getPointsTo(a);
            if (!psnode) return true;
            for (auto& ptr : psnode->pointsTo) {
                llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
                if(llvmVal == b) return true;
            }
        } else {
            return true; // a may point to b
        }
        
        return false;
    }
    
    bool isNotEqual(llvm::Value* a, llvm::Value* b) {
        if(PTA) {
            PSNode *psnode = PTA->getPointsTo(a);
            for (auto& ptr : psnode->pointsTo) {
                llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
                if(llvmVal == b) return false;
            }
        } else {
            return true; // we do not know whether a points to b
        }
        
        return true;
    }
    
    bool greaterThan(llvm::Value* a, llvm::Value* b) {
        if(PTA) {
            (void)a;
            (void)b;
            //TODO
        }
        return true;
    }

    bool lessThan(llvm::Value* a, llvm::Value* b) {
        if(PTA) {
            (void)a;
            (void)b;
            //TODO
        }
        return true;
    }
    
    bool lessOrEqual(llvm::Value* a, llvm::Value* b) {
        if(PTA) {
            (void)a;
            (void)b;
            //TODO
        }
        return true;
    }
    
    bool greaterOrEqual(llvm::Value* a, llvm::Value* b) {
        if(PTA) {
            (void)a;
            (void)b;
            //TODO
        }
        return true;
    }

    PointsToPlugin(llvm::Module* module) {
        llvm::errs() << "Running points-to analysis...\n";
        PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(module));
        PTA->run<dg::analysis::pta::PointsToFlowSensitive>();
    }
};

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new PointsToPlugin(module);
}
