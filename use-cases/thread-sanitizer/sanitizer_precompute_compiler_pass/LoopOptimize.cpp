//
// Created by tim on 12.05.25.
//

#include "tsan_precompute_cleanup.h"

#include "precompute/compiler/analysis_results.h"
#include "precompute/compiler/openmp_runtime_functions.h"
#include "precompute/compiler/std_funcs.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include <cassert>

using namespace llvm;

static inline std::pair<BasicBlock *, CallBase *>
getCallInFunc(Function *func, std::string func_target_name,
              bool starts_with = false) {
  for (BasicBlock &bb : *func) {
    for (Instruction &inst : bb) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {
        auto *called_func = call->getCalledFunction();
        if (!called_func)
          continue;
        auto func_name = called_func->getName();
        if (starts_with) {
          if (func_name.starts_with(func_target_name))
            return std::make_pair(&bb, call);
        } else {
          if (func_name == func_target_name)
            return std::make_pair(&bb, call);
        }
      }
    }
  }
  return std::make_pair(nullptr, nullptr);
}

template <typename T>
static bool isRecUser(Instruction *inst, DenseSet<Instruction *> &visited) {
  if (isa<T>(inst))
    return true;
  if (visited.contains(inst))
    return false;
  visited.insert(inst);
  for (auto *u : inst->users())
    if (auto i = dyn_cast<Instruction>(u))
      if (isRecUser<T>(i, visited))
        return true;
  return false;
}

static Value *getBoundStore(Value *omp_bound, LoadInst *first) {
  auto *ptr = first->getPointerOperand();

  Value *second = nullptr;
  for (auto *u : omp_bound->users()) {
    if (auto *store = dyn_cast<StoreInst>(u)) {
      auto *storeVal = store->getValueOperand();
      if (auto *ci = dyn_cast<ConstantInt>(storeVal)) {
        assert(not second);
        second = ConstantInt::get(ci->getType(), ci->getValue());
        // TODO non constant bounds
        /*
        } else if (auto *sv = dyn_cast<Instruction>(storeVal);
                   not isRecOperand(sv, ptr)) {
          assert(not second);
          auto svClone = sv->clone();
          svClone->insertAfter(sv);
          second = svClone;
        */
      } else {
        continue;
      }
    }
  }
  return second;
}

static void getBoundLoadStoreReplacement(
    Value *omp_bound,
    DenseMap<Value *, SmallDenseSet<Use *>> &boundReplacement) {
  DenseSet<LoadInst *> loadSet;
  for (auto *u : omp_bound->users()) {
    DenseSet<Instruction *> visited;
    if (auto *load = dyn_cast<LoadInst>(u))
      if (isRecUser<BranchInst>(load, visited))
        loadSet.insert(load);
  }
  if (loadSet.empty())
    return;

  for (auto *load : loadSet) {
    auto origVal = getBoundStore(omp_bound, load);
    // TODO non constant bounds
    if (not origVal)
      continue;

    SmallDenseSet<Use *> useSet;
    for (Use &u : load->uses()) {
      useSet.insert(&u);
      u.set(origVal);
    }
    boundReplacement[load] = useSet;
  }
}

static void
openMPboundFix(Function *func,
               DenseMap<Value *, SmallDenseSet<Use *>> &boundReplacement) {
  if (not func->getName().contains(".omp_outlined."))
    return;

  auto call_pair = getCallInFunc(func, "__kmpc_for_static_init", true);
  auto *omp_for_static = call_pair.second;
  if (not omp_for_static)
    return;

  auto omp_lower = omp_for_static->getArgOperand(4);
  getBoundLoadStoreReplacement(omp_lower, boundReplacement);
  auto omp_upper = omp_for_static->getArgOperand(5);
  getBoundLoadStoreReplacement(omp_upper, boundReplacement);
}

// if e.g. loop index is used after the loop
// TODO not extensively tested!
static void compute_other_loop_values(Module &M, ScalarEvolution *SE,
                                      const SCEV *exitCount, Loop *loop,
                                      SCEVExpander &seExpander) {
  std::map<Value *, Value *> replacement_map;
  for (auto &bb : loop->getBlocks()) {
    for (auto &inst_in_loop : *bb) {
      if (not SE->isSCEVable(inst_in_loop.getType()))
        continue;

      auto scev = dyn_cast<SCEVAddRecExpr>(SE->getSCEV(&inst_in_loop));
      if (not scev)
        continue;

      for (auto *u : inst_in_loop.users()) {
        if (auto user_inst = dyn_cast<Instruction>(u)) {
          if (not loop->contains(user_inst)) {
            auto *end_value_scev = scev->evaluateAtIteration(exitCount, *SE);
            Value *end_value = seExpander.expandCodeFor(end_value_scev,
                                                        inst_in_loop.getType());
            replacement_map[&inst_in_loop] = end_value;
            // only one replacement value is needed even if multiple users
            break;
          }
        }
      }
    }
  }
  // perform the replacement
  for (auto pair : replacement_map) {
    pair.first->replaceAllUsesWith(pair.second);
  }
}

static bool replace_tsan_ranges(Module &M, ScalarEvolution *SE, Loop *loop,
                                CallBase *call) {
  auto called_func = call->getCalledFunction();
  auto func_name = called_func->getName();

  // TODO other TSAN calls
  // maybe move func_entry (before loop) and func_exit (after loop)
  if (not func_name.starts_with("__tsan_read") &&
      not func_name.starts_with("__tsan_write")) {
    return false;
  }
  assert(not func_name.starts_with("__tsan_read_write"));

  auto *call_arg_0 = call->getArgOperand(0);
  auto *tsan_size = get_size_of_tsan_access(call);
  if (not tsan_size) {
    return false;
  }

  if (loop->isLoopInvariant(call_arg_0)) {
    // TODO restrict to only one iteration
    /*
    call->moveAfter(builder.GetInsertPoint());
    */
    return false;
  }

  auto scev = SE->getSCEV(call_arg_0);
  if (not SE->hasComputableLoopEvolution(scev, loop)) {
    // Ptr in loop has non computable Scalar Evolution
    return false;
    // TODO else: we could compute the memory accesses before the loop
    // without running it and tell tsan that whole region is accessed
    // at once effectively
  }

  auto *addRec = dyn_cast<SCEVAddRecExpr>(scev);
  if (!addRec) {
    // Could not compute start and end values of ptr
    return false;
  }

  // only if contingous address range
  auto *stepConstant = dyn_cast<SCEVConstant>(addRec->getStepRecurrence(*SE));
  if (not stepConstant)
    return false;
  if (stepConstant->getAPInt().abs() != tsan_size->getValue())
    return false;

  auto *tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);
  auto *start = addRec->getStart();
  auto *stop = addRec->evaluateAtIteration(tripCount, *SE);

  // TODO non-constant iteration counts
  if (not isa<SCEVConstant>(tripCount))
    return false;

  SCEVExpander seExpander(*SE, M.getDataLayout(), "scev");
  seExpander.setInsertPoint(call);

  // TODO i64 might not always be applicable
  auto *ctx = &M.getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto int64Ty = Type::getInt64Ty(*ctx);
  auto constOne = ConstantInt::get(int64Ty, 1);

  auto *lower_bound = stepConstant->getAPInt().isNegative() ? stop : start;
  auto *val_min = seExpander.expandCodeFor(lower_bound, int64Ty);
  if (isa<PoisonValue>(val_min))
    return false;

  // fix trailing DbgRecords in BasicBlock
  auto *prevInst = call->getPrevNonDebugInstruction(true);
  assert(prevInst);
  auto *nextNode = call->getNextNode();
  assert(nextNode);
  auto *origBB = call->getParent();
  prevInst->adoptDbgRecords(origBB, nextNode->getIterator(), false);

  auto *restBB = origBB->splitBasicBlock(call->getNextNonDebugInstruction());
  auto *tsanBB = BasicBlock::Create(*ctx, "tsan_bb", call->getFunction());
  origBB->getTerminator()->eraseFromParent();

  IRBuilder<> origBuilder(origBB);
  origBuilder.SetInsertPoint(call);
  auto *base_ptr = origBuilder.CreateIntToPtr(val_min, ptrTy);
  auto *isEQ = origBuilder.CreateICmpEQ(call_arg_0, base_ptr);
  origBuilder.CreateCondBr(isEQ, tsanBB, restBB);

  IRBuilder<> tsanBuilder(tsanBB);
  tsanBuilder.CreateBr(restBB);

  // length = size_of(element) * (abs(last - base) + 1)
  tsanBuilder.SetInsertPoint(tsanBB->begin());
  seExpander.setInsertPoint(tsanBuilder.GetInsertPoint());
  auto *iter_count = seExpander.expandCodeFor(tripCount, int64Ty);
  auto *count_full = tsanBuilder.CreateAdd(iter_count, constOne);
  auto *range_full = tsanBuilder.CreateMul(count_full, tsan_size);

  bool isWrite = func_name.starts_with("__tsan_write");
  createTSANrange(M, tsanBuilder, base_ptr, range_full, isWrite);
  remove_inst_from_func(call);

  return true;
}

static unsigned perform_tsan_licm(Module &M, Loop *loop,
                                  const std::vector<CallBase *> &tsan_in_loop) {
  unsigned removed_tsan_calls = 0;

  BasicBlock *incoming;
  BasicBlock *backedge;
  if (not loop->getIncomingAndBackEdge(incoming, backedge))
    return removed_tsan_calls;

  IRBuilder<> insert_builder(incoming);
  BasicBlock::iterator insert_dummy;
  auto *incomingTerm = incoming->getTerminator();
  if (incomingTerm)
    insert_dummy = incomingTerm->getIterator();
  else
    insert_dummy = incoming->begin();

  auto *func = loop->getHeader()->getParent();
  DenseMap<Value *, SmallDenseSet<Use *>> boundReplacement;
  openMPboundFix(func, boundReplacement);

  auto *SE = analysis_results->getSE(*func);
  SCEVExpander seExpander(*SE, M.getDataLayout(), "scev");
  seExpander.setInsertPoint(insert_builder.GetInsertPoint());

  // TODO writes only when follow-up loop was also optimized
  for (auto *call : tsan_in_loop)
    if (replace_tsan_ranges(M, SE, loop, call))
      removed_tsan_calls++;

  // Rollback: Do not break OpenMP thread handling
  for (auto br : boundReplacement) {
    auto *load = br.getFirst();
    for (Use *u : br.getSecond())
      u->set(load);
  }

  return removed_tsan_calls;
}

std::string Optimize_loops(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Optimize Loops\n";
  unsigned removed_tsan_calls = 0;

  for (auto &f : M) {
    if (not f.isDeclaration() && not is_func_from_std(&f)) {
      auto li = analysis_results->getLoopInfo(f);
      for (auto loop : li->getLoopsInPreorder()) {
        std::vector<llvm::CallBase *> tsan_calls;
        for (auto &bb : loop->getBlocks()) {
          for (auto &inst : *bb) {
            if (auto *call = dyn_cast<CallBase>(&inst)) {
              auto called_func = call->getCalledFunction();
              if (not called_func)
                continue;
              auto func_name = called_func->getName();
              if (not called_func->getName().starts_with("__tsan"))
                continue;

              if (func_name != "__tsan_func_entry" &&
                  func_name != "__tsan_func_exit") {
                tsan_calls.push_back(call);
              }
            }
          }
        }
        removed_tsan_calls += perform_tsan_licm(M, loop, tsan_calls);
      }
    }
  }

  // print statistics
  return "invariant/unrolled TSAN calls: " + std::to_string(removed_tsan_calls);
}
