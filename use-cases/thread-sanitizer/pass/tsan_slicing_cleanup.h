//
// Created by Jan Braun on 01.11.25.
//

#ifndef TSAN_PRECOMPUTE_CLEANUP_H
#define TSAN_PRECOMPUTE_CLEANUP_H

#include "precompute/compiler/precalculation.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include <memory>

extern std::shared_ptr<PrecalculationAnalysis> precalculation_analysis;

std::string optimize_loops(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
std::string combine_tsan_calls(llvm::Module &M,
                               llvm::ModuleAnalysisManager &AM);
std::string eliminate_only_in_critical(llvm::Module &M,
                                       llvm::ModuleAnalysisManager &AM);
std::string remove_all_single_thread_regions(llvm::Module &M,
                                             llvm::ModuleAnalysisManager &AM);
std::string wrap_non_openmp_tsan_calls(llvm::Module &M,
                                       llvm::ModuleAnalysisManager &AM);

void run_cleanup(llvm::Module &M, llvm::ModuleAnalysisManager &AM);

llvm::CallInst *createTSANrange(llvm::Module &M, llvm::IRBuilder<> &builder,
                                llvm::Value *base_ptr, llvm::Value *struct_size,
                                const bool isWrite);
llvm::CallInst *createTSANrange(llvm::Module &M, llvm::Instruction *base_ptr,
                                const unsigned struct_size, const bool isWrite);
void remove_inst_from_func(llvm::Instruction *inst);

void get_called_functions(llvm::CallBase *call,
                          llvm::SmallDenseSet<llvm::Function *> &functions);

bool mightInfluenceHappensBefore(
    llvm::Function *func, llvm::DenseSet<llvm::Function *> &alreadyVisisted);
bool mightInfluenceHappensBefore(
    llvm::Instruction *inst, llvm::DenseSet<llvm::Function *> &alreadyVisisted);

void splitBBexecOnce(
    llvm::Instruction *inst,
    std::function<llvm::Value *(llvm::IRBuilder<> &origBuilder)> origInserter,
    std::function<void(llvm::IRBuilder<> &tsanBuilder)> tsanInserter);

inline unsigned bits2bytes(const unsigned bits) { return (bits + 7) / 8; };

inline std::optional<llvm::StringRef> getCallName(llvm::CallBase *call) {
  auto called_func = call->getCalledFunction();
  if (!called_func)
    return {};
  auto func_name = called_func->getName();
  return func_name;
}

inline bool isAcceptableTsanCall(llvm::CallBase *call) {
  auto call_name = getCallName(call);
  if (not call_name.has_value())
    return false;
  auto func_name = call_name.value();

  if (not func_name.starts_with("__tsan"))
    return false;

  if (func_name.starts_with("__tsan_read") ||
      func_name.starts_with("__tsan_write") ||
      func_name.starts_with("__tsan_unaligned_read") ||
      func_name.starts_with("__tsan_unaligned_write")) {
    assert(not func_name.contains("read_write"));
    return true;
  }
  return false;
}

inline llvm::ConstantInt *get_size_of_tsan_access(llvm::CallBase *tsan_call) {
  auto name = getCallName(tsan_call).value();
  auto len = 0;
  if (name.ends_with("_range")) {
    assert(name.starts_with("__tsan_"));
    auto range_size = tsan_call->getArgOperand(1);
    if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(range_size))
      return ci;
    else
      // TODO are non-constant ranges possible?
      return nullptr;
  } else if (name.starts_with("__tsan_read")) {
    len = std::string("__tsan_read").size();
  } else if (name.starts_with("__tsan_write")) {
    len = std::string("__tsan_write").size();
  } else
    return nullptr;

  // extract e.g 16 from either "__tsan_read16" or "__tsan_write16"
  auto num = std::stoi(name.substr(len).str());
  auto type = llvm::Type::getInt64Ty(tsan_call->getContext());
  return llvm::ConstantInt::get(type, num);
}

#endif // TSAN_PRECOMPUTE_CLEANUP_H
