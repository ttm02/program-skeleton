#include "tsan_precompute_cleanup.h"

#include "precompute/compiler/openmp_runtime_functions.h"
#include "precompute/compiler/precalculation_function_analysis.h"
#include "precompute/compiler/std_funcs.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Instrumentation/ThreadSanitizer.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

static DenseSet<Function *> parallel_functions;

static bool check_module(Module &M) {
  // might be a library -> calls from other functions outside the scope possible
  if (not M.getFunction("main"))
    return false;

  for (Function &Func : M) {
    // ignore tsan itself
    if (Func.getName().starts_with("tsan"))
      continue;

    for (BasicBlock &BB : Func) {
      for (Instruction &Inst : BB) {
        if (auto call = dyn_cast<CallBase>(&Inst)) {
          Function *called_func = call->getCalledFunction();
          if (not called_func)
            continue;

          if (is_thread_function(called_func)) {
            if (not is_omp_function(called_func)) {
              // TODO also support pthread fork and join
              return false;
            }
          }
        }
      }
    }
  }

  return true;
}

static void collectAllParallelFunctions(Function *func) {
  // ignore tsan itself
  if (func->getName().starts_with("tsan"))
    return;

  if (parallel_functions.contains(func))
    return;

  // TODO
  parallel_functions.insert(func);
  return;

  for (auto &bb : *func) {
    for (auto &inst : bb) {
      if (auto call = dyn_cast<CallBase>(&inst)) {
        // TODO function pointer? indirect calls?
        auto *called_func = call->getCalledFunction();
        assert(called_func);
        parallel_functions.insert(called_func);
        collectAllParallelFunctions(called_func);
      }
    }
  }
}

static void collect_and_cleanup(Module &M) {
  assert(not parallel_functions.empty());
  for (Function &Func : M) {
    // ignore tsan itself
    if (Func.getName().starts_with("tsan"))
      continue;

    DenseSet<Instruction *> to_be_erased;
    for (BasicBlock &BB : Func) {
      for (Instruction &Inst : BB) {
        if (auto call = dyn_cast<CallBase>(&Inst)) {
          auto *called_func = call->getCalledFunction();
          if (not parallel_functions.contains(called_func))
            to_be_erased.insert(call);
        }
      }
    }
    for (auto *Inst : to_be_erased)
      remove_inst_from_func(Inst);
  }
}

std::string remove_all_single_thread_regions(Module &M,
                                             ModuleAnalysisManager &AM) {
  if (not check_module(M))
    return "";

  for (auto &func : M)
    collectAllParallelFunctions(&func);
  auto func_main = M.getFunction("main");

  collect_and_cleanup(M);

  return "Aggressive removal finished";
}
