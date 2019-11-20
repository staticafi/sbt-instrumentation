#ifndef PREDATOR_PLUGIN_H
#define PREDATOR_PLUGIN_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <llvm/IR/Module.h>
#include "instr_plugin.hpp"

class PredatorPlugin : public InstrPlugin
{
private:

    enum class ErrorType {
        Invalid,
        Leak,
        Free,
    };

    struct PairHash {
        template <class T1, class T2>
        std::size_t operator() (const std::pair<T1, T2> &pair) const {
            return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
        }
    };

    template<typename T>
    class ErrorContainer {
        std::unordered_map<std::pair<unsigned, unsigned>, std::unordered_set<T>, PairHash> container;

    public:
        ErrorContainer() = default;

        bool hasReport(size_t line, size_t col, const T& r) const {
            auto it = container.find(std::make_pair(line, col));
            if (it == container.end()) {
                return false;
            } else {
                return it->second.find(r) != it->second.end();
            }
        }

        bool hasAnyReport(size_t line, size_t col) const {
            auto it = container.find(std::make_pair(line, col));
            if (it != container.end()) {
                return !it->second.empty();
            }

            return false;
        }

        void add(unsigned line, unsigned col, T r) {
            auto it = container.find(std::make_pair(line, col));
            if (it != container.end()) {
                it->second.insert(std::move(r));
            } else {
                container[std::make_pair(line, col)].insert(r);
            }
        }

        bool empty() const {
            return container.empty();
        }
    };

    using ErrorReport = ErrorType;

    ErrorContainer<ErrorReport> errors;
    std::vector<unsigned> lineOnlyErrors;
    std::unordered_set<const llvm::Value *> dangerous;

    /**
     * Return true if any user of @operand has an associated error report of type @et
     */
    bool someUserHasErrorReport(const llvm::Value* operand, ErrorType et) const;
    bool someUserHasSomeErrorReport(const llvm::Value* operand) const;

    void addReportsForLineErrors(llvm::Module* mod);

    void loadPredatorOutput();
    void runPredator(llvm::Module* mod);

    bool isDangerous(const llvm::Value* v) const {
        return dangerous.find(v) != dangerous.end();
    }

    bool predatorSuccess;

public:
    PredatorPlugin(llvm::Module* module) : InstrPlugin("Predator") {
        predatorSuccess = false;
        llvm::errs() << "PredatorPlugin: Running Predator...\n";
        runPredator(module);
        loadPredatorOutput();
        addReportsForLineErrors(module);
    }

    bool supports(const std::string& query) override;

    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands) {

        if (!predatorSuccess) {
            return "maybe";
        }

        if (query == "isInvalid") {
            assert(operands.size() == 1);
            if (someUserHasSomeErrorReport(operands[0]) || isDangerous(operands[0])) {
                return "maybe";
            } else {
                return "false";
            }
        } else if (query == "isValidPointer") {
            assert(operands.size() == 2);
            if (someUserHasSomeErrorReport(operands[0]) || isDangerous(operands[0])) {
                return "maybe";
            } else {
                return "true";
            }
        } else if (query == "mayBeLeaked") {
            assert(operands.size() == 1);
            if (someUserHasSomeErrorReport(operands[0]) || isDangerous(operands[0])) {
                return "true";
            } else {
                return "false";
            }
        } else if (query == "mayBeLeakedOrFreed") {
            assert(operands.size() == 1);
            if (someUserHasSomeErrorReport(operands[0]) || isDangerous(operands[0])) {
                return "true";
            } else {
                return "false";
            }
        } else if (query == "safeForFree") {
            assert(operands.size() == 1);
            if (someUserHasSomeErrorReport(operands[0]) || isDangerous(operands[0])) {
                return "maybe";
            } else {
                return "true";
            }
        }

        return "unsupported query";
    }

    virtual ~PredatorPlugin() = default;
};


#endif /* PREDATOR_PLUGIN_H */
