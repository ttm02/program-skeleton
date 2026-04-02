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

using namespace llvm;

static DenseSet<CallBase *> ParallelCalls;
static GlobalVariable *ThreadCounter;

static bool check_module(Module &M) {
  // might be a library -> calls from other functions outside the scope possible
  return M.getFunction("main");
}

static void collectAllParallelFunctions(Function *func) {
  for (auto &bb : *func) {
    for (auto &inst : bb) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {
        auto *called_func = call->getCalledFunction();
        if (not called_func)
          continue;
        if (is_thread_function(called_func))
          if (not is_omp_function(called_func))
            ParallelCalls.insert(call);
      }
    }
  }
}

static void wrap_tsan_calls(DenseSet<CallBase *> &tsan_calls) {

  for (auto *call : tsan_calls) {
    const auto func_name = getCallName(call).value();
    assert(func_name.starts_with("__tsan"));

    auto origInserter = [&](IRBuilder<> &origBuilder) {
      const auto int64Ty = origBuilder.getInt64Ty();
      const auto zero = origBuilder.getInt64(0);
      const auto tc = origBuilder.CreateLoad(int64Ty, ThreadCounter, true);
      auto *isNE = origBuilder.CreateICmpNE(tc, zero);
      return isNE;
    };
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      call->moveAfter(tsanBuilder.GetInsertPoint());
    };
    splitBBexecOnce(call, origInserter, tsanInserter);
  }
}

static inline void wrap_fork(CallBase *call) {
  IRBuilder<> builder(call->getParent());
  const auto constOne = builder.getInt64(1);
  auto *counterInc =
      builder.CreateAtomicRMW(AtomicRMWInst::Add, ThreadCounter, constOne,
                              MaybeAlign(), AtomicOrdering::AcquireRelease);
  counterInc->moveBefore(call->getIterator());
}

static inline void wrap_join(CallBase *call) {
  IRBuilder<> builder(call->getParent());
  const auto constOne = builder.getInt64(1);
  auto *counterDec =
      builder.CreateAtomicRMW(AtomicRMWInst::Sub, ThreadCounter, constOne,
                              MaybeAlign(), AtomicOrdering::AcquireRelease);
  counterDec->moveAfter(call->getIterator());
}

static void wrap_parallel_calls() {
  for (auto call : ParallelCalls) {
    auto call_name = getCallName(call);
    assert(call_name.has_value());
    auto func_name = call_name.value();
    // TODO there might be other calls
    if (func_name == "pthread_create") {
      wrap_fork(call);
    } else if (func_name == "pthread_join") {
      wrap_join(call);
    }
  }
}

std::string wrap_non_openmp_tsan_calls(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Restrict impact of single-threaded TSAN calls in POSIX thread "
            "programs\n";
  if (not check_module(M))
    return "";

  for (auto &func : M) {
    // ignore tsan itself
    if (func.getName().starts_with("tsan"))
      continue;
    collectAllParallelFunctions(&func);
  }

  unsigned wrapped_tsan_calls = 0;

  if (not ParallelCalls.empty()) {
    auto *ctx = &M.getContext();
    auto *int64Ty = Type::getInt64Ty(*ctx);
    ThreadCounter = new llvm::GlobalVariable(
        M, int64Ty, /*isConstant=*/false, GlobalValue::PrivateLinkage,
        ConstantInt::get(int64Ty, 0),
        "PRECOMPUTE_STATIC_ANALYSIS_INTERNAL_THREAD_COUNTER");

    for (auto &func : M) {
      DenseSet<CallBase *> tsan_calls;
      for (auto &bb : func)
        for (auto &inst : bb)
          if (auto *call = dyn_cast<CallBase>(&inst))
            if (isAcceptableTsanCall(call))
              tsan_calls.insert(call);

      if (not tsan_calls.empty()) {
        wrap_tsan_calls(tsan_calls);
        wrapped_tsan_calls += tsan_calls.size();
      }
    }

    if (wrapped_tsan_calls != 0)
      wrap_parallel_calls();
  }

  return "wrapped possible single-threaded TSAN calls: " +
         std::to_string(wrapped_tsan_calls);
}
