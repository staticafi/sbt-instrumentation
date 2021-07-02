#include "range_analysis_plugin.hpp"
#include <limits>
#include <cmath>

#include <llvm/IR/Operator.h>

using namespace llvm;

bool checkUnknown(const Range& a, const Range& b) {
    if (a.isRegular() && b.isRegular())
        return false;

    return true;
}

std::string RangeAnalysisPlugin::canBeZero(Value* value) {
    // Support only instructions
    auto* inst = dyn_cast<Instruction>(value);
    if (!inst)
        return "maybe";

    auto result = RA.find(inst->getFunction());
    ConstraintGraph& CG = result->second;
    Range r = getRange(CG, value);

    if (!r.isRegular())
        return "maybe";

    double x = r.getLower().signedRoundToDouble();
    double y = r.getUpper().signedRoundToDouble();

    if (x > 0)
        return "false";

    if (y < 0)
        return "false";

    return "true";
}

std::string RangeAnalysisPlugin::canOverflow(Value* value) {
    // Support only instructions
    auto* inst = dyn_cast<Instruction>(value);
    if (!inst)
        return "unknown";

    // Support only instruction of integer type
    const auto* intT = dyn_cast<IntegerType>(inst->getType());
    if (!intT)
        return "unknown";

    auto result = RA.find(inst->getFunction());
    if (result == RA.end())
        return "unknown";

    ConstraintGraph& CG = result->second;

    if (const auto* binOp
        = dyn_cast<OverflowingBinaryOperator>(inst)) {
        // we are looking for bin. ops with nsw
        if (!binOp->hasNoSignedWrap())
            return "false";

        Range a = getRange(CG, binOp->getOperand(0));
        Range b = getRange(CG, binOp->getOperand(1));

        // check for unknown ranges
        if (checkUnknown(a, b))
            return "unknown";

        // check addition
        if (isa<AddOperator>(inst))
            return canOverflowAdd(a, b, *intT);

        // check subtraction
        if (isa<SubOperator>(inst))
            return canOverflowSub(a, b, *intT);

        // check multiplication
        if (isa<MulOperator>(inst))
            return canOverflowMul(a, b, *intT);
    }

    // check division
    if (const auto* binOp
        = dyn_cast<SDivOperator>(inst)) {
        Range a = getRange(CG, binOp->getOperand(0));
        Range b = getRange(CG, binOp->getOperand(1));
        if (checkUnknown(a, b))
            return "true";
        return canOverflowDiv(a, b, *intT);
    }

    // check trunc
    if (const auto* truncOp
        = dyn_cast<TruncInst>(inst)) {
        Range a = getRange(CG, truncOp->getOperand(0));
        return canOverflowTrunc(a, *truncOp);
    }


    // check shifts
    if (const auto* binOp = dyn_cast<ShlOperator>(inst)) {
        Range a = getRange(CG, binOp->getOperand(0));
        Range b = getRange(CG, binOp->getOperand(1));
        return canOverflowShl(a, b, *intT);
    }

    return "unknown";
}

std::string RangeAnalysisPlugin::canOverflowShl(const Range& a, const Range& b,
                                                const IntegerType& Ty) {
  assert(Ty.getBitWidth() <= 64);
  if (!a.isRegular() || !b.isRegular())
    return "unknown";

  if (b.getUpper().getZExtValue() >= a.getLower().getBitWidth())
    return "true";

  // invalid number of positions
  if (b.getUpper().getZExtValue() >= Ty.getBitWidth())
    return "true";

  uint64_t max = (1UL << (Ty.getBitWidth() - 1)) - 1;
  uint64_t e = 1UL << b.getUpper().getZExtValue();
  llvm::errs() << "max: " << max << "\n";
  if (a.getLower().isNegative()) {
    auto l = a.getLower();
    l.negate();
    if (l.getZExtValue() > (max >> e))
      return "true";
  } else if (!a.getLower().isNegative()) {
    if (a.getUpper().getZExtValue() > ((max - 1) >> e))
      return "true";
  }
  // else a == 0, which is fine

  return "false";
}


std::string RangeAnalysisPlugin::canOverflowTrunc(const Range& a,
        const TruncInst& truncOp)
{
    if (!a.isRegular())
        return "unknown";

    double x = a.getLower().signedRoundToDouble();
    double y = a.getUpper().signedRoundToDouble();

    const auto* t = dyn_cast<IntegerType>(truncOp.getDestTy());
    if (!t)
        return "unknown";

    if (y > 0 && y > (std::pow(2, t->getBitWidth() - 1) - 1))
        return "true";

    if (x < 0 && x < (-std::pow(2, t->getBitWidth() - 1)))
        return "true";

    return "false";
}

Range RangeAnalysisPlugin::getRange(ConstraintGraph& CG,
                        llvm::Value* val)
{
    Range r = CG.getRange(val);
    if (r.isUnknown()) {
        if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(val))
            return CG.getRange(loadInst->getOperand(0));
    }

    return r;
}

bool checkOverflowAdd(const APInt& ax, const APInt& ay, const IntegerType& t) {
    double x = ax.signedRoundToDouble();
    double y = ay.signedRoundToDouble();

    if((x > 0) && (y > 0) &&
       (x > (std::pow(2, t.getBitWidth() - 1) - 1) - y))\
    return true;

    if((x < 0) && (y < 0) && (x < (-std::pow(2, t.getBitWidth() - 1)) - y))
        return true;

    return false;
}

std::string RangeAnalysisPlugin::canOverflowAdd(const Range& a,
        const Range& b, const IntegerType& t)
{
    if (checkOverflowAdd(a.getUpper(), b.getUpper(), t))
        return "true";

    if (checkOverflowAdd(a.getLower(), b.getLower(), t))
        return "true";

    return "false";
}

bool checkOverflowSub(APInt ax, APInt ay, const IntegerType& t) {
    double x = ax.signedRoundToDouble();
    double y = ay.signedRoundToDouble();

    if((y > 0) && (x < (-std::pow(2, t.getBitWidth() - 1)) +  y))
        return true;

    if((y < 0) && (x > (std::pow(2, t.getBitWidth() - 1) - 1) + y))
        return true;

    return false;
}

std::string RangeAnalysisPlugin::canOverflowSub(const Range& a,
        const Range& b, const IntegerType& t)
{
    if (checkOverflowSub(a.getUpper(), b.getUpper(), t))
        return "true";

    if (checkOverflowSub(a.getLower(), b.getLower(), t))
        return "true";

    if (checkOverflowSub(a.getUpper(), b.getLower(), t))
        return "true";

    if (checkOverflowSub(a.getLower(), b.getUpper(), t))
        return "true";

    return "false";
}

bool checkOverflowMul(APInt ax, APInt ay, const IntegerType& t) {
    double x = ax.signedRoundToDouble();
    double y = ay.signedRoundToDouble();

    if (x == 0 || y == 0)
        return false;

    if (x > (std::pow(2, t.getBitWidth() - 1) - 1) / y)
        return true;

    if (x < (-std::pow(2, t.getBitWidth() - 1)) / y)
        return true;

    return false;
}

std::string RangeAnalysisPlugin::canOverflowMul(const Range& a,
        const Range& b, const IntegerType& t)
{
    if (checkOverflowMul(a.getUpper(), b.getUpper(), t))
        return "true";

    if (checkOverflowMul(a.getLower(), b.getLower(), t))
        return "true";

    if (checkOverflowMul(a.getUpper(), b.getLower(), t))
        return "true";

    if (checkOverflowMul(a.getLower(), b.getUpper(), t))
        return "true";

    if (a.getLower().signedRoundToDouble() <= (-std::pow(2, t.getBitWidth()))
         && b.getLower().signedRoundToDouble() <= -1
         && b.getUpper().signedRoundToDouble() >= -1)
        return "true";

    if (b.getLower().signedRoundToDouble() <= (-std::pow(2, t.getBitWidth()))
         && a.getLower().signedRoundToDouble() <= -1
         && a.getUpper().signedRoundToDouble() >= -1)
        return "true";

    return "false";
}

std::string RangeAnalysisPlugin::canOverflowDiv(const Range& a,
        const Range& b, const IntegerType& t)
{
    // TODO: Check also for division by zero?
    // if (b.getLower() <= 0 && b.getUpper() >= 0)
    //    return "true";

    if (a.getLower().signedRoundToDouble() <= (-std::pow(2, t.getBitWidth() - 1))
         && b.getLower().signedRoundToDouble() <= -1
         && b.getUpper().signedRoundToDouble() >= -1)
        return "true";

    return "false";
}

static const std::string supportedQueries[] = {
    "canOverflow",
    "canBeZero",
};

bool RangeAnalysisPlugin::supports(const std::string& query) {
    for (unsigned idx = 0; idx < sizeof(supportedQueries)/sizeof(*supportedQueries); ++idx) {
        if (query == supportedQueries[idx])
            return true;
    }

    return false;
}

extern "C" InstrPlugin* create_object(llvm::Module* module) {
    return new RangeAnalysisPlugin(module);
}
