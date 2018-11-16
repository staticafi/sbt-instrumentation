#ifndef POINTS_TO_PLUGIN_H
#define POINTS_TO_PLUGIN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <tuple>
#include "instr_plugin.hpp"
#include "dg/llvm/analysis/PointsTo/PointerAnalysis.h"
#include "dg/analysis/PointsTo/PointerAnalysisFSInv.h"
#include "call_graph.hpp"


using dg::analysis::pta::PSNode;

class PointerInfo
{

public:
    PointerInfo() {};

    PointerInfo(llvm::Value* pointerOp, uint64_t offset, uint64_t space)
    : pointer(pointerOp), min_offset(offset), min_space(space),
      max_offset(offset), max_space(space) {};

    PointerInfo(llvm::Value* pointerOp, uint64_t minOffset, uint64_t minSpace,
                uint64_t maxOffset, uint64_t maxSpace)
    : pointer(pointerOp), min_offset(minOffset), min_space(minSpace),
      max_offset(maxOffset), max_space(maxSpace) {};

    llvm::Value* getPointer() {
        return pointer;
    }

    uint64_t getMinSpace() {
        return min_space;
    }

    uint64_t getMinOffset() {
        return min_offset;
    }

    uint64_t getMaxSpace() {
        return max_space;
    }

    uint64_t getMaxOffset() {
        return max_offset;
    }

private:
    llvm::Value* pointer = nullptr;
    uint64_t min_offset = 0;
    uint64_t min_space = 0;
    uint64_t max_offset = 0;
    uint64_t max_space = 0;
};

class PointsToPlugin : public InstrPlugin
{

private:
    bool allMayBeLeaked = false;
    // possibly leaked allocations
    std::set<PSNode *> possiblyLeaked;
    std::unique_ptr<dg::analysis::pta::PointerAnalysis> PTAImpl;
    std::unique_ptr<dg::LLVMPointerAnalysis> PTA;
    CallGraph cg;

    std::string isNull(llvm::Value* a);
    std::string isValidPointer(llvm::Value* a, llvm::Value *len);
    std::string pointsTo(llvm::Value* a, llvm::Value *b);
    std::string hasKnownSize(llvm::Value* a);
    std::string hasKnownSizes(llvm::Value* a);
    std::string isInvalid(llvm::Value* a);
    std::string pointsToHeap(llvm::Value* a);
    std::string pointsToStack(llvm::Value* a);
    std::string pointsToGlobal(llvm::Value* a);
    std::string mayBeLeaked(llvm::Value* a);
    std::string mayBeLeakedOrFreed(llvm::Value* a);
    std::string safeForFree(llvm::Value* a);

    void gatherPossiblyLeaked(llvm::Module *);
    void gatherPossiblyLeaked(llvm::ReturnInst *);

public:
    bool supports(const std::string& query) override;
    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands)
    {
        if (query == "isValidPointer") {
            assert(operands.size() == 2 && "Wrong number of operands");
            return isValidPointer(operands[0], operands[1]);
        } else if (query == "pointsTo") {
            assert(operands.size() == 2 && "Wrong number of operands");
            return pointsTo(operands[0], operands[1]);
        } else if (query == "hasKnownSize") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return hasKnownSize(operands[0]);
        } else if (query == "hasKnownSizes") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return hasKnownSizes(operands[0]);
        } else if (query == "isNull") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return isNull(operands[0]);
        } else if (query == "pointsToHeap") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return pointsToHeap(operands[0]);
        } else if (query == "pointsToGlobal") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return pointsToGlobal(operands[0]);
        } else if (query == "pointsToStack") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return pointsToStack(operands[0]);
        } else if (query == "isInvalid") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return isInvalid(operands[0]);
        } else if (query == "mayBeLeaked") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return mayBeLeaked(operands[0]);
        } else if (query == "mayBeLeakedOrFreed") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return mayBeLeakedOrFreed(operands[0]);
        } else if (query == "safeForFree") {
            assert(operands.size() == 1 && "Wrong number of operands");
            return safeForFree(operands[0]);
        } else {
            return "unsupported query";
        }
    }

    virtual bool getPointsTo(llvm::Value* a,
                             std::vector<llvm::Value*>& ptset);
    virtual PointerInfo getPointerInfo(llvm::Value* a);
    virtual PointerInfo getPInfoMin(llvm::Value* a);
    virtual PointerInfo getPInfoMinMax(llvm::Value* a,
                                        std::vector<llvm::Value*>& ptset);
    virtual void getReachableFunctions(std::set<const llvm::Function*>& reachableFunctions, const llvm::Function* a);
    virtual std::string notMinMemoryBlock(llvm::Value* min, llvm::Value* a);

    PointsToPlugin(llvm::Module* module) : InstrPlugin("PointsTo") {
        llvm::errs() << "Running points-to analysis...\n";
        PTA = std::unique_ptr<dg::LLVMPointerAnalysis>(new dg::LLVMPointerAnalysis(module));
        PTAImpl.reset(PTA->createPTA<dg::analysis::pta::PointerAnalysisFSInv>());
        PTAImpl->run();

        llvm::errs() << "Building call-graph...\n";
        cg.buildCallGraph(*module, PTA);

        gatherPossiblyLeaked(module);

        llvm::errs() << "PT plugin done.\n";
    }
};

#endif

