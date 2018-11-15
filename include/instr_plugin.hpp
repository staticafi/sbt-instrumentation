#ifndef INSTR_PLUGIN_H
#define INSTR_PLUGIN_H

#include <string>

class InstrPlugin
{
    private:
      std::string name{};

    public:
      virtual bool supports(const std::string& query) = 0;
      virtual std::string query(const std::string& query,
                                const std::vector<llvm::Value *>& operands) = 0;

      const std::string& getName() { return name; }

      InstrPlugin() {}
      InstrPlugin(const std::string& pluginName) : name(pluginName) {}

      // Add virtual destructor, so that child classes will
      // call their destructor
      virtual ~InstrPlugin() {}
};

#endif
