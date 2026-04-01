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

void get_called_functions(CallBase *call,
                          SmallDenseSet<Function *> &functions) {
  auto *called_func = call->getCalledFunction();
  if (called_func) {
    functions.insert(called_func);
  } else {
    auto func_targets =
        precalculation_analysis->get_possible_call_targets(call);
    functions.insert_range(func_targets);
  }
}

static bool check_module(Module &M) {
  // might be a library -> calls from other functions outside the scope possible
  if (not M.getFunction("main"))
    return false;

  for (Function &Func : M) {
    // ignore tsan itself
    if (Func.getName().starts_with("tsan."))
      continue;
    if (Func.isDeclaration())
      continue;

    for (BasicBlock &BB : Func) {
      for (Instruction &Inst : BB) {
        if (auto call = dyn_cast<CallBase>(&Inst)) {
          SmallDenseSet<Function *> function_list;
          get_called_functions(call, function_list);
          for (auto *called_func : function_list) {
            assert(called_func);
            if (called_func->isDeclaration())
              continue;

            if (is_thread_function(called_func))
              if (not is_omp_function(called_func))
                return false;
          }
        }
      }
    }
  }

  return true;
}

#define capf(f, p) collectAllParallelFunctions(call_graph, f, p)

static void collectAllParallelFunctions(DenseSet<Function *> &call_graph,
                                        Function *func, bool parallel = false) {

  if (func->isDeclaration())
    return;
  if (parallel_functions.contains(func))
    return;
  if (parallel) {
    parallel_functions.insert(func);
  } else {
    if (call_graph.contains(func))
      return;
    call_graph.insert(func);
  }

#ifndef NDEBUG
  if (func->getName().contains(".omp_outlined")) {
    if (not parallel && 1 < call_graph.size()) {
      errs() << func->getName() << "\n";
      llvm_unreachable("All OpenMP regions should be parallel");
    }
  }
#endif

  for (auto &bb : *func) {
    for (auto &inst : bb) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {

        SmallDenseSet<Function *> function_list;
        get_called_functions(call, function_list);
        assert(not function_list.empty());

        for (auto *ct : function_list) {
          assert(ct);
          if (is_thread_function(ct)) {
            bool isOMP = is_omp_function(ct);
            for (Use &a : call->args())
              if (auto omp_target_func = dyn_cast<Function>(a.get()))
                capf(omp_target_func, isOMP ? true : parallel);
          } else {
            bool is_thread_func = is_thread_function(ct);
            assert(not is_thread_func);
            capf(ct, is_thread_func ? true : parallel);
          }
        }
      }
    }
  }
}

static bool cleanup_call_with_chain(Instruction *inst) {
  auto *call = dyn_cast<CallBase>(inst);
  assert(call);
  auto func_name = getCallName(call);

  auto &M = *call->getModule();
  auto *ctx = &call->getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto int32Ty = Type::getInt32Ty(*ctx);
  auto int64Ty = Type::getInt64Ty(*ctx);

  FunctionCallee func;
  if (func_name == "__tsan_memset")
    func = M.getOrInsertFunction("memset", ptrTy, ptrTy, int32Ty, int64Ty);
  else if (func_name == "__tsan_memcpy")
    func = M.getOrInsertFunction("memcpy", ptrTy, ptrTy, ptrTy, int64Ty);
  else
    return false;

  IRBuilder<> builder(call);
  SmallVector<Value *, 4> args(call->args());
  auto *newCall = builder.CreateCall(func, args);
  call->replaceAllUsesWith(newCall);
  remove_inst_from_func(call);
  return true;
}

static void collect_and_cleanup(Module &M, unsigned *removed_tsan_calls) {
  // assert(not parallel_functions.empty());
  for (Function &Func : M) {
    // ignore tsan itself
    if (Func.getName().starts_with("tsan."))
      continue;
    if (Func.isDeclaration())
      continue;

    // remove TSAN calls only in single-threaded functions
    if (parallel_functions.contains(&Func))
      continue;

    SmallVector<Instruction *> to_be_erased;
    for (BasicBlock &BB : Func)
      for (Instruction &Inst : BB)
        if (auto *call = dyn_cast<CallBase>(&Inst))
          if (getCallName(call)->starts_with("__tsan"))
            if (not getCallName(call)->starts_with("__tsan_func_"))
              to_be_erased.push_back(call);

    for (auto *Inst : to_be_erased) {
      if (cleanup_call_with_chain(Inst)) {
        (*removed_tsan_calls)++;
        continue;
      }
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
    DenseSet<Function *> call_graph;
    collectAllParallelFunctions(call_graph, &func);
  }

  unsigned removed_tsan_calls = 0;
  collect_and_cleanup(M, &removed_tsan_calls);

  return "single-thread removal: " + std::to_string(removed_tsan_calls);
}
