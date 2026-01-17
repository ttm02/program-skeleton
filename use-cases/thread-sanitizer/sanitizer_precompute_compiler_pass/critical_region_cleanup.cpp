//
// Created by Jan Braun on 03.11.25.
//

#include "precompute/compiler/analysis_results.h"
#include "tsan_precompute_cleanup.h"

#include "llvm/ADT/StringSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"

#include <string>

using namespace llvm;

static unsigned
remove_tsan_usages_to_var(DenseMap<CallBase *, StringSet<>> &tsan_crit_map,
                          Value *v) {
  SmallVector<CallBase *> to_be_deleted;
  StringSet<> crit_names;

  for (auto *u : v->users()) {
    // non TSAN call are irrelevant for data race detection
    auto *call = dyn_cast<CallBase>(u);
    if (not call)
      continue;
    auto called_func = call->getCalledFunction();
    if (!called_func)
      continue;
    auto func_name = called_func->getName();
    if (not func_name.starts_with("__tsan"))
      continue;

    // if TSAN, but not in map -> very risky -> abort
    if (not tsan_crit_map.contains(call))
      return 0;

    auto tsan_crit_names = tsan_crit_map.lookup(call);
    if (crit_names.empty()) {
      crit_names = tsan_crit_names;
    } else {
      // intersection of all TSAN calls critical regions
      StringSet<> crit_names_tmp;
      for (auto &cn : crit_names)
        if (tsan_crit_names.contains(cn.getKey()))
          crit_names_tmp.insert(cn.getKey());

      if (crit_names_tmp.empty())
        return 0;
      else
        crit_names = crit_names_tmp;
    }

    to_be_deleted.push_back(call);
  }

  unsigned removed_tsan_calls = 0;
  for (auto *call : to_be_deleted) {
    remove_inst_from_func(call);
    removed_tsan_calls++;
  }
  return removed_tsan_calls;
}

static unsigned
remove_global_in_same_crit(DenseMap<CallBase *, StringSet<>> &tsan_crit_map,
                           Module &M) {
  unsigned removed_tsan_calls = 0;

  for (auto &gv : M.globals()) {
    if (gv.hasPrivateLinkage())
      continue;
    removed_tsan_calls += remove_tsan_usages_to_var(tsan_crit_map, &gv);
  }

  return removed_tsan_calls;
}

static bool instBetweenTwoInst(Function &Func, Instruction *inst,
                               Instruction *start, Instruction *end) {
  auto *DT = analysis_results->getDomTree(Func);
  auto *PDT = analysis_results->getPostDomTree(Func);

  if (DT->dominates(start, inst) && not DT->dominates(inst, start) &&
      not DT->dominates(end, inst) && DT->dominates(inst, end))
    return true;
  if (not PDT->dominates(start, inst) && PDT->dominates(inst, start) &&
      not PDT->dominates(end, inst) && PDT->dominates(inst, end))
    return true;

  return false;
}

void group_tsan2crit_in_func(DenseMap<CallBase *, StringSet<>> &tsan_crit_map,
                             Module &M, Function &Func) {
  // do not instrument tsan itself
  if (Func.getName().starts_with("tsan.module_ctor"))
    return;

  // skip declarations
  if (Func.isDeclaration())
    return;

  SmallVector<CallBase *, 16> callsTSAN;
  StringMap<SmallVector<CallBase *, 2>> criticalBegin, criticalEnd;

  auto getCriticalName = [](CallBase *call) {
    auto call_name = getCallName(call);
    assert(call_name.has_value());
    auto func_name = call_name.value();
    assert(func_name.starts_with("__kmpc_") &&
           func_name.ends_with("_critical"));
    assert(3 <= call->getNumOperands());
    auto *csName = call->getOperand(2);
    assert(csName->hasName() &&
           csName->getName().starts_with(".gomp_critical_user_"));
    return csName->getName();
  };

  auto addToCritList = [&](StringMap<SmallVector<CallBase *, 2>> &map,
                           CallBase *call) {
    auto csNameStr = getCriticalName(call);
    map[csNameStr].push_back(call);
  };

  for (BasicBlock &BB : Func) {
    for (Instruction &inst : BB) {
      if (auto call = dyn_cast<CallBase>(&inst)) {
        auto call_name = getCallName(call);
        if (not call_name.has_value())
          continue;
        auto func_name = call_name.value();

        if (func_name == "__kmpc_critical") {
          addToCritList(criticalBegin, call);
        } else if (func_name == "__kmpc_end_critical") {
          addToCritList(criticalEnd, call);
        } else if (func_name.starts_with("__tsan")) {
          if (func_name == "__tsan_func_entry" ||
              func_name == "__tsan_func_exit")
            continue;

          callsTSAN.push_back(call);
        }
      }
    }
  }

  assert(criticalBegin.size() == criticalEnd.size());

  // nothing to do
  if (criticalBegin.empty())
    return;

  StringMap<SmallVector<std::pair<CallBase *, CallBase *>, 2>> criticalRegions;

  auto addToRegionList = [&](auto critName, auto critBeginList,
                             auto critEndList) {
    for (auto *critBegin : critBeginList) {
      for (auto *critEnd : critEndList) {
        if (critBegin->getArgOperand(0) == critEnd->getArgOperand(0)) {
          criticalRegions[critName].push_back(
              std::make_pair(critBegin, critEnd));
          break;
        }
      }
    }
  };

  // find corresponding end call to begin of critical region
  for (auto &crit : criticalBegin) {
    auto critName = crit.getKey();
    auto critBeginList = crit.getValue();
    auto critEndList = criticalEnd.lookup(critName);
    assert(critBeginList.size() == critEndList.size());
    addToRegionList(critName, critBeginList, critEndList);
    assert(critBeginList.size() == criticalRegions[critName].size());
  }
  assert(criticalBegin.size() == criticalRegions.size());

  // map TSAN calls to surrounding regions
  for (auto *tsan : callsTSAN) {
    StringSet<> regionNames;
    for (auto &crit : criticalRegions) {
      auto critName = crit.getKey().str();
      for (auto critSE : crit.getValue()) {
        auto critStart = critSE.first;
        auto critEnd = critSE.second;
        if (instBetweenTwoInst(Func, tsan, critStart, critEnd)) {
          regionNames.insert(critName);
          break;
        }
      }
    }
    if (regionNames.empty())
      continue;
    tsan_crit_map[tsan] = regionNames;
  }
}

std::string eliminate_only_in_critical(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Eliminate non-multi-threaded variable accesses\n";
  unsigned removed_tsan_calls = 0;

  // collect and map TSAN calls to critical region
  DenseMap<CallBase *, StringSet<>> tsan_crit_map;
  for (Function &Func : M)
    group_tsan2crit_in_func(tsan_crit_map, M, Func);

  if (not tsan_crit_map.empty()) {
    removed_tsan_calls += remove_global_in_same_crit(tsan_crit_map, M);
  }

  // print statistics
  return "Removed TSAN calls: " + std::to_string(removed_tsan_calls);
}
