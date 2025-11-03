//
// Created by Jan Braun on 03.11.25.
//

#include "tsan_precompute_cleanup.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

static unsigned remove_tsan_calls_in_func(DenseSet<CallBase *> &tsan_calls,
                                          Module &M) {
  if (tsan_calls.empty())
    return 0;
  unsigned removed_tsan_calls = 0;

  for (auto *ts : tsan_calls) {
    if (auto arg0 = dyn_cast<GlobalVariable>(ts->getOperand(0))) {
      if (not arg0->isDeclarationForLinker()) {
        ts->dump();
        arg0->dump();
      }
    }
  }

  return removed_tsan_calls;
}

std::string eliminate_single_thread(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Eliminate non-multi-threaded variable accesses\n";
  unsigned int removed_tsan_calls = 0;

  for (Function &Func : M) {
    DenseSet<CallBase *> tsan_calls;

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

          if (not func_name.starts_with("__tsan"))
            continue;
          if (func_name == "__tsan_func_entry" ||
              func_name == "__tsan_func_exit") {
            continue;
          }

          tsan_calls.insert(call);
        }
      }
    }

    removed_tsan_calls += remove_tsan_calls_in_func(tsan_calls, M);
  }

  // print statistics
  return "Removed TSAN calls: " + std::to_string(removed_tsan_calls);
}
