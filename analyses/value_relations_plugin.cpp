#include "value_relations_plugin.hpp"

#include "dg/llvm/ValueRelations/GraphBuilder.hpp"
#include "dg/llvm/ValueRelations/StructureAnalyzer.hpp"
#include "dg/llvm/ValueRelations/RelationsAnalyzer.hpp"

ValueRelationsPlugin::ValueRelationsPlugin(llvm::Module* module)
: InstrPlugin("ValueRelationsPlugin") {
    assert(module);
    dg::analysis::vr::GraphBuilder gb(*module, locationMapping, blockMapping);
    gb.build();

    dg::analysis::vr::StructureAnalyzer sa(*module, locationMapping, blockMapping);        
    dg::analysis::vr::RelationsAnalyzer ra(*module, locationMapping, blockMapping, sa);
    ra.analyze(maxPass);
}

// returns number of bytes the elements of given type occupy
uint64_t getBytes(const llvm::Type* type) {
    unsigned byteWidth = 8;
    assert(type->isSized());

    uint64_t size = type->getPrimitiveSizeInBits();
    assert (size % byteWidth == 0);

    return size / byteWidth;
}

// returns number of allocated elements and element's size in bytes
std::pair<const llvm::Value*, uint64_t> getAllocatedCountAndSize(
        const dg::analysis::vr::RelationsGraph& relations,
        const llvm::GetElementPtrInst* gep) {
    for (const llvm::Value* equal : relations.getEqual(gep->getPointerOperand())) {

        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(equal)) {
            const llvm::Type* allocatedType = alloca->getAllocatedType();
            const llvm::Value* count = nullptr;
            uint64_t size = 0;
            
            if (allocatedType->isArrayTy()) {
                const llvm::Type* elemType = allocatedType->getArrayElementType();
                // DANGER just an arbitrary type
                llvm::Type* i32 = llvm::Type::getInt32Ty(elemType->getContext());
                uint64_t intCount = allocatedType->getArrayNumElements();
                count = llvm::ConstantInt::get(i32, intCount);
                size = getBytes(elemType);
            } else {
                count = alloca->getOperand(0);
                size = getBytes(allocatedType);
            }
            return { count, size };
        } else if (auto call = llvm::dyn_cast<llvm::CallInst>(equal)) {
            auto function = call->getCalledFunction();

            if (! function) return { nullptr, 0 };

            if (function->getName().equals("malloc"))
                return { call->getOperand(0), 1};

            if (function->getName().equals("calloc")) {
                if (auto size = llvm::dyn_cast<llvm::ConstantInt>(call->getOperand(1)))
                    return { call->getOperand(0), size->getZExtValue() };
            }

            if (function->getName().equals("realloc"))
                return { call->getOperand(1), 1 };
        }
    }
    return { nullptr, 0 };
}

// if gep has any zero indices at the beginning, function returns first non-zero index
// which is not followed by any other index
// else it returns nullptr instead of the index
std::pair<const llvm::Value*, const llvm::Type*> getOnlyNonzeroIndex(const llvm::GetElementPtrInst* gep) {
    const llvm::Value* firstIndex = nullptr;
    const llvm::Type* readType = gep->getSourceElementType();

    for (const llvm::Value* index : gep->indices()) {
        // consider only cases when nonzero index is the last
        if (firstIndex) return { nullptr, nullptr };

        if (auto constIndex = llvm::dyn_cast<llvm::ConstantInt>(index)) {
            if (constIndex->isZero()) {
                assert(readType->isArrayTy());
                readType = readType->getArrayElementType();
                continue;
            };
        }

        firstIndex = index;
    }
    return { firstIndex, readType };
}

// if the count of allocated element is result of arithmetic operation, returns
// elem count and possible new size of element before the operation
std::pair<const llvm::Value*, uint64_t> stripArithmeticOp(const llvm::Value* allocCount, uint64_t allocElem) {
    while (auto cast = llvm::dyn_cast<llvm::CastInst>(allocCount))
        allocCount = cast->getOperand(0);

    if (! llvm::isa<llvm::BinaryOperator>(allocCount)) return { nullptr, 0 };

    auto op = llvm::cast<llvm::BinaryOperator>(allocCount);
    if (op->getOpcode() != llvm::Instruction::Add
            && op->getOpcode() != llvm::Instruction::Mul)
        // TODO could also handle subtraction of negative constant
        return { nullptr, 0 };

    auto c1 = llvm::dyn_cast<llvm::ConstantInt>(op->getOperand(0));
    auto c2 = llvm::dyn_cast<llvm::ConstantInt>(op->getOperand(1));

    if (c1 && c2) {
        llvm::APInt result;
        switch (op->getOpcode()) {
            case llvm::Instruction::Add:
                result = c1->getValue() + c2->getValue(); break;
            case llvm::Instruction::Mul:
                result = c1->getValue() * c2->getValue(); break;
            default:
                assert (0 && "stripArithmeticOp: unreachable");
        }
        return { llvm::ConstantInt::get(c1->getType(), result), allocElem };
    }

    // TODO use more info here
    if (! c1 && ! c2) return { nullptr, 0 };

    const llvm::Value* param = nullptr;
    if (c2) { c1 = c2; param = op->getOperand(0); }
    else param = op->getOperand(1);

    assert (c1 && param);

    switch(op->getOpcode()) {
        case llvm::Instruction::Add:
            return { param, allocElem };
        case llvm::Instruction::Mul:
            return { param, allocElem * c1->getZExtValue() };
        default:
            return { nullptr, 0 };
    }
}

// returns the verdict of gep validity for given relations graph
std::string isValidForGraph(
        dg::analysis::vr::RelationsGraph& relations,
        const llvm::GetElementPtrInst* gep,
        uint64_t readSize) {    
    //std::cerr << "==== PROOF BEGINS =====" << std::endl;
    //std::cerr << dg::debug::getValName(gep) << std::endl << std::endl;

    //relations.ddump(); // mark parameter const when deleting

    const llvm::Value* allocCount;
    uint64_t allocElem;
    std::tie(allocCount, allocElem) = getAllocatedCountAndSize(relations, gep);
    // if we do not know the size of allocated memory
    if (! allocCount) return "unknown";

    if (gep->hasAllZeroIndices()) return readSize <= allocElem ? "true" : "maybe";
    // maybe, because can read i64 from an array of two i32

    const llvm::Value* gepIndex;
    const llvm::Type* gepType;
    std::tie(gepIndex, gepType) = getOnlyNonzeroIndex(gep);
    // if gep has more indices than one, or there are zeros after
    if (! gepIndex) return "unknown";

    uint64_t gepElem = getBytes(gepType);

    //std::cerr << "[readSize] " << readSize << std::endl;
    //std::cerr << "[allocElem] " << allocElem << std::endl;
    //std::cerr << "[gepElem] " << gepElem << std::endl;
    //std::cerr << "[allocCount] " << dg::debug::getValName(allocCount) << std::endl;
    //std::cerr << "[gepIndex] " << dg::debug::getValName(gepIndex) << std::endl;

    // DANGER just an arbitrary type
    llvm::Type* i32 = llvm::Type::getInt32Ty(allocCount->getContext());
    const llvm::Constant* zero = llvm::ConstantInt::getSigned(i32, 0);

    // check if index doesnt point before memory
    if (relations.isLesser(gepIndex, zero))
        // we know that pointed memory is alloca because allocCount is not nullptr
        return "false";
    if (! relations.isLesserEqual(zero, gepIndex)) return "maybe";

    // check if index doesnt point after memory
    do {
        //std::cerr << "inloop" << std::endl;
        //std::cerr << "[allocCount] " << dg::debug::getValName(allocCount) << std::endl;
        //std::cerr << "[gepIndex] " << dg::debug::getValName(gepIndex) << std::endl;
        if (relations.isLesser(gepIndex, allocCount)) {
            // TODO handle some cases where size and max index are both constants,
            // but type accessed is different from type allocated
            if (gepElem <= allocElem) return readSize <= allocElem ? "true" : "maybe";
        }
        if (relations.isLesserEqual(allocCount, gepIndex) && gepElem >= allocElem) return "false";

        std::tie(allocCount, allocElem) = stripArithmeticOp(allocCount, allocElem);
    } while (allocCount);

    return "maybe";
}

std::string ValueRelationsPlugin::isValidPointer(llvm::Value* ptr, llvm::Value *size) {
    // ptr is not a pointer
    if (! ptr->getType()->isPointerTy()) return "false";

    uint64_t readSize = 0;
    if (auto* constant = llvm::dyn_cast<llvm::ConstantInt>(size)) {
        readSize = constant->getLimitedValue();

        // size cannot be expressed as uint64_t
        if (readSize == ~((uint64_t) 0)) return "maybe";
    } else {
        // size is not constant int
        return "maybe";
    }

    assert (readSize > 0 && readSize < ~((uint64_t) 0));

    auto gep = llvm::dyn_cast<llvm::GetElementPtrInst>(ptr);
    if (! gep) return "unknown";

    auto& relations = locationMapping.at(gep)->relations;
    if (relations.getCallRelations().empty())
        return isValidForGraph(relations, gep, readSize);

    // else we have to check that access is valid in every case
    for (const dg::analysis::vr::RelationsGraph::CallRelation& callRelation : relations.getCallRelations()) {
        dg::analysis::vr::RelationsGraph merged = relations;
        for (auto& equalPair : callRelation.equalPairs)
            merged.setEqual(equalPair.first, equalPair.second);
        merged.merge(*callRelation.callSiteRelations);
        merged.getCallRelations().clear();

        std::string result = isValidForGraph(merged, gep, readSize);
        if (result != "true") return result;
    }
    return "true";
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new ValueRelationsPlugin(module);
}
