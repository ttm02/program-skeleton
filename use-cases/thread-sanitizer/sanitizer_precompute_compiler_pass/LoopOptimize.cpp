//
// Created by tim on 12.05.25.
//

#include "tsan_precompute_cleanup.h"

#include "precompute/compiler/analysis_results.h"
#include "precompute/compiler/openmp_runtime_functions.h"
#include "precompute/compiler/std_funcs.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

using namespace llvm;

// if e.g. loop index is used after the loop
// TODO not extensively tested!
static bool compute_other_loop_values(llvm::Module &M, ScalarEvolution *SE,
                                      Loop *loop, Instruction *insert_point) {
  const SCEV *exitCount = SE->getExitCount(loop, loop->getExitingBlock());
  if (isa<SCEVCouldNotCompute>(exitCount)) {
#ifdef VERBOSE_DEBUG_PRINTING
    errs() << "Loop Optimization fail: Could not compute loop exit count\n";
#endif
    return false;
  }

  std::map<Value *, Value *> replacement_map;
  for (auto &bb : loop->getBlocks()) {
    for (auto &inst_in_loop : *bb) {
      if (not SE->isSCEVable(inst_in_loop.getType()))
        continue;

      for (auto *u : inst_in_loop.users()) {
        if (auto user_inst = dyn_cast<Instruction>(u)) {
          if (not loop->contains(user_inst)) {
            auto scev = dyn_cast<SCEVAddRecExpr>(SE->getSCEV(&inst_in_loop));
            if (not scev) {
#ifdef VERBOSE_DEBUG_PRINTING
              errs() << "Could not compute Scalar Evolution of value used "
                        "after loop\n";
              inst_in_loop.dump();
#endif
              break;
            }
            auto *end_value_scev = scev->evaluateAtIteration(exitCount, *SE);
            SCEVExpander expander(*SE, M.getDataLayout(), "scev");
            expander.setInsertPoint(insert_point);

            Value *end_value =
                expander.expandCodeFor(end_value_scev, inst_in_loop.getType());
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
  return true;
}

// removes the BB
static void clean_temp_bb(BasicBlock *bb) {
  assert(bb->getNumUses() == 0);
  bb->eraseFromParent();
}

// true if loop was optimized
static bool
perform_tsan_licm(llvm::Module &M, Loop *loop,
                  const std::vector<llvm::CallBase *> &tsan_in_loop) {
  auto *ctx = &M.getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto voidTy = Type::getVoidTy(*ctx);
  auto int64Ty = Type::getInt64Ty(*ctx);

  auto tsan_read_range_func =
      M.getOrInsertFunction("__tsan_read_range", voidTy, ptrTy, int64Ty);
  auto tsan_write_range_func =
      M.getOrInsertFunction("__tsan_write_range", voidTy, ptrTy, int64Ty);

  auto SE = analysis_results->getSE(*loop->getHeader()->getParent());

  // loop bounds have to be known
  auto trip_count = SE->getSymbolicMaxBackedgeTakenCount(loop);
  if (isa<SCEVCouldNotCompute>(trip_count))
    return false;

  BasicBlock *incoming;
  BasicBlock *outgoing;
  if (not loop->getIncomingAndBackEdge(incoming, outgoing)) {
#ifdef VERBOSE_DEBUG_PRINTING
    errs() << "Loop Optimization Incoming and Back edge are not unique\n";
#endif
    return false;
  }
  outgoing = loop->getExitBlock();
  assert(incoming);
  if (!outgoing) {
    // TODO implement
#ifdef VERBOSE_DEBUG_PRINTING
    errs() << "Loop Optimization fail: Outgoing edge not unique\n";
#endif
    return false;
  }
  // errs() << "create new BB instead of loop\n";
  BasicBlock *new_bb =
      BasicBlock::Create(loop->getHeader()->getContext(), "loop_replacement",
                         incoming->getParent(), outgoing);
  IRBuilder<> builder(new_bb);
  // dummy instruction serving as the insertion point to insert everything
  // before
  auto *dummy_inst =
      builder.CreateAlloca(builder.getInt64Ty(), nullptr, "dummy");

  // check if other values, such as the loop index are used after the loop
  // and compute them if possible
  if (not compute_other_loop_values(M, SE, loop, dummy_inst)) {
    clean_temp_bb(new_bb);
    return false;
  }

  for (auto *call : tsan_in_loop) {
    if (call->getNumOperands() != 2)
      continue;
    auto call_arg_0 = call->getArgOperand(0);

    if (not get_size_of_tsan_access(call)) {
      // TODO this tsan call is not supported yet
#ifdef VERBOSE_DEBUG_PRINTING
      errs() << "Loop Optimization fail: TSAN call not supported yet\n";
      call->dump();
#endif
      continue;
    }

    auto called_func = call->getCalledFunction();
    if (loop->isLoopInvariant(call_arg_0)) {
      builder.SetInsertPoint(dummy_inst);
      builder.CreateCall(call->getCalledFunction(), call_arg_0);
    } else {
      auto scev = SE->getSCEV(call_arg_0);
      if (not SE->hasComputableLoopEvolution(scev, loop)) {
#ifdef VERBOSE_DEBUG_PRINTING
        errs() << "Loop Optimization fail: Ptr in loop has non computable "
                  "Scalar Evolution\n";
#endif
        continue;
        // TODO else: we could compute the memory accesses before the loop
        // without running it and tell tsan that whole region is accessed
        // at once effectively
      }

      auto *addRec = dyn_cast<SCEVAddRecExpr>(scev);
      if (!addRec) {
        // could not determine start and end value
        clean_temp_bb(new_bb);
#ifdef VERBOSE_DEBUG_PRINTING
        errs() << "Loop Optimization fail: Could not compute start and end "
                  "values of ptr\n";
#endif
        return false;
      }

      auto tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);

      auto *start = addRec->getStart();
      auto *stop = addRec->evaluateAtIteration(tripCount, *SE);

      if (!SE->isKnownPredicate(ICmpInst::ICMP_ULE, start, stop)) {
        std::swap(start, stop); // "backward" loop
        if (!SE->isKnownPredicate(ICmpInst::ICMP_ULE, start, stop)) {
          // could not determine iteration order
          clean_temp_bb(new_bb);
#ifdef VERBOSE_DEBUG_PRINTING
          errs() << "Loop Optimization fail: Could not determine iteration "
                    "order\n";
#endif
          return false;
        }
      }

      // Expand to runtime values
      // Expand SCEV at min/max trip count
      SCEVExpander expander(*SE, M.getDataLayout(), "scev");
      expander.setInsertPoint(dummy_inst);

      Value *val_min = expander.expandCodeFor(start, builder.getInt64Ty());
      Value *val_max = expander.expandCodeFor(stop, builder.getInt64Ty());

      // create tsan call
      builder.SetInsertPoint(dummy_inst);
      auto *as_ptr = builder.CreateIntToPtr(val_min, builder.getPtrTy());
      auto *size = builder.CreateSub(val_max, val_min);
      // need to include the size of last access
      auto *size_full = builder.CreateAdd(size, get_size_of_tsan_access(call));

      auto func_name = called_func->getName();
      if (func_name.starts_with("__tsan_read")) {
        // not supported right now
        assert(not func_name.starts_with("__tsan_read_write"));
        builder.CreateCall(tsan_read_range_func, {as_ptr, size_full});
      } else {
        assert(func_name.starts_with("__tsan_write"));
        builder.CreateCall(tsan_write_range_func, {as_ptr, size_full});
      }
    }
  }

  // finish up replacement BB
  builder.SetInsertPoint(dummy_inst);
  builder.CreateBr(outgoing);
  dummy_inst->eraseFromParent();
  /*
    errs() << "Loop replaced:\n";
    for (auto bb : loop->getBlocks()) {
      bb->dump();
    }
    errs() << "replaced with:\n";
    new_bb->dump();
  */
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
    for (auto it_i = bb->begin(); it_i != bb->end(); ++it_i) {
      Instruction *inst = &*it_i;
      inst->replaceAllUsesWith(PoisonValue::get(inst->getType()));
    }
    bb->eraseFromParent();
  }

  return true;
}

std::string Optimize_loops(llvm::Module &M, ModuleAnalysisManager &AM) {
  errs() << "Optimize Loops\n";
  unsigned int optimized_loops = 0;

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
                    // TODO analyze if we may be able to do something here?
#ifdef VERBOSE_DEBUG_PRINTING
                    errs() << "Loop Optimization fail: Call to Openmp\n";
                    call->dump();
#endif
                    loop_applicable = false;
                    break;
                  } else {
                    // call to something else: we cant analyze that
#ifdef VERBOSE_DEBUG_PRINTING
                    errs() << "Loop Optimization fail: Call in loop\n";
                    call->dump();
#endif
                    loop_applicable = false;
                    break;
                  }
                } else {
                  llvm_unreachable("Loop Optimization fail: This call does not "
                                   "call anything");
                }
              }
              if (auto *store = dyn_cast<StoreInst>(&inst)) {
                // some computation result may be necessary
                loop_applicable = false;
#ifdef VERBOSE_DEBUG_PRINTING
                errs() << "Loop Optimization fail: store in loop\n";
                inst.dump();
#endif
                // debug info
                auto SE =
                    analysis_results->getSE(*loop->getHeader()->getParent());
                auto scev = SE->getSCEV(store->getPointerOperand()); // ptr
#ifdef VERBOSE_DEBUG_PRINTING
                errs() << "Ptr: Invariant? " << SE->isLoopInvariant(scev, loop)
                       << " Konwn Evolution? "
                       << SE->hasComputableLoopEvolution(scev, loop) << "\n";
#endif
                break;
              }
            }
          }

          if (loop_applicable) {
            if (perform_tsan_licm(M, loop, tsan_calls)) {
              optimized_loops++;
              optimized = true;
              // LoopInfo is invalid!
              analysis_results->invalidate(f);
              break; // end looping over loops, as iterator is invalid
            }
          }
        }
      }
    }
  }

  // print statistics
  return "Optimized loops: " + std::to_string(optimized_loops);
}
