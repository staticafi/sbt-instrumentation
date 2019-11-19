#ifndef PREDATOR_PLUGIN_H
#define PREDATOR_PLUGIN_H

#include <unordered_map>
#include <unordered_set>

#include <llvm/IR/Module.h>
#include "instr_plugin.hpp"

class PredatorPlugin : public InstrPlugin
{
private:

    enum class ErrorType {
        Invalid,
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

        void add(unsigned line, unsigned col, T r) {
            auto it = container.find(std::make_pair(line, col));
            if (it != container.end()) {
                it->second.insert(std::move(r));
            } else {
                container[std::make_pair(line, col)].insert(r);
            }
        }
    };

    using ErrorReport = ErrorType;

    ErrorContainer<ErrorReport> errors;

    /**
     * Return true if any user of @operand has an associated error report of type @et
     */
    bool someUserHasErrorReport(const llvm::Value* operand, ErrorType et) const;

    void loadPredatorOutput();
    void runPredator(llvm::Module* mod);

    bool predatorSuccess;

public:
    PredatorPlugin(llvm::Module* module) : InstrPlugin("Predator") {
        predatorSuccess = false;
        llvm::errs() << "PredatorPlugin: Running Predator...\n";
        runPredator(module);
        loadPredatorOutput();
    }

    bool supports(const std::string& query) override;

    std::string query(const std::string& query,
                      const std::vector<llvm::Value *>& operands) {

        if (!predatorSuccess) {
            return "maybe";
        }

        if (query == "isInvalid") {
            assert(operands.size() == 1);
            if (someUserHasErrorReport(operands[0], ErrorType::Invalid)) {
                return "maybe";
            } else {
                return "false";
            }
        } else if (query == "isValidPointer") {
            assert(operands.size() == 2);
            if (someUserHasErrorReport(operands[0], ErrorType::Invalid)) {
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
