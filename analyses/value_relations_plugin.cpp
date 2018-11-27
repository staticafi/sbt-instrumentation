#include "value_relations_plugin.hpp"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_os_ostream.h>

using namespace llvm;

ValueRelationsPlugin::ValueRelationsPlugin(llvm::Module* module)
: InstrPlugin("ValueRelationsPlugin"), VR(module) {
    ///
    // Compute value relations
    VR.build();
    VR.compute();

    processCycles(module);
}

const llvm::Value *getMemorySize(const llvm::Value *v) {
    if (auto AI = dyn_cast<AllocaInst>(v)) {
        if (AI->isArrayAllocation())
            return AI->getArraySize();
    } else if (auto CI = dyn_cast<CallInst>(v)) {
        auto F = CI->getCalledFunction();
        if (!F)
            return nullptr;
        if (F->getName().equals("malloc")) {
            return CI->getOperand(0);
        }
    }

    return nullptr;
}

std::string ValueRelationsPlugin::isValidPointer(Value *ptr, Value *len) {
    if (!ptr->getType()->isPointerTy())
        // null must be a pointer
        return "false";

    if (ok_instructions.count(ptr) > 0)
        return "true";

    uint64_t size = 0;
    if (ConstantInt *C = dyn_cast<ConstantInt>(len)) {
        size = C->getLimitedValue();
        // if the size cannot be expressed as an uint64_t,
        // say we do not know
        if (size == ~((uint64_t) 0))
            return "maybe";

        // the offset is concrete number, fall-through
    } else {
        // we do not know anything with variable length
        return "maybe";
    }

    assert(size > 0 && size < ~((uint64_t) 0));

    auto GEP = dyn_cast<GetElementPtrInst>(ptr);
    if (!GEP) {
        return "unknown";
    }

    if (GEP->getNumIndices() > 1)
        return "unknown";

    //errs() << *GEP << " + " << *len << "\n";

    auto memSize = getMemorySize(GEP->getPointerOperand());
    if (!memSize)
        return "unknown";

    auto idx = GEP->getOperand(1);
    //errs() << "idx " << *idx << "\n";

    if (VR.isLt(ptr, idx, memSize)) {
        // since we stop computing values after
        // weird casts and shifts, it must be true
        // that idx means lesser element in array
        // and "len" fits into memory
        // (e.g. it cannot be the case that idx
        //  one byte before the end of memory
        //  and we want to write 4 bytes)
        return "true";
    }

    return "unknown";
}

namespace cycles {
using namespace llvm;

// modified dfs to reveal (most of the) cycles
class dfs {
    std::vector<BasicBlock *> stack;
    std::set<BasicBlock *> on_stack;
public:
    template <typename F>
    void run(BasicBlock *cur, F fun) {
        // cycle
        if (on_stack.count(cur) > 0) {
            std::vector<BasicBlock *> cycle;
            // we should made this faster by using a map
            // (on_stack: BasicBlock * -> index)
            // or by iterating from behind
            auto it = stack.begin();
            while (*it != cur)
                ++it;
            assert(it != stack.end());

            while (it != stack.end()) {
                cycle.push_back(*it);
                ++it;
            }
            assert(cycle[0] == cur);

            fun(cycle);
            return;
        }

        stack.push_back(cur);
        on_stack.insert(cur);

        for (auto it = succ_begin(cur), et = succ_end(cur);
             it != et; ++it) {
            run(*it, fun);
        }

        on_stack.erase(cur);
        stack.pop_back();
    }
};

} // namespace cycles

void ValueRelationsPlugin::processCycles(llvm::Module *M) {
    using namespace llvm;
    // do DFS through M and detect simple cycles.
    // This is does not detect all cycles, but at least some of them...

    for (auto& F : *M) {
        cycles::dfs dfs;
        dfs.run(&F.getEntryBlock(), [&](const std::vector<BasicBlock *>& cycle) {
            processCycle(cycle);
        });
    }
}

std::pair<llvm::Value *, llvm::Value *>
iterateUntilValue(llvm::BasicBlock *blk, const std::set<BasicBlock *>& blocks) {
    using namespace llvm;

    auto term = blk->getTerminator();
    auto br = dyn_cast<BranchInst>(term);
    if (!br)
        return {nullptr, nullptr};

    auto predicate = ICmpInst::Predicate::ICMP_NE;
    // negated condition
    if (blocks.count(br->getSuccessor(0)) == 0)
        predicate = ICmpInst::Predicate::ICMP_EQ;

    auto cond = dyn_cast<ICmpInst>(br->getCondition());
    if (!cond)
        return {nullptr, nullptr};

    auto pred = cond->getSignedPredicate();
    if (pred != predicate)
        return {nullptr, nullptr};

    auto what = cond->getOperand(1);
    auto value = dyn_cast<ConstantInt>(cond->getOperand(0));
    if (!value) {
        value = dyn_cast<ConstantInt>(cond->getOperand(1));
        what = cond->getOperand(0);
    }

    if (!value)
        return {nullptr, nullptr};

    // iterate until you find the 'value'
    // via the given pointer
    if (auto LI = dyn_cast<LoadInst>(what))
        return {LI->getPointerOperand(), value};

    return {nullptr, nullptr};
}

std::pair<llvm::Value *, llvm::Value *>
iterateUntilLessThan(llvm::BasicBlock *blk, const std::set<BasicBlock *>& blocks) {
    using namespace llvm;

    auto term = blk->getTerminator();
    auto br = dyn_cast<BranchInst>(term);
    if (!br)
        return {nullptr, nullptr};

    auto predicate1 = ICmpInst::Predicate::ICMP_SLT;
    auto predicate2 = ICmpInst::Predicate::ICMP_ULT;
    // negated condition
    if (blocks.count(br->getSuccessor(0)) == 0) {
        predicate1 = ICmpInst::Predicate::ICMP_SGE;
        predicate2 = ICmpInst::Predicate::ICMP_UGE;
    }

    auto cond = dyn_cast<ICmpInst>(br->getCondition());
    if (!cond)
        return {nullptr, nullptr};

    auto pred = cond->getSignedPredicate();
    if (pred != predicate1 && pred != predicate2)
        return {nullptr, nullptr};

    return {cond->getOperand(0), cond->getOperand(1)};
}


void ValueRelationsPlugin::handleIterateUntilValue(llvm::Value *ptr, llvm::Value *value,
                                                   llvm::BasicBlock *entry,
                                                   std::set<llvm::BasicBlock *>& blocks)
{
    using namespace llvm;

    // check that the value is in the memory before entering the loop
    const AllocaInst *AI = nullptr;
    if (auto GEP = dyn_cast<GetElementPtrInst>(ptr)) {
        AI = dyn_cast<AllocaInst>(GEP->getPointerOperand());
        if (!AI) {
            AI = VR.getEqualValue<AllocaInst>(GEP, GEP->getPointerOperand());
        }
    }

    if (!AI)
        return;

    auto it = pred_begin(entry);
    BasicBlock *predBlock = *it;
    if (blocks.count(*it) > 0) {
        ++it;
        predBlock = *it;

        if (blocks.count(*it) > 0 || ++it != pred_end(entry)) {
            return;
        }
    } else if (++it != pred_end(entry)) {
        return;
    }

    assert(predBlock);
    auto reads = VR.getReadsFromAlloca(predBlock->getTerminator(), AI);
    if (reads.empty())
        return;
    bool hasTheValue = false;
    for (auto& it : reads) {
        if (it.second == value) {
            hasTheValue = true;
            break;
        }

        auto C1 = dyn_cast<ConstantInt>(it.second);
        auto C2 = dyn_cast<ConstantInt>(value);
        if (C1 && C2 && C1->getZExtValue() == C2->getZExtValue()) {
            hasTheValue = true;
            break;
        }
    }

    if (!hasTheValue)
        return;

    // get the counter variable
    const AllocaInst *counter = nullptr;
    if (auto GEP = dyn_cast<GetElementPtrInst>(ptr)) {
        const LoadInst *LI = dyn_cast<LoadInst>(GEP->getOperand(1));
        if (!LI) {
            LI = VR.getEqualValue<LoadInst>(ptr, GEP->getOperand(1));
        }
        if (!LI)
            return;

        counter = dyn_cast<AllocaInst>(LI->getPointerOperand());
        if (!counter)
            counter = VR.getEqualValue<AllocaInst>(ptr, LI->getPointerOperand());
    }

    if (!counter)
        return;

    hasTheValue = false;
    reads = VR.getReadsFromAlloca(predBlock->getTerminator(), counter);
    if (reads.empty())
        return;
    for (auto& it : reads) {
        auto C1 = dyn_cast<ConstantInt>(it.second);
        if (C1 && C1->getZExtValue() == 0) {
            hasTheValue = true;
            break;
        }
    }

    if (!hasTheValue)
        return;

    // we know that counter starts with 0,
    // check that the counter is being increased by 1
    // and check that all accesses to the alloca are through counter
    // => all accesses to that alloca are ok
    std::set<llvm::Value *> fineDerefs;
    bool counter_ok = false;
    for (auto blk : blocks) {
        for (auto& I : *blk) {
            GetElementPtrInst *GEP = nullptr;
            if (auto SI = dyn_cast<StoreInst>(&I)) {
                GEP = dyn_cast<GetElementPtrInst>(SI->getPointerOperand());

                if (VR.areEqual(counter, SI->getPointerOperand())) {
                    if (counter_ok) // another write to the counter?
                        return;
                    // write to counter -- check that we are writing
                    // the previous value increased by 1
                    auto value = dyn_cast<BinaryOperator>(SI->getOperand(0));
                    if (!value) {
                        if (auto c = dyn_cast<CastInst>(SI->getOperand(0)))
                            value = dyn_cast<BinaryOperator>(c->getOperand(0));
                    }
                    if (!value || (value->getOpcode() != Instruction::Add))
                        return;

                    auto from = value->getOperand(0);
                    auto addition = dyn_cast<ConstantInt>(value->getOperand(1));
                    if (!addition) {
                        addition = dyn_cast<ConstantInt>(value->getOperand(0));
                        from = value->getOperand(1);
                    }

                    if (addition->getZExtValue() != 1)
                        return;

                    if (auto LI = dyn_cast<LoadInst>(from)) {
                        if (!VR.areEqual(counter, LI->getOperand(0)))
                            return;
                    } else
                        return;

                    counter_ok = true;
                } else if (VR.areEqual(AI, SI->getPointerOperand()) ||
                           (GEP && VR.areEqual(AI, GEP->getOperand(0)))) {
                    // are the writes to the alloca through the counter?
                    if (!GEP || GEP->hasAllConstantIndices())
                        continue;

                    const LoadInst *value = dyn_cast<LoadInst>(GEP->getOperand(1));
                    if (!value) {
                        if (auto c = dyn_cast<CastInst>(GEP->getOperand(1))) {
                            value = dyn_cast<LoadInst>(c->getOperand(0));
                            if (!value) {
                                if (auto equals = VR.getEquals(c, c->getOperand(0))) {
                                    for (auto eq : *equals) {
                                        if (auto LI = dyn_cast<LoadInst>(eq))
                                            if (VR.areEqual(LI->getOperand(0), counter))
                                                value = LI;
                                    }
                                }
                            }
                        }
                    }
                    if (!value)
                        return;

                    if (!VR.areEqual(value->getOperand(0), counter))
                        return;
                    // fine, access through counter
                    fineDerefs.insert(GEP);
                    errs() << "OK : " << *SI << "\n";
                } else if (VR.getEqualValue<AllocaInst>(SI, SI->getPointerOperand())) {
                    // write to some different *known* memory
                    continue;
                } else {
                    return;
                }


            ////
            /// Load Inst
            ////
            } else if (auto LI = dyn_cast<LoadInst>(&I)) {
                // are reads from the alloca through the counter?
                GEP = dyn_cast<GetElementPtrInst>(LI->getPointerOperand());
                if (VR.areEqual(AI, LI->getPointerOperand()) ||
                           (GEP && VR.areEqual(AI, GEP->getOperand(0)))) {
                    // are the writes to the alloca through the counter?
                    if (!GEP || GEP->hasAllConstantIndices())
                        continue;

                    const LoadInst *value = dyn_cast<LoadInst>(GEP->getOperand(1));
                    if (!value) {
                        if (auto c = dyn_cast<CastInst>(GEP->getOperand(1))) {
                            value = dyn_cast<LoadInst>(c->getOperand(0));
                            if (!value) {
                                if (auto equals = VR.getEquals(c, c->getOperand(0))) {
                                    for (auto eq : *equals) {
                                        if (auto LI = dyn_cast<LoadInst>(eq))
                                            if (VR.areEqual(LI->getOperand(0), counter))
                                                value = LI;
                                    }
                                }
                            }
                        }
                    }
                    if (!value)
                        return;

                    if (!VR.areEqual(value->getOperand(0), counter))
                        return;
                    // fine, access through counter
                    fineDerefs.insert(GEP);
                    errs() << "OK : " << *LI << "\n";
                } else if (VR.getEqualValue<AllocaInst>(LI, LI->getPointerOperand())) {
                    // read from some different *known* memory
                    continue;
                } else {
                    return;
                }
            }
        }
    }

    if (!counter_ok)
        return;
    // we are here, we survived!
    ok_instructions.insert(fineDerefs.begin(), fineDerefs.end());
}

void ValueRelationsPlugin::handleIterateUntilLessThan(llvm::Value *ptr, llvm::Value *value,
                                                      llvm::BasicBlock *entry,
                                                      std::set<llvm::BasicBlock *>& blocks)
{
}

void ValueRelationsPlugin::processCycle(const std::vector<llvm::BasicBlock *>& cycle) {
    using namespace llvm;

    //errs() << "Cycle of length " << cycle.size() << "\n";

    std::set<BasicBlock *> blocks;
    BasicBlock *entry = cycle[0];
    BasicBlock *branch = nullptr;
    for (auto blk : cycle) {
        blocks.insert(blk);

        if (!blk->getSingleSuccessor()) {
            if (succ_begin(blk) == succ_end(blk))
                return;
            if (branch)
                return;
            branch = blk;
        }

        // we handle only single entry cycles
        if (blk == entry)
            continue;

        if (!blk->getSinglePredecessor())
            return;
    }

    auto iter = iterateUntilValue(branch, blocks);
    if (iter.first) {
        /*
        errs() << "Iterate until you find " << *iter.second
               << " on " << *iter.first << "\n";
               */
        handleIterateUntilValue(iter.first, iter.second, entry, blocks);
        return;
    }

    iter = iterateUntilLessThan(branch, blocks);
    if (iter.first) {
        /*
        errs() << "Iterate until  " << *iter.first
               << " is less than " << *iter.second << "\n";
        handleIterateUntilValue(iter.first, iter.second, entry, blocks);
        */
        return;
    }
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new ValueRelationsPlugin(module);
}
