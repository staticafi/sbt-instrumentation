#include <catch2/catch.hpp>
#include "json/json.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IRReader/IRReader.h>

#include "value_relations_plugin.hpp"
#include "getValName.h"

#include <string>
#include <iostream>
#include <cstdio>
#include <fstream>

bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

std::string compileBenchmark(const std::string& path, const std::string& benchmark) {
    std::string targetFile = "tmp/" + benchmark + ".ll";

    if (! fileExists(targetFile)) {
        std::string command = "clang-10 -S -emit-llvm " + path + "/" + benchmark + ".c -o " + targetFile;

        int returnCode = system(command.c_str());
        if (returnCode != 0)
            return {};
    }

    return targetFile;
}

uint64_t getAllocatedSize(llvm::Instruction* I, const llvm::Module& M) {
    llvm::Type* Ty;

    if (const llvm::AllocaInst *AI = llvm::dyn_cast<llvm::AllocaInst>(I)) {
        Ty = AI->getAllocatedType();
    }
    else if (const llvm::StoreInst *SI = llvm::dyn_cast<llvm::StoreInst>(I)) {
        Ty = SI->getOperand(0)->getType();
    }
    else if (const llvm::LoadInst *LI = llvm::dyn_cast<llvm::LoadInst>(I)) {
        Ty = LI->getType();
    }
    else {
        return 0;
    }

    if(!Ty->isSized())
        return 0;

    return M.getDataLayout().getTypeAllocSize(Ty);
}

llvm::Value* getPtr(llvm::Instruction& inst) {
    llvm::Value* result;
    if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
        assert(load->getNumOperands() == 1 && "load has only operand");
        return load->getPointerOperand();
    } else if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
        assert(store->getNumOperands() == 2 && "store has two operands");
        return store->getPointerOperand();
    }
    return nullptr;
}

llvm::Value* getSize(llvm::Module& module, llvm::Instruction& inst) {
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(module.getContext()), getAllocatedSize(&inst, module));
}

enum class CheckType {
    REQUIRE_TRUE,
    PRINT_ALL,
    PRINT_NONE
};

void testBenchmark(const std::string& path, const std::string& benchmark, CheckType type) {
    auto targetFile = compileBenchmark(path, benchmark);

    llvm::LLVMContext context;
    llvm::SMDiagnostic SMD;

    auto module = llvm::parseIRFile(targetFile, SMD, context);
    REQUIRE(module);

    ValueRelationsPlugin plugin(module.get());

    for (auto& function : *module) {
        for (auto& block : function) {
            for (auto& inst : block) {
                llvm::Value* ptr = getPtr(inst);
                llvm::Value* size = getSize(*module, inst);

                if (! ptr || size == 0)
                    continue;

                std::string answer = plugin.query("isValidPointer", { ptr, size });

                if (llvm::isa<llvm::GetElementPtrInst>(ptr)) {
                    if (type == CheckType::REQUIRE_TRUE) {
                        DYNAMIC_SECTION(dg::debug::getValName(ptr)) {
                            REQUIRE(answer == "true");
                        }
                    } else if (type == CheckType::PRINT_ALL) {
                        INFO(dg::debug::getValName(ptr));
                        INFO(answer);
                        CHECK(false); // force printing of info messages
                    }
                }
            }
        }
    }
}

void testGroup(const std::string& benchmarksPath, Json::Value& group, CheckType type) {
        Json::Value folder = GENERATE_REF(from_range(group.begin(), group.end()));
        std::string folderPath = benchmarksPath + folder["name"].asString();

        const Json::Value& benchmark = GENERATE_REF(from_range(folder["benchmarks"].begin(), folder["benchmarks"].end()));

        DYNAMIC_SECTION(folder["name"].asString() << "/" << benchmark.asString()) {
            testBenchmark(folderPath, benchmark.asString(), type);
        }
}

TEST_CASE("main") {

    std::ifstream tests("tested-files.json");
    REQUIRE(tests.good());

    Json::Value root;
    tests >> root;

    const std::string& benchmarksPath = root["benchmarks_path"].asString();
    Json::Value groups = root["benchmark_groups"];

    SECTION("all_correct") {
        testGroup(benchmarksPath, groups["all_correct"], CheckType::REQUIRE_TRUE);
    }

    SECTION("problematic") {
        testGroup(benchmarksPath, groups["problematic"], CheckType::PRINT_ALL);
    }

    SECTION("many_01") {
        testGroup(benchmarksPath, groups["many_01"], CheckType::PRINT_NONE);
    }
}