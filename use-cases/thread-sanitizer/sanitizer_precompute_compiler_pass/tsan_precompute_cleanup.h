//
// Created by Jan Braun on 01.11.25.
//

#ifndef TSAN_PRECOMPUTE_CLEANUP_H
#define TSAN_PRECOMPUTE_CLEANUP_H

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

std::string Optimize_loops(llvm::Module &M, llvm::ModuleAnalysisManager &AM);

inline unsigned bits2bytes(const unsigned bits) { return (bits + 7) / 8; };

inline llvm::ConstantInt *get_size_of_tsan_access(llvm::CallBase *tsan_call) {
  auto name = tsan_call->getCalledFunction()->getName();
  auto len = 0;
  if (name.starts_with("__tsan_read")) {
    if (name == "__tsan_read_range")
      return nullptr;
    len = std::string("__tsan_read").size();
  } else if (name.starts_with("__tsan_write")) {
    if (name == "__tsan_write_range")
      return nullptr;
    len = std::string("__tsan_write").size();
  } else
    return nullptr;

  // extract e.g 16 from either "__tsan_read16" or "__tsan_write16"
  auto num = std::stoi(name.substr(len).str());
  auto type = llvm::Type::getInt64Ty(tsan_call->getContext());
  return llvm::ConstantInt::get(type, num);
}

#endif // TSAN_PRECOMPUTE_CLEANUP_H
