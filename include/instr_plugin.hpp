#ifndef INSTR_PLUGIN_H
#define INSTR_PLUGIN_H

#include <string>

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Operator.h>

class InstrPlugin
{
    private:
        std::string name;
    public:
      // the default behaviour is returning true, since we have
      // no information about the value, so it may be anything
      virtual std::string isNull(llvm::Value*) {
          return "unknown";
      }

      // XXX: we should probably rename it to isValidPointerRange
      virtual std::string isValidPointer(llvm::Value*, llvm::Value *) {
          return "unknown";
      }

      virtual std::string pointsTo(llvm::Value*, llvm::Value*) {
          return "unknown";
      }

      virtual std::string isConstant(llvm::Value* a) {
          if (llvm::isa<llvm::Constant>(a))
              return "true";
          else
              return "false";
      }

      virtual std::string canOverflow(llvm::Value *a) {
          if (llvm::OverflowingBinaryOperator *O
              = llvm::dyn_cast<llvm::OverflowingBinaryOperator>(a)) {
              if (!O->hasNoSignedWrap())
                  return "true";
              else
                  return "false";
          }

          return "false";
      }

      virtual std::string hasKnownSize(llvm::Value*) {
          return "unknown";
      }

      std::string getName() { return name; }

      InstrPlugin() {}
      InstrPlugin(const std::string& pluginName) : name(pluginName) {}

      // add virtual destructor, so that child classes will
      // call their destructor
      virtual ~InstrPlugin() {}
};

#endif
