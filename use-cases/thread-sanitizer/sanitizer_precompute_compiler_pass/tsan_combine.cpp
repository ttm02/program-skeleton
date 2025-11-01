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
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"

#include "precompute/compiler/analysis_results.h"

using namespace llvm;

static inline void collect_base_ptr_to_tsan_call(
    DenseMap<Instruction *, SmallDenseSet<CallBase *>> &base_ptr_to_call,
    DenseMap<CallBase *, SmallDenseSet<Instruction *>> &call_to_base_ptr,
    CallBase *tsan_call, Instruction *inst) {
  base_ptr_to_call[inst].insert(tsan_call);
  call_to_base_ptr[tsan_call].insert(inst);

  // value is uncertain -> do not map these DFG values to TSAN call
  if (isa<PHINode>(inst) || isa<CallBase>(inst) || isa<LoadInst>(inst) ||
      isa<SelectInst>(inst) || isa<AllocaInst>(inst))
    return;

  if (not(isa<GetElementPtrInst>(inst) || isa<IntToPtrInst>(inst) ||
          isa<CastInst>(inst) || isa<BinaryOperator>(inst))) {
    inst->dump();
    llvm_unreachable("unexpected instruction in TSAN call DFG");
  }

  for (auto &u : inst->operands())
    if (auto useGep = dyn_cast<Instruction>(u.get()))
      collect_base_ptr_to_tsan_call(base_ptr_to_call, call_to_base_ptr,
                                    tsan_call, useGep);
}

static inline void remove_inst_from_func(
    CallBase *call, Instruction *base_ptr,
    DenseMap<Instruction *, SmallDenseSet<CallBase *>> &base_ptr_to_call,
    DenseMap<CallBase *, SmallDenseSet<Instruction *>> &call_to_base_ptr) {
  if (!call->use_empty())
    call->replaceAllUsesWith(UndefValue::get(call->getType()));
  call->eraseFromParent();

  // remove from all other lists to avoid segmentation fault
  for (auto bp : call_to_base_ptr[call])
    if (bp != base_ptr)
      base_ptr_to_call[bp].erase(call);
}

static inline StructType *getGEPstructTy(const GetElementPtrInst *gep) {
  auto *elemTy = gep->getSourceElementType();
  // getelementptr inbounds nuw [N x %struct.s], ptr %a, i64 0, i64 %b
  // [N x %struct.s]
  if (auto *ArrayTy = dyn_cast<ArrayType>(elemTy)) {
    auto numIdx = gep->getNumIndices();
    if (numIdx != 2 && numIdx != 3) // i64 0, i64 %b
      return nullptr;
    auto idx0 = gep->indices().begin()->get();
    assert(idx0);
    if (auto *CI = dyn_cast<ConstantInt>(idx0)) {
      if (not CI->isZero()) // i64 0
        return nullptr;
    } else
      return nullptr;
    // [N x %struct.s] -> %struct.s
    elemTy = ArrayTy->getElementType();
  }
  // getelementptr inbounds %struct.s, ptr %a, i64 %b
  // %struct.s
  if (auto *STy = dyn_cast<StructType>(elemTy)) {
    assert(elemTy != gep->getSourceElementType() || gep->getNumIndices() == 1);
    return STy;
  }
  return nullptr;
}

static inline void createTSANrange(Module &M, Instruction *base_ptr,
                                   const unsigned struct_size,
                                   const bool isWrite) {
  auto *ctx = &M.getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto voidTy = Type::getVoidTy(*ctx);
  auto int64Ty = Type::getInt64Ty(*ctx);

  auto func_name = isWrite ? "__tsan_write_range" : "__tsan_read_range";
  auto tsan_func = M.getOrInsertFunction(func_name, voidTy, ptrTy, int64Ty);

  assert(base_ptr->getNextNode());
  IRBuilder<> builder(base_ptr->getNextNode());
  // TODO i64 might not always be applicable
  Value *ssv = ConstantInt::get(Type::getInt64Ty(*ctx), struct_size, false);
  builder.CreateCall(tsan_func, {base_ptr, ssv});
}

static inline const Instruction *
check_path_to_base_ptr(const Instruction *inst, const Instruction *base_ptr) {
  if (not inst)
    return nullptr;
  if (not isa<CastInst>(inst)) {
    if (inst == base_ptr)
      return inst;
    /* TODO consider non BinaryOperator instructions
    for (auto &u : inst->operands())
      if (u.get() == base_ptr)
        return inst;
    */
    if (inst->getOperand(0) == base_ptr)
      return inst;
    return nullptr;
  }
  assert(inst->getNumOperands() == 1);
  auto arg0 = dyn_cast<Instruction>(inst->getOperand(0));
  return check_path_to_base_ptr(arg0, base_ptr);
}

static unsigned remove_tsan_calls_in_func(DenseSet<CallBase *> &tsan_calls,
                                          Module &M) {
  if (tsan_calls.empty())
    return 0;

  unsigned removed_tsan_calls = 0;
  DenseMap<Instruction *, SmallDenseSet<CallBase *>> base_ptr_to_call;
  DenseMap<CallBase *, SmallDenseSet<Instruction *>> call_to_base_ptr;

  for (auto *ts : tsan_calls) {
    auto arg0 = ts->getArgOperand(0);
    if (Instruction *inst0 = dyn_cast<Instruction>(arg0))
      collect_base_ptr_to_tsan_call(base_ptr_to_call, call_to_base_ptr, ts,
                                    inst0);
  }

  for (auto bp2call : base_ptr_to_call) {
    auto call_list = bp2call.getSecond();
    if (call_list.size() < 2)
      continue;

    DenseSet<Value *> ptr_values;
    DenseSet<CallBase *> tsan_writes, tsan_reads;

    for (auto call : call_list) {
      auto arg0 = call->getArgOperand(0);
      ptr_values.insert(arg0);

      auto func_name = call->getCalledFunction()->getName();
      if (func_name.ends_with("_range"))
        continue;

      if (func_name.starts_with("__tsan_write"))
        tsan_writes.insert(call);
      else if (func_name.starts_with("__tsan_read"))
        tsan_reads.insert(call);
    }

    // nothing to do
    if (tsan_writes.empty() && tsan_reads.empty())
      continue;

    auto *bp = bp2call.getFirst();
    auto bit2bytes = [&](unsigned bits) { return (bits + 7) / 8; };

    if (ptr_values.size() == 1) {
      // TODO does TSAN already do that itself?
      // Are we removing too much here? additional DRB tests are not failing.
      // take a look at DRB173 -> maybe only remove if in same BasicBlock

      // replace:
      //   call void @__tsan_readX(ptr nonnull %a)
      //   call void @__tsan_writeX(ptr nonnull %a)
      //   call void @__tsan_readX(ptr nonnull %a)
      //   call void @__tsan_writeX(ptr nonnull %a)
      // with:
      //   call void @__tsan_writeX(ptr nonnull %a)

      // keep only one (write) version of of identical TSAN calls
      CallBase *keep_inst =
          tsan_writes.empty() ? *call_list.begin() : *tsan_writes.begin();
      for (auto call : call_list) {
        if (call != keep_inst) {
          remove_inst_from_func(call, bp, base_ptr_to_call, call_to_base_ptr);
          removed_tsan_calls++;
        }
      }
    } else if (auto *base_ptr = dyn_cast<GetElementPtrInst>(bp)) {
      // replace:
      //   %struct.a = type { i32, i32, i32 }
      //   %base = getelementptr inbounds %struct.a, ptr %a, i64 0, ...
      //   %b1 = getelementptr inbounds nuw i8, ptr %base, i64 4
      //   %b2 = getelementptr inbounds nuw i8, ptr %base, i64 8
      // with:
      //   call void @__tsan_write_range(ptr nonnull %base, i64 12)

      // combine contingous address range to TSAN range call
      SmallVector<const GetElementPtrInst *, 10> offset_ptrs;
      const auto STy = getGEPstructTy(base_ptr);
      if (!STy)
        continue;

      // TODO allow partial ranges of only read or only write
      // Both might create false positives/negatives because only one member of
      // the struct is written to.
      if (not tsan_writes.empty() && not tsan_reads.empty())
        continue;

      for (auto ptr : ptr_values) {
        if (auto offset_gep = dyn_cast<GetElementPtrInst>(ptr)) {
          auto base_gep_op = offset_gep->getPointerOperand();
          if (base_ptr != base_gep_op)
            continue;
          assert(offset_gep->getNumIndices() == 1);
          auto idx0 = offset_gep->indices().begin()->get();
          assert(idx0);
          // TODO Scalar Evolution possible?
          if (not isa<ConstantInt>(idx0))
            continue;
          offset_ptrs.push_back(offset_gep);
        }
      }

      if (offset_ptrs.size() < 2)
        continue;

      // TODO are they always sorted -> unnecessary?
      auto byOffset = [&](const GetElementPtrInst *LHS,
                          const GetElementPtrInst *RHS) {
        assert(LHS && RHS);
        auto LHS_idx0 = cast<ConstantInt>(LHS->indices().begin()->get());
        auto RHS_idx0 = cast<ConstantInt>(RHS->indices().begin()->get());
        assert(not LHS_idx0->isNegative());
        assert(not RHS_idx0->isNegative());
        return LHS_idx0->getZExtValue() < RHS_idx0->getZExtValue();
      };
      sort(offset_ptrs, byOffset);

      unsigned byte_offset = 0;
      assert(not STy->elements().empty());
      auto elemTy = STy->elements().consume_front();
      byte_offset += bit2bytes(elemTy->getIntegerBitWidth());
      assert(not offset_ptrs.empty());
      for (auto *offPtr : offset_ptrs) {
        elemTy = STy->elements().consume_front();
        auto idx0 = cast<ConstantInt>(offPtr->indices().begin()->get());
        // struct offset == ptr byte offset * size_of(byte)
        if (byte_offset != idx0->getZExtValue())
          goto bp2call; // no contingous range
        byte_offset += bit2bytes(elemTy->getIntegerBitWidth());
      }

      // TODO allow partial ranges of only read or only write
      createTSANrange(M, base_ptr, byte_offset, tsan_reads.empty());

      // cleanup replaced TSAN calls
      for (auto call : bp2call.getSecond()) {
        auto arg0 = call->getArgOperand(0);
        if (arg0 == base_ptr) {
          remove_inst_from_func(call, bp, base_ptr_to_call, call_to_base_ptr);
          removed_tsan_calls++;
        } else {
          for (auto *offPtr : offset_ptrs) {
            if (arg0 == offPtr) {
              remove_inst_from_func(call, bp, base_ptr_to_call,
                                    call_to_base_ptr);
              removed_tsan_calls++;
            }
          }
        }
      }
    } else if (auto *c_gep = dyn_cast<GetElementPtrInst>(*ptr_values.begin())) {
      // replace;
      //   %base = ...
      //   %base0 = sext i32 %base to i64
      //   %base_ptr = getelementptr inbounds double, ptr %var, i64 %base0
      //   call void @__tsan_write8(ptr %base_ptr)
      //   %b1 = add i32 %base, 1
      //   %base1 = sext i32 %b1 to i64
      //   %ptr_1 = getelementptr inbounds double, ptr %var, i64 %base1
      //   call void @__tsan_write8(ptr %36)
      //   %b2 = add i32 %base, 2
      // with:
      //   call void @__tsan_write_range(ptr %base_ptr, i64 32)

      // TODO allow partial ranges of only read or only write
      // Both might create false positives/negatives because only one member of
      // the struct is written to.
      if (not tsan_writes.empty() && not tsan_reads.empty())
        continue;

      SmallVector<std::pair<GetElementPtrInst *, const Instruction *>, 10>
          offset_ptrs;
      for (auto ptr : ptr_values) {
        auto *ptr_gep = dyn_cast<GetElementPtrInst>(ptr);
        if (not ptr_gep)
          continue;
        if (ptr_gep->getPointerOperand() != c_gep->getPointerOperand())
          continue;
        assert(1 <= ptr_gep->getNumIndices());
        auto *idx0 = dyn_cast<Instruction>(ptr_gep->indices().begin()->get());
        auto *base = check_path_to_base_ptr(idx0, bp);

        if (base) {
          if (base == bp) {
            offset_ptrs.push_back({ptr_gep, base});
          } else if (auto *base_inst = dyn_cast<BinaryOperator>(base)) {
            if (base_inst->getOpcode() == Instruction::Add)
              if (isa<ConstantInt>(base_inst->getOperand(1)))
                offset_ptrs.push_back({ptr_gep, base_inst});
          }
        }
      }

      if (offset_ptrs.size() < 2)
        continue;

      auto byOffset =
          [&](std::pair<GetElementPtrInst *, const Instruction *> LHS,
              std::pair<GetElementPtrInst *, const Instruction *> RHS) {
            assert(RHS.second);
            if (RHS.second == bp)
              return false;
            assert(LHS.second);
            if (LHS.second == bp)
              return true;
            auto *LHS_o1 = cast<ConstantInt>(LHS.second->getOperand(1));
            auto *RHS_o1 = cast<ConstantInt>(RHS.second->getOperand(1));
            assert(not LHS_o1->isNegative());
            assert(not RHS_o1->isNegative());
            return LHS_o1->getZExtValue() < RHS_o1->getZExtValue();
          };
      sort(offset_ptrs, byOffset);

      unsigned n = 0;
      for (auto offPtr : offset_ptrs) {
        auto offAdd = offPtr.second;
        assert(bp == offAdd || bp == offAdd->getOperand(0));
        if (n == 0 && bp != offAdd) {
          goto bp2call; // no contingous range
        } else if (0 < n) {
          auto *offInt = cast<ConstantInt>(offAdd->getOperand(1));
          if (n != offInt->getZExtValue())
            goto bp2call;
        }
        n++;
      }

      // TODO allow partial ranges of only read or only write
      auto base_ptr = offset_ptrs.begin()->first;
      auto *elemTy = base_ptr->getSourceElementType();
      const llvm::DataLayout &DL = M.getDataLayout();
      unsigned elemBitwidth = DL.getTypeSizeInBits(elemTy);
      unsigned byte_offset = bit2bytes(n * elemBitwidth);
      createTSANrange(M, base_ptr, byte_offset, tsan_reads.empty());

      // cleanup replaced TSAN calls
      for (auto call : bp2call.getSecond()) {
        auto arg0 = call->getArgOperand(0);
        if (arg0 == base_ptr) {
          remove_inst_from_func(call, bp, base_ptr_to_call, call_to_base_ptr);
          removed_tsan_calls++;
        } else {
          for (auto offPtr : offset_ptrs) {
            if (arg0 == offPtr.first) {
              remove_inst_from_func(call, bp, base_ptr_to_call,
                                    call_to_base_ptr);
              removed_tsan_calls++;
            }
          }
        }
      }
    }
  bp2call:;
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
          } else if (func_name.starts_with("__tsan_vptr")) {
            // TODO
            // HPCCG has `call void @__tsan_vptr_update(ptr nonnull %3,
            // ptr nonnull getelementptr inbounds nuw inrange(-16, 16)
            // (i8, ptr @_ZTVSt9basic_iosIcSt11char_traitsIcEE, i64 16))`
          } else {
            call->dump();
            llvm_unreachable("TSAN Combiner: only read or write call expected");
          }
        }
      }
    }

    removed_tsan_calls += remove_tsan_calls_in_func(tsan_calls, M);
  }

  // print statistics
  return "Removed TSAN calls: " + std::to_string(removed_tsan_calls);
}
