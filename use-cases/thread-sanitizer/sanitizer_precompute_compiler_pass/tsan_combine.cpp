//
// Created by Jan Braun on 29.10.25.
//

#include "tsan_precompute_cleanup.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Compiler.h"

#include "precompute/compiler/analysis_results.h"

using namespace llvm;

static inline bool map_base_ptr_to_tsan_call(
    DenseMap<Instruction *, DenseSet<CallBase *>> &base_ptr_to_call,
    CallBase *tsan_call, Instruction *inst) {
  // value is uncertain -> avoid these DFG values
  if (isa<PHINode>(inst) || isa<CallBase>(inst) || isa<LoadInst>(inst) ||
      isa<SelectInst>(inst))
    return false;
  // Do not readd to list
  if (base_ptr_to_call.contains(inst))
    return true;

  if (not(isa<GetElementPtrInst>(inst) || isa<AllocaInst>(inst) ||
          isa<IntToPtrInst>(inst) || isa<CastInst>(inst) ||
          isa<BinaryOperator>(inst))) {
    inst->dump();
    llvm_unreachable("unexpected instruction in TSAN call DFG");
  }

  for (auto &u : inst->operands())
    if (auto useGep = dyn_cast<Instruction>(u.get()))
      if (not map_base_ptr_to_tsan_call(base_ptr_to_call, tsan_call, useGep))
        return false;

  base_ptr_to_call[inst].insert(tsan_call);
  return true;
}

static inline void removeInst(Instruction *Inst) {
  if (!Inst->use_empty())
    Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
  Inst->eraseFromParent();
}

static unsigned remove_tsan_calls_in_func(DenseSet<CallBase *> &tsan_calls) {
  if (tsan_calls.empty())
    return 0;

  unsigned removed_tsan_calls = 0;
  DenseMap<Instruction *, DenseSet<CallBase *>> base_ptr_to_call;

  for (auto ts : tsan_calls) {
    auto arg0 = ts->getArgOperand(0);
    if (Instruction *inst0 = dyn_cast<Instruction>(arg0))
      map_base_ptr_to_tsan_call(base_ptr_to_call, ts, inst0);
  }

  for (auto bp : base_ptr_to_call) {
    auto call_list = bp.getSecond();
    if (call_list.size() < 2)
      continue;

    DenseSet<Value *> ptr_values;
    DenseSet<CallBase *> tsan_writes;

    for (auto call : call_list) {
      ptr_values.insert(call->getArgOperand(0));

      auto func_name = call->getCalledFunction()->getName();
      if (func_name.starts_with("tsan_write")) {
        assert(not func_name.ends_with("_range"));
        tsan_writes.insert(call);
      }
    }

    if (ptr_values.size() == 1) {
      CallBase *keep_inst =
          tsan_writes.empty() ? *call_list.begin() : *tsan_writes.begin();
      for (auto call : call_list) {
        if (call != keep_inst) {
          removeInst(call);
          removed_tsan_calls++;
        }
      }
    }
  }

  return removed_tsan_calls;
}

std::string reduce_tsan_calls(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Combine multiple TSAN calls with range call\n";
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

          if (func_name.starts_with("__tsan_write")) {
            tsan_calls.insert(call);
          } else if (func_name.starts_with("__tsan_read")) {
            tsan_calls.insert(call);
          } else if (func_name.starts_with("__tsan_unaligned")) {
            // TODO
            // DRB011 has @__tsan_unaligned_read4
          } else if (func_name.starts_with("__tsan_atomic")) {
            // TODO
            // DRB074 has `@__tsan_atomic32_fetch_add(ptr %3, i32 %18, i32 0)`
          } else if (func_name == "__tsan_memset" ||
                     func_name == "__tsan_memcpy") {
            // TODO
            // DRB058 has `call ptr @__tsan_memset(ptr %20, i32 0, i64 %14)`
            // DRB058 has `call ptr @__tsan_memcpy(ptr %39, ptr %38, i64 %32)`
          } else {
            call->dump();
            llvm_unreachable("TSAN Combiner: only read or write call expected");
          }
        }
      }
    }

    removed_tsan_calls += remove_tsan_calls_in_func(tsan_calls);
  }

  // print statistics
  return "Removed TSAN calls: " + std::to_string(removed_tsan_calls);
}
