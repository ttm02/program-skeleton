#pragma once

#include <chrono>
#include <stack>
#include <stdexcept>
#include <string>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
// #include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

inline bool at_utils_is_allocation(llvm::Function *F) {
  if (F->getName() == "malloc") {
    return true;
  }
  if (F->getName() == "calloc") {
    return true;
  }
  if (F->getName() == "realloc") {
    return true;
  }
  if (F->getName() == "aligned_alloc") {
    return true;
  }

  return false;
}

void at_utils_collect_problematic_calls(
    llvm::CallBase *call, const std::shared_ptr<TaintedValue> &ptr,
    std::vector<llvm::Instruction *> &problematic_calls) {
  auto start_time_point = std::chrono::high_resolution_clock::now();

  llvm::errs() << "\t[AllocTrackerLTOPass::Precompute] Trying to find "
                  "problematic allocation calls ... \n";

  // llvm::PostDominatorTree PDT(*(call->getFunction()));
  llvm::DominatorTree DT(*(call->getFunction()));

  std::vector<llvm::Value *> already_processed_TV;
  std::stack<llvm::Value *> listt;
  listt.push(ptr->v);

  bool found_problematic_allocation_calls = false;

  while (!listt.empty()) {
    auto current = listt.top();
    listt.pop();
    // llvm::errs() << "\t\tCURRENT : " << *current << "\n";

    const bool already_processed =
        std::find_if(already_processed_TV.begin(), already_processed_TV.end(),
                     [current](llvm::Value *vv) { return vv == current; }) !=
        already_processed_TV.end();

    if (already_processed)
      continue;
    already_processed_TV.push_back(current);

    if (auto *store_I = llvm::dyn_cast<llvm::StoreInst>(current)) {
      llvm::Value *ptr_op = store_I->getPointerOperand();
      listt.push(ptr_op);
      continue;
    }

    if (auto *gep_I = llvm::dyn_cast<llvm::GetElementPtrInst>(current)) {
      llvm::Value *ptr_op = gep_I->getPointerOperand();
      listt.push(ptr_op);
    }

    // Check for dynamic allocation call
    if (auto *call_B = llvm::dyn_cast<llvm::CallBase>(current)) {
      llvm::Function *called_F_ = call_B->getCalledFunction();

      if (called_F_ && at_utils_is_allocation(called_F_)) {
        if (auto *current_V_I = llvm::dyn_cast<llvm::Instruction>(current)) {

          // Now simply check whether the problematic allocation call comes
          // logically after the MPI_Recv/MPI_Bcast/...
          //  This makes sure no allocation calls are flagged as problematic
          //  that depend on the ptr but come before the MPI_Recv/MPI_Bcast/...

          // Note: we need to use the dominator tree (DT) here (and not post
          // dominator tree (PDT)) since we are only interested
          //       whether every path that reaches the problematic allocation
          //       call must have gone through MPI_Recv first. Post dominance on
          //       the other hand would check whether the problematic allocation
          //       call would execute after MPI_Recv on ALL paths that continue
          //       from MPI_Recv. This is not the case if there is e.g. an exit
          //       call after the allocation call like this:
          //
          //       void *ptr = (void *) malloc(size);
          //       if (ptr == NULL) {
          //          printf("NULL pointer from malloc call in %s at %d\n",
          //          file, line); exit(-1);
          //       }
          //
          //       In this case PDT would not find the problematic allocation
          //       call.
          if (DT.dominates(call, current_V_I)) {
            found_problematic_allocation_calls = true;
            problematic_calls.push_back(current_V_I);
            llvm::errs()
                << "\t\t\t[AllocTrackerLTOPass::Precompute] Found problematic "
                   "dynamic allocation (tracking communication not supported): "
                << *current_V_I << "\n";

            std::error_code EC_try;
            llvm::raw_fd_ostream out_file_stream_try(
                "try_information.txt", EC_try,
                llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
            if (!EC_try) {
              out_file_stream_try << "call: " << *call
                                  << "\n\t\tproblematic allocation call: ";
              out_file_stream_try << *current << "\n";
            } else {
              llvm::errs() << "Error opening file: " << EC_try.message()
                           << "\n";
            }
          }
        }
      }
    }

    // llvm::errs() << "\n\n\n\tCURRENT->USERS:\n";
    for (auto Udd : current->users()) {
      if (auto Idd = llvm::dyn_cast<llvm::Instruction>(Udd)) {
        // llvm::errs() << "\t\t\t" << *Idd << "\n";
        listt.push(Idd);
      }
    }
    // llvm::errs() << "\n\n\n";
  }

  auto end_time_point = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end_time_point - start_time_point;
  llvm::errs() << "\t[AllocTrackerLTOPass::Precompute] It took "
               << duration.count()
               << " seconds to find the problematic allocation call(s)\n";

  assert(found_problematic_allocation_calls &&
         "The analysis was not able to find the problematic allocation call. "
         "This might be a false positive, but its probably best not to use the "
         "IgnoreMPISendRecvCommunication or IgnoreMPIBcastCommunication option "
         "and just add all MPI Communication for this program.");
}
