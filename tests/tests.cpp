#include <catch2/catch.hpp>
#include "json/json.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IRReader/IRReader.h>

#include "value_relations_plugin.hpp"
#include "dg/llvm/ValueRelations/getValName.h"

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
        std::string command = "clang-10 -S -emit-llvm " + path + "/" + benchmark + ".i -o " + targetFile;

        int returnCode = system(command.c_str());
        if (returnCode != 0)
            return {};
    }

    return targetFile;
}

uint64_t getAllocatedSize(llvm::Instruction* inst, const llvm::Module& module) {
    llvm::Type* type;

    if (const auto *alloca = llvm::dyn_cast<llvm::AllocaInst>(inst)) {
        type = alloca->getAllocatedType();
    }
    else if (const auto *store = llvm::dyn_cast<llvm::StoreInst>(inst)) {
        type = store->getOperand(0)->getType();
    }
    else if (const auto *load = llvm::dyn_cast<llvm::LoadInst>(inst)) {
        type = load->getType();
    }
    else {
        return 0;
    }

    if(!type->isSized())
        return 0;

    return module.getDataLayout().getTypeAllocSize(type);
}

llvm::Value* getPtr(llvm::Instruction& inst) {
    if (auto *load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
        assert(load->getNumOperands() == 1 && "load has only operand");
        return load->getPointerOperand();
    }
    if (auto *store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
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
    if (targetFile.empty()) {
        INFO("benchmark " << benchmark << " not found");
        CHECK(false);
        return;
    }

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
                        INFO(dg::debug::getValName(ptr));
                        CHECK(answer == "true");
                    } else if (type == CheckType::PRINT_ALL) {
                        INFO(dg::debug::getValName(ptr));
                        INFO(answer);
                        CHECK(false); // force printing of info messages
                    } else {
                        CHECK(true); // count success
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

        std::cerr << benchmark.asString() << "\n";
        INFO(folder["name"].asString() << "/" << benchmark.asString());
        testBenchmark(folderPath, benchmark.asString(), type);
}

TEST_CASE("main") {

    std::ifstream tests("tested-files.json");
    INFO("failed opening tested-files.json")
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

    SECTION("old_problematic") {
        testGroup(benchmarksPath, groups["old_problematic"], CheckType::PRINT_NONE);
    }

    SECTION("many_01") {
        testGroup(benchmarksPath, groups["many_01"], CheckType::PRINT_NONE);
    }
}
