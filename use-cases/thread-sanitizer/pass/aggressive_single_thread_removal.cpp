#include "tsan_slicing_cleanup.h"

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

#include <cassert>

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

          if (is_thread_function(called_func))
            if (not is_omp_function(called_func))
              return false;
        }
      }
    }
  }

  return true;
}

static void collectAllParallelFunctions(Function *func, bool parallel = false) {

  if (parallel_functions.contains(func))
    return;
  if (parallel)
    parallel_functions.insert(func);

  for (auto &bb : *func) {
    for (auto &inst : bb) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {
        auto *called_func = call->getCalledFunction();
        if (parallel) {
          if (called_func) {
            if (is_thread_function(called_func)) {
              for (Use &a : call->args())
                if (auto target_func = dyn_cast<Function>(a.get()))
                  collectAllParallelFunctions(target_func, parallel);
            } else {
              collectAllParallelFunctions(called_func, parallel);
            }
          } else {
            // TODO function pointer? indirect calls?
            for (auto *ct : DevirtAnalysis::get_possible_call_targets(call)) {
              assert(ct);
              collectAllParallelFunctions(ct, parallel);
            }
          }
        } else {
          if (not called_func)
            continue;
          if (not is_thread_function(called_func))
            continue;

          if (is_omp_function(called_func)) {
            for (Use &a : call->args())
              if (auto omp_target_func = dyn_cast<Function>(a.get()))
                collectAllParallelFunctions(omp_target_func, true);
          } else {
            llvm_unreachable("did someone change check_module()?");
          }
        }
      }
    }
  }
}

static void collect_and_cleanup(Module &M, unsigned *removed_tsan_calls) {
  // assert(not parallel_functions.empty());
  for (Function &Func : M) {
    // ignore tsan itself
    if (Func.getName().starts_with("tsan."))
      continue;

    // remove TSAN calls only in single-threaded functions
    if (parallel_functions.contains(&Func))
      continue;

    SmallVector<Instruction *> to_be_erased;
    for (BasicBlock &BB : Func)
      for (Instruction &Inst : BB)
        if (auto *call = dyn_cast<CallBase>(&Inst))
          if (isAcceptableTsanCall(call))
            to_be_erased.push_back(call);

    for (auto *Inst : to_be_erased) {
      (*removed_tsan_calls)++;
      remove_inst_from_func(Inst);
    }
  }
}

std::string remove_all_single_thread_regions(Module &M,
                                             ModuleAnalysisManager &AM) {
  errs() << "Aggressivly remove all single-threaded TSAN calls\n";
  if (not check_module(M))
    return "";

  for (auto &func : M) {
    // ignore tsan itself
    if (func.getName().starts_with("tsan"))
      continue;
    collectAllParallelFunctions(&func);
  }

  unsigned removed_tsan_calls = 0;
  collect_and_cleanup(M, &removed_tsan_calls);

  return "single-thread removal: " + std::to_string(removed_tsan_calls);
}
