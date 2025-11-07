//
// Created by Jan Braun on 03.11.25.
//

#include "tsan_precompute_cleanup.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

static unsigned remove_tsan_calls_in_func(
    DenseMap<CallBase *, SmallVector<CallBase *, 2>> &tsan_calls, Module &M) {
  if (tsan_calls.empty())
    return 0;
  unsigned removed_tsan_calls = 0;

  DenseMap<Value *, SmallDenseSet<Value *>> var2regionMap;
  DenseMap<Value *, SmallDenseSet<CallBase *>> var2callMap;

  for (auto ts : tsan_calls) {
    auto *ts_call = ts.getFirst();
    if (auto *arg0 = dyn_cast<GlobalVariable>(ts_call->getOperand(0))) {
      if (not arg0->isDeclarationForLinker()) {
        for (auto *cs : ts.getSecond()) {
          assert(3 <= cs->getNumOperands());
          auto *csName = cs->getOperand(2);
          assert(csName->hasName() &&
                 csName->getName().starts_with(".gomp_critical_user_"));

          var2regionMap[arg0].insert(csName);
          var2callMap[arg0].insert(ts_call);
        }
      }
    }
  }

  for (auto vr : var2regionMap) {
    auto *var = vr.getFirst();
    auto regions = vr.getSecond();

    // if all variable accesses are inside the same critical region name
    if (regions.size() == 1) {
      for (auto call : var2callMap[var]) {
        remove_inst_from_func(call);
        removed_tsan_calls++;
      }
    }
  }

  return removed_tsan_calls;
}

std::string eliminate_single_thread(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Eliminate non-multi-threaded variable accesses\n";
  unsigned int removed_tsan_calls = 0;

  for (Function &Func : M) {
    DenseMap<CallBase *, SmallVector<CallBase *, 2>> tsan_calls;
    SmallVector<CallBase *, 2> criticalRegionCalls;

    // do not instrument tsan itself
    if (Func.getName().starts_with("tsan.module_ctor"))
      continue;

    for (BasicBlock &BB : Func) {
      for (Instruction &inst : BB) {
        if (auto call = dyn_cast<CallBase>(&inst)) {
          auto called_func = call->getCalledFunction();
          if (!called_func)
            continue;
          auto func_name = called_func->getName();

          // TODO check domination tree
          // new nested critical section barrier
          if (func_name == "__kmpc_critical")
            criticalRegionCalls.push_back(call);
          // end of current critical section
          if (func_name == "__kmpc_end_critical")
            criticalRegionCalls.pop_back();

          if (not func_name.starts_with("__tsan"))
            continue;
          if (func_name == "__tsan_func_entry" ||
              func_name == "__tsan_func_exit") {
            continue;
          }

          // NOTE: because we are not able to do correct multi modul analysis,
          // this has to be restricted to OpenMP as this has specific keywords
          // to enforce single threads inside a certain code region
          if (not criticalRegionCalls.empty())
            tsan_calls[call] = criticalRegionCalls;
        }
      }
    }

    removed_tsan_calls += remove_tsan_calls_in_func(tsan_calls, M);
  }

  // print statistics
  return "Removed TSAN calls: " + std::to_string(removed_tsan_calls);
}
