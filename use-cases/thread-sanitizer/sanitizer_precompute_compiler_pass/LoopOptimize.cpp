//
// Created by tim on 12.05.25.
//

#include "tsan_precompute_cleanup.h"

#include "precompute/compiler/analysis_results.h"
#include "precompute/compiler/openmp_runtime_functions.h"
#include "precompute/compiler/std_funcs.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/BasicBlock.h"
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

using namespace llvm;

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
      if (not scev) {
        // Could not compute Scalar Evolution of value used after loop
        // TODO do not remove loop completely
        continue;
      }

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

static bool replace_tsan_ranges(Module &M, IRBuilder<> &builder,
                                ScalarEvolution *SE, SCEVExpander &seExpander,
                                Loop *loop, CallBase *call) {
  auto called_func = call->getCalledFunction();
  auto func_name = called_func->getName();

  // TODO other TSAN calls
  // maybe move func_entry (before loop) and func_exit (after loop)
  if (not func_name.starts_with("__tsan_read") &&
      not func_name.starts_with("__tsan_write")) {
    return false;
  }
  assert(not func_name.starts_with("__tsan_read_write"));

  auto call_arg_0 = call->getArgOperand(0);
  auto tsan_size = get_size_of_tsan_access(call);
  if (not tsan_size) {
    return false;
  }

  if (loop->isLoopInvariant(call_arg_0)) {
    // TODO move call instead of recreate
    // call->moveAfter(builder.GetInsertPoint());
    builder.CreateCall(call->getCalledFunction(), call_arg_0);
    return true;
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
  auto *step = addRec->getStepRecurrence(*SE);
  if (auto stepConstant = dyn_cast<SCEVConstant>(step)) {
    if (stepConstant->getAPInt() != tsan_size->getValue()) {
      return false;
    }
  } else
    return false;

  auto *tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);
  auto *start = addRec->getStart();
  auto *stop = addRec->evaluateAtIteration(tripCount, *SE);

  if (SE->isKnownPredicate(ICmpInst::ICMP_ULE, stop, start)) {
    std::swap(start, stop);
  } else if (not SE->isKnownPredicate(ICmpInst::ICMP_ULE, start, stop)) {
    return false;
  }

  // TODO i64 might not always be applicable
  auto int64Ty = builder.getInt64Ty();
  auto constOne = ConstantInt::get(int64Ty, 1);

  Value *val_min = seExpander.expandCodeFor(start, int64Ty);
  Value *val_max = seExpander.expandCodeFor(stop, int64Ty);

  // size_of(element) * ((last - base) + 1)
  auto *base_ptr = builder.CreateIntToPtr(val_min, builder.getPtrTy());
  auto *iter_count = builder.CreateSub(val_max, val_min);
  auto *count_full = builder.CreateAdd(iter_count, constOne);
  auto *range_full = builder.CreateMul(count_full, tsan_size);

  auto isWrite = func_name.starts_with("__tsan_write");
  createTSANrange(M, builder, base_ptr, range_full, isWrite);
  // TODO remove old call
  // remove_inst_from_func(call);

  return true;
}

// true if loop was optimized
static std::pair<bool, unsigned>
perform_tsan_licm(Module &M, Loop *loop,
                  const std::vector<CallBase *> &tsan_in_loop) {
  unsigned removed_tsan_calls = 0;
  bool full_replacement_possible = true;

  BasicBlock *incoming;
  BasicBlock *backedge;
  if (not loop->getIncomingAndBackEdge(incoming, backedge))
    return std::make_pair(false, 0);

  assert(incoming);
  BasicBlock *outgoing = loop->getExitBlock();
  if (not outgoing)
    full_replacement_possible = false;

  IRBuilder<> insert_builder(incoming);
  auto beforeTermInst =
      incoming->getTerminator()->getPrevNonDebugInstruction(true);
  BasicBlock::iterator insert_dummy;
  // use the BasicBlock begin iterator, if there is only the terminator
  if (beforeTermInst)
    insert_dummy = beforeTermInst->getIterator();
  else
    insert_dummy = incoming->begin();
  insert_builder.SetInsertPoint(insert_dummy);

  auto *func = loop->getHeader()->getParent();
  auto *SE = analysis_results->getSE(*func);
  SCEVExpander seExpander(*SE, M.getDataLayout(), "scev");
  seExpander.setInsertPoint(insert_dummy);

  // loop bounds have to be known
  auto trip_count = SE->getSymbolicMaxBackedgeTakenCount(loop);
  if (isa<SCEVCouldNotCompute>(trip_count))
    full_replacement_possible = false;

  const SCEV *exitCount = SE->getExitCount(loop, loop->getExitingBlock());
  if (isa<SCEVCouldNotCompute>(exitCount)) {
    // unknown loop iteration count -> might be variable
    // TODO do not remove loop completely
    // TSAN range with non-constant runtime param possible
    full_replacement_possible = false;
  }

  // check if other values, such as the loop index are used after the loop
  // and compute them if possible
  if (full_replacement_possible)
    compute_other_loop_values(M, SE, exitCount, loop, seExpander);

  for (auto *call : tsan_in_loop) {
    if (replace_tsan_ranges(M, insert_builder, SE, seExpander, loop, call))
      removed_tsan_calls++;
    else
      full_replacement_possible = false;
  }

  if (full_replacement_possible) {
    BasicBlock *new_bb = BasicBlock::Create(loop->getHeader()->getContext(),
                                            "loop_replacement", func, outgoing);
    IRBuilder<> full_replacement_builder(new_bb);
    full_replacement_builder.CreateBr(outgoing);

    // set incoming BB
    auto *incoming_br = dyn_cast<BranchInst>(incoming->getTerminator());
    assert(incoming_br);
    int num_successors_replaced = 0;
    // find successor to replace and check if it is unique
    for (unsigned int i = 0; i < incoming_br->getNumSuccessors(); i++) {
      auto *succ = incoming_br->getSuccessor(i);
      if (loop->contains(succ)) {
        incoming_br->setSuccessor(i, new_bb);
        num_successors_replaced++;
      }
    }
    assert(num_successors_replaced == 1);

    // remove old loop
    std::vector<BasicBlock *> to_delete;
    for (auto *bb : loop->getBlocks()) {
      bb->replaceAllUsesWith(new_bb);
      to_delete.push_back(bb);
    }
    for (auto *bb : to_delete) {
      // dont care about correct deletion order, we already checked that
      // nothing more is used outside of loop
      for (auto &inst : *bb)
        inst.replaceAllUsesWith(PoisonValue::get(inst.getType()));
      bb->eraseFromParent();
    }
    return std::make_pair(true, removed_tsan_calls);
  }

  return std::make_pair(false, removed_tsan_calls);
}

std::string Optimize_loops(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Optimize Loops\n";
  unsigned optimized_loops = 0;
  unsigned removed_tsan_calls = 0;

  for (auto &f : M) {
    if (not f.isDeclaration() && not is_func_from_std(&f)) {
      bool optimized = true;
      while (optimized) { // until no more optimization
        optimized = false;

        // get new loop info if it was invalidated
        auto li = analysis_results->getLoopInfo(f);

        for (auto loop : li->getLoopsInPreorder()) {
          bool loop_applicable = true;

          std::vector<llvm::CallBase *> tsan_calls;
          for (auto &bb : loop->getBlocks()) {
            for (auto &inst : *bb) {
              if (auto *call = dyn_cast<CallBase>(&inst)) {
                auto called_func = call->getCalledFunction();
                if (called_func) {
                  if (called_func->getName().starts_with("__tsan")) {
                    auto func_name = call->getCalledFunction()->getName();
                    if (func_name != "__tsan_func_entry" &&
                        func_name != "__tsan_func_exit") {
                      tsan_calls.push_back(call);
                    }
                  } else if (called_func->getName() == "llvm.returnaddress") {
                    continue;
                  } else if (is_thread_function(called_func)) {
                    // call to OpenMP?
                    // TODO analyze if we may be able to do something here?
                    loop_applicable = false;
                    break;
                  } else {
                    // TODO do not remove the loop completely
                    // call to something else: we cant analyze that
                    loop_applicable = false;
                    break;
                  }
                }
              }
              if (isa<StoreInst>(&inst)) {
                // TODO do not remove the loop completely
                // some computation result may be necessary
                loop_applicable = false;
                break;
              }
            }
          }

          if (not loop_applicable)
            continue;

          auto ptl = perform_tsan_licm(M, loop, tsan_calls);
          removed_tsan_calls += ptl.second;
          if (ptl.first) {
            optimized_loops++;
            optimized = true;
            analysis_results->invalidate(f);
            break; // end looping over loops, as iterator is invalid
          }
        }
      }
    }
  }

  // print statistics
  return "Loops: " + std::to_string(optimized_loops) +
         "\nTSAN calls: " + std::to_string(removed_tsan_calls);
}
