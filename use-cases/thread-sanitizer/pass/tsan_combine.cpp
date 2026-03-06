//
// Created by Jan Braun on 29.10.25.
//

#include "tsan_slicing_cleanup.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/ModuleInliner.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include <cassert>
#include <cstdint>

#include "precompute/compiler/analysis_results.h"

using namespace llvm;

#define b2c_map DenseMap<Value *, SmallDenseSet<CallBase *>>
#define c2b_map DenseMap<CallBase *, SmallDenseSet<Value *>>

#define common_parameter                                                       \
  Module &M, b2c_map &base_ptr_to_call, c2b_map &call_to_base_ptr,             \
      unsigned *removed_tsan_calls, unsigned *added_tsan_calls,                \
      const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values_write,         \
      const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values_read,          \
      Value *base_ptr

typedef void (*replace_func_t)(Module &, b2c_map &, c2b_map &, unsigned *,
                               unsigned *,
                               const DenseMap<Value *, DenseSet<CallBase *>> &,
                               const DenseMap<Value *, DenseSet<CallBase *>> &,
                               Value *);

static inline void collect_base_ptr_to_tsan_call(b2c_map &base_ptr_to_call,
                                                 c2b_map &call_to_base_ptr,
                                                 CallBase *tsan_call,
                                                 Value *val) {
  if (isa<Constant>(val))
    return;

  base_ptr_to_call[val].insert(tsan_call);
  call_to_base_ptr[tsan_call].insert(val);

  // value is uncertain -> do not map these DFG values to TSAN call
  if (isa<PHINode>(val) || isa<CallBase>(val) || isa<LoadInst>(val) ||
      isa<SelectInst>(val) || isa<AllocaInst>(val) || isa<FreezeInst>(val) ||
      isa<ExtractElementInst>(val) || isa<Argument>(val) || isa<CmpInst>(val))
    return;

  if (not(isa<GetElementPtrInst>(val) || isa<IntToPtrInst>(val) ||
          isa<CastInst>(val) || isa<BinaryOperator>(val))) {
    val->dump();
    llvm_unreachable("unexpected instruction in TSAN call DFG");
  }

  auto *inst = dyn_cast<Instruction>(val);
  assert(inst);
  for (auto &u : inst->operands())
    collect_base_ptr_to_tsan_call(base_ptr_to_call, call_to_base_ptr, tsan_call,
                                  u.get());
}

static void replace_invoke_term(Instruction *inst) {
  auto invoke = dyn_cast<InvokeInst>(inst);
  assert(invoke);

  IRBuilder<> builder(invoke);
  BasicBlock *normalDest = invoke->getNormalDest();
  builder.CreateBr(normalDest);
}

void remove_inst_from_func(Instruction *inst) {
  if (not inst->use_empty())
    inst->replaceAllUsesWith(PoisonValue::get(inst->getType()));
  if (inst->isTerminator())
    replace_invoke_term(inst);
  inst->eraseFromParent();
}

static inline void remove_inst_from_func(CallBase *call, const Value *base_ptr,
                                         b2c_map &base_ptr_to_call,
                                         c2b_map &call_to_base_ptr) {
  remove_inst_from_func(call);
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
  if (auto *STy = dyn_cast<StructType>(elemTy))
    if (elemTy != gep->getSourceElementType() || gep->getNumIndices() == 1)
      return STy;
  return nullptr;
}

CallInst *createTSANrange(Module &M, IRBuilder<> &builder, Value *base_ptr,
                          Value *struct_size, const bool isWrite) {
  assert(base_ptr);
  auto *ctx = &base_ptr->getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto voidTy = Type::getVoidTy(*ctx);
  auto int64Ty = Type::getInt64Ty(*ctx);

  auto func_name = isWrite ? "__tsan_write_range" : "__tsan_read_range";
  auto tsan_func = M.getOrInsertFunction(func_name, voidTy, ptrTy, int64Ty);
  return builder.CreateCall(tsan_func, {base_ptr, struct_size});
}

CallInst *createTSANrange(Module &M, Instruction *base_ptr,
                          const unsigned struct_size, const bool isWrite) {
  auto *ctx = &M.getContext();
  auto int64Ty = Type::getInt64Ty(*ctx);
  // TODO i64 might not always be applicable
  Value *struct_size_value = ConstantInt::get(int64Ty, struct_size, false);

  assert(base_ptr);
  IRBuilder<> builder(base_ptr->getNextNode());
  return createTSANrange(M, builder, base_ptr, struct_size_value, isWrite);
}

static inline bool check_path_to_base_ptr(const Instruction *inst,
                                          const Value *base_ptr) {
  if (not inst)
    return false;
  if (inst == base_ptr)
    return true;
  if (auto *gep = dyn_cast<GetElementPtrInst>(inst)) {
    auto ptr = gep->getPointerOperand();
    return base_ptr == ptr;
  }
  if (not isa<CastInst>(inst)) {
    for (auto &u : inst->operands())
      if (u.get() == base_ptr)
        return true;
    return false;
  }
  assert(inst->getNumOperands() == 1);
  auto arg0 = dyn_cast<Instruction>(inst->getOperand(0));
  return check_path_to_base_ptr(arg0, base_ptr);
}

#define createSCEV(Ty, Getter)                                                 \
  if (isa<Ty>(scev))                                                           \
    return SE.Getter(newOperands);

const SCEV *removeCastInSCEV(const SCEV *scev, ScalarEvolution &SE) {
  if (auto *scev_cast = dyn_cast<SCEVCastExpr>(scev))
    return removeCastInSCEV(scev_cast->getOperand(), SE);

  if (isa<SCEVConstant>(scev))
    return scev;
  // probably base_ptr or other variable
  if (isa<SCEVUnknown>(scev))
    return scev;

  if (auto *udiv = dyn_cast<SCEVUDivExpr>(scev))
    return SE.getUDivExpr(removeCastInSCEV(udiv->getLHS(), SE),
                          removeCastInSCEV(udiv->getRHS(), SE));

  SmallVector<const SCEV *, 4> newOperands;
  for (const SCEV *operand : scev->operands()) {
    const SCEV *scev_operand = removeCastInSCEV(operand, SE);
    if (not scev_operand)
      return nullptr;
    newOperands.push_back(scev_operand);
  }

  createSCEV(SCEVAddExpr, getAddExpr);
  createSCEV(SCEVMulExpr, getMulExpr);
  createSCEV(SCEVSMaxExpr, getSMaxExpr);
  if (auto *addRec = dyn_cast<SCEVAddRecExpr>(scev))
    return SE.getAddRecExpr(newOperands, addRec->getLoop(),
                            addRec->getNoWrapFlags());

  errs() << scev->getSCEVType() << ": ";
  scev->dump();
  llvm_unreachable("unknown SCEV operation");
}

static void getIdxVec(const DataLayout DL, const StructType *STy,
                      const StructLayout *SL, const uint64_t memberOffset,
                      SmallVector<unsigned> &offsetVector) {
  unsigned structIdx = SL->getElementContainingOffset(memberOffset);
  assert(0 <= structIdx && structIdx <= STy->elements().size());
  offsetVector.push_back(structIdx);
  if (auto elemSTy = dyn_cast<StructType>(STy->getElementType(structIdx))) {
    auto elemSL = DL.getStructLayout(elemSTy);
    auto elemOffset = memberOffset - SL->getElementOffset(structIdx);
    getIdxVec(DL, elemSTy, elemSL, elemOffset, offsetVector);
  }
}

static uint64_t getOffsetAfterIdx(const DataLayout DL, const StructType *STy,
                                  const StructLayout *SL,
                                  const uint64_t memberOffset) {
  unsigned structIdx = SL->getElementContainingOffset(memberOffset);
  assert(0 <= structIdx && structIdx <= STy->elements().size());
  if (auto elemSTy = dyn_cast<StructType>(STy->getElementType(structIdx))) {
    auto elemSL = DL.getStructLayout(elemSTy);
    auto elemOffset = memberOffset - SL->getElementOffset(structIdx);
    auto maxOffset = getOffsetAfterIdx(DL, elemSTy, elemSL, elemOffset);
    return memberOffset + maxOffset;
  } else {
    auto mo = memberOffset;
    auto structMaxBytes = SL->getSizeInBytes();
    while (SL->getElementContainingOffset(mo) == structIdx) {
      mo++;
      if (mo == structMaxBytes)
        break;
    };
    return mo;
  }
}

static void
range_replace_struct(Module &M, b2c_map &base_ptr_to_call,
                     c2b_map &call_to_base_ptr, unsigned *removed_tsan_calls,
                     unsigned *added_tsan_calls, bool isWrite,
                     const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values,
                     GetElementPtrInst *base_ptr) {
  // replace:
  //   %struct.a = type { i32, i32, i32 }
  //   %base = getelementptr inbounds %struct.a, ptr %a, i64 0, ...
  //   %b1 = getelementptr inbounds nuw i8, ptr %base, i64 4
  //   %b2 = getelementptr inbounds nuw i8, ptr %base, i64 8
  // with:
  //   call void @__tsan_write_range(ptr nonnull %base, i64 12)

  // combine contingous address range to TSAN range call
  SmallVector<GetElementPtrInst *, 10> offset_ptrs;
  const auto STy = getGEPstructTy(base_ptr);
  if (!STy)
    return;
  assert(not STy->elements().empty());

  for (auto p2c : ptr_values) {
    auto *ptr = p2c.getFirst();
    assert(p2c.getSecond().size() == 1);
    if (auto offset_gep = dyn_cast<GetElementPtrInst>(ptr)) {
      if (base_ptr == offset_gep) {
        offset_ptrs.push_back(offset_gep);
        continue;
      }
      auto base_gep_op = offset_gep->getPointerOperand();
      if (base_ptr != base_gep_op)
        continue;
      assert(offset_gep->getNumIndices() == 1);
      auto idx0 = offset_gep->indices().begin()->get();
      assert(idx0);
      if (not isa<ConstantInt>(idx0))
        continue;
      offset_ptrs.push_back(offset_gep);
    }
  }

  if (offset_ptrs.size() < 2)
    return;
  assert(not offset_ptrs.empty());

  auto byOffset = [&](const GetElementPtrInst *LHS,
                      const GetElementPtrInst *RHS) {
    assert(LHS && RHS);
    if (RHS == base_ptr)
      return false;
    if (LHS == base_ptr)
      return true;
    auto LHS_idx0 = cast<ConstantInt>(LHS->indices().begin()->get());
    auto RHS_idx0 = cast<ConstantInt>(RHS->indices().begin()->get());
    assert(not LHS_idx0->isNegative());
    assert(not RHS_idx0->isNegative());
    return LHS_idx0->getZExtValue() < RHS_idx0->getZExtValue();
  };
  sort(offset_ptrs, byOffset);

  auto getMemberOffset = [](const GetElementPtrInst *base_ptr,
                            const GetElementPtrInst *offPtr) {
    uint64_t offset;
    if (base_ptr == offPtr) {
      offset = 0;
    } else {
      assert(offPtr->getNumIndices() == 1);
      auto idxGEP = cast<ConstantInt>(offPtr->indices().begin()->get());
      offset = idxGEP->getZExtValue();
    }
    return offset;
  };
  auto range_replace_struct_part = [&](Instruction *base_ptr_part,
                                       unsigned range_size,
                                       DenseSet<CallBase *> calls) {
    assert(0 < range_size);
    auto *newCall = createTSANrange(M, base_ptr_part, range_size, isWrite);
    newCall->setDebugLoc((*calls.begin())->getDebugLoc());
    (*added_tsan_calls)++;

    // cleanup replaced TSAN calls
    for (auto call : calls) {
      remove_inst_from_func(call, base_ptr, base_ptr_to_call, call_to_base_ptr);
      (*removed_tsan_calls)++;
    }
  };

  const auto DL = M.getDataLayout();
  const auto *SL = DL.getStructLayout(STy);

  auto isAdjacentOffVec = [&](const SmallVector<unsigned> LHS,
                              const SmallVector<unsigned> RHS) {
    auto *LHS_STy = STy;
    unsigned i = 0;
    for (; i < LHS.size() && i < RHS.size(); i++) {
      assert(LHS[i] <= RHS[i]);
      if (1 < RHS[i] - LHS[i])
        return false;

      auto *LHS_elem = LHS_STy->getElementType(LHS[i]);
      assert(LHS_elem);
      if (auto *next_LHS_STy = dyn_cast<StructType>(LHS_elem))
        LHS_STy = next_LHS_STy;
      else
        break;

      if (RHS[i] != LHS[i])
        break;
    }

    // LHS always last idx
    unsigned j = ++i;
    for (; j < LHS.size(); ++j) {
      if (LHS[j] != STy->elements().size())
        return false;

      auto *LHS_elem = LHS_STy->getElementType(LHS[j]);
      if (auto *next_LHS_STy = dyn_cast<StructType>(LHS_elem))
        LHS_STy = next_LHS_STy;
      else {
        j++;
        break;
      }
    }
    assert(j == LHS.size());

    // RHX always first idx
    for (unsigned k = i; k < RHS.size(); ++k)
      if (RHS[k] != 0)
        return false;

    return true;
  };

  DenseSet<CallBase *> calls;
  SmallVector<unsigned> startIdx, lastIdx;
  uint64_t startOffset;
  GetElementPtrInst *startPtr, *lastPtr;
  startPtr = nullptr;
  lastPtr = offset_ptrs.back();

  // check for contingous part ranges
  for (auto *offPtr : offset_ptrs) {
    auto memberOffset = getMemberOffset(base_ptr, offPtr);
    SmallVector<unsigned> idxMember;
    getIdxVec(DL, STy, SL, memberOffset, idxMember);

    // first iteration
    if (not startPtr) {
      lastIdx = startIdx = idxMember;
      startPtr = offPtr;
      startOffset = memberOffset;
    }
    if (lastPtr == offPtr) {
      // if there is a gap before the last element, it remains alone
      if (not isAdjacentOffVec(lastIdx, idxMember))
        continue;
      // if last element, then it can be combined with previous elements
      lastIdx = idxMember;
    }
    auto vc = ptr_values.lookup(offPtr);
    assert(vc.size() == 1);
    calls.insert(*vc.begin());

    if (not isAdjacentOffVec(lastIdx, idxMember) || lastPtr == offPtr) {
      // if more than one call
      if (startPtr != offPtr) {
        assert(startPtr);
        auto offsetOoR = getOffsetAfterIdx(DL, STy, SL, memberOffset);
        range_replace_struct_part(startPtr, offsetOoR - startOffset, calls);
        calls.clear();
      }
      startIdx = idxMember;
      startPtr = offPtr;
      startOffset = memberOffset;
    }
    lastIdx = idxMember;
  }
}

static void
range_replace_array(Module &M, b2c_map &base_ptr_to_call,
                    c2b_map &call_to_base_ptr, unsigned *removed_tsan_calls,
                    unsigned *added_tsan_calls, bool isWrite,
                    const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values,
                    const Value *base_ptr) {
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

  auto *cb = *ptr_values.begin()->getSecond().begin();
  auto *SE = analysis_results->getSE(*cb->getFunction());
  SmallVector<std::pair<Value *, const SCEV *>, 10> offset_ptrs;

  uint64_t align = 0;
  if (auto *arg = dyn_cast<Argument>(base_ptr))
    if (arg->hasAttribute(Attribute::Alignment))
      align = arg->getAttribute(Attribute::Alignment).getValueAsInt();

  for (auto p2c : ptr_values) {
    auto *ptr = p2c.getFirst();

    if (auto *ptr_gep = dyn_cast<GetElementPtrInst>(ptr)) {
      if (not check_path_to_base_ptr(ptr_gep, base_ptr))
        continue;

      if (getGEPstructTy(ptr_gep)) {
        // if struct, array should have gep + add pattern
        if (1 < ptr_gep->getNumIndices())
          continue;
        if (isa<ConstantInt>(ptr_gep->indices().begin()->get()))
          continue;
      } else if (align) {
        // try eliminating struct replacement by checking alignment
        for (auto *call : p2c.getSecond())
          if (get_size_of_tsan_access(call)->getZExtValue() != align)
            return;
      } else {
        // try eliminating struct replacement by checking alignment
        for (auto *call : p2c.getSecond()) {
          auto align_tsan = get_size_of_tsan_access(call)->getZExtValue();
          for (auto *user : call->getArgOperand(0)->users()) {
            if (auto *load = dyn_cast<LoadInst>(user))
              if (align_tsan < load->getAlign().value())
                return;
            if (auto *store = dyn_cast<StoreInst>(user))
              if (align_tsan < store->getAlign().value())
                return;
          }
        }
      }
      // primitive types are allowed to have a gep %base, %offset pattern
    } else {
      assert(base_ptr == ptr);
    }

    const auto *scev_gep = SE->getSCEV(ptr);
    if (not scev_gep)
      continue;
    // Without removing casts SCEV does not see a constant difference between
    // pointers. For example zero extend in `24 * zext(2 + b) + (-24) * zext(b)`
    // does not make it clear that the difference is a constant 48. We are only
    // interested in the difference if it is constant.
    // DO NEVER USE SCEVExpander on this!
    scev_gep = removeCastInSCEV(scev_gep, *SE);
    if (not scev_gep)
      continue;
    offset_ptrs.push_back(std::make_pair(ptr, scev_gep));
  }

  if (offset_ptrs.size() < 2)
    return;

  auto byOffset = [&SE](const std::pair<Value *, const SCEV *> LHS,
                        const std::pair<Value *, const SCEV *> RHS) {
    auto *ptr_diff = SE->getMinusSCEV(LHS.second, RHS.second);
    if (not ptr_diff)
      return false;
    if (auto *ptr_diff_const = dyn_cast<SCEVConstant>(ptr_diff))
      return ptr_diff_const->getAPInt().isNegative();
    return false;
  };
  sort(offset_ptrs, byOffset);

  auto getCall = [&](Value *gep) {
    CallBase *call = nullptr;
    auto calls2ptr = ptr_values.lookup(gep);
    if (calls2ptr.size() != 1)
      return call;
    call = *calls2ptr.begin();
    return call;
  };
  auto replace_array_or_reset = [&](Value *base_ptr_part,
                                    const unsigned range_size,
                                    const SmallVector<CallBase *, 8> &calls) {
    auto *firstCall = *calls.begin();
    IRBuilder<> b(firstCall);
    auto int64Ty = Type::getInt64Ty(firstCall->getContext());
    Value *range_val = ConstantInt::get(int64Ty, range_size, false);
    auto *newCall = createTSANrange(M, b, base_ptr_part, range_val, isWrite);
    newCall->setDebugLoc((*calls.begin())->getDebugLoc());
    (*added_tsan_calls)++;

    // cleanup replaced TSAN calls
    unsigned sumOldSizes = 0;
    for (auto call : calls) {
      sumOldSizes += get_size_of_tsan_access(call)->getZExtValue();
      remove_inst_from_func(call, base_ptr, base_ptr_to_call, call_to_base_ptr);
      (*removed_tsan_calls)++;
    }
    assert(sumOldSizes == range_size);
  };

  SmallVector<CallBase *, 8> calls;
  std::pair<Value *, const SCEV *> lastPtr, curPtr;
  Value *start_ptr;
  ConstantInt *lastTsanSize, *tsan_size;
  CallBase *call_cur, *call_last;
  curPtr = *offset_ptrs.begin();
  start_ptr = curPtr.first;
  call_cur = getCall(start_ptr);
  if (not call_cur)
    return;
  tsan_size = get_size_of_tsan_access(call_cur);
  unsigned accSize = 0;

  // check for contingous part ranges
  for (auto offPtr : offset_ptrs) {
    lastTsanSize = tsan_size;
    lastPtr = curPtr;
    call_last = call_cur;

    calls.push_back(call_last);
    accSize += lastTsanSize->getZExtValue();
    curPtr = offPtr;
    auto *ptr_gep = curPtr.first;
    call_cur = getCall(ptr_gep);
    if (not call_cur)
      return;
    tsan_size = get_size_of_tsan_access(call_cur);

    auto raor = [&]() {
      if (2 <= calls.size()) {
        assert(start_ptr);
        assert(0 < accSize);
        replace_array_or_reset(start_ptr, accSize, calls);
      }

      accSize = 0;
      calls.clear();
      start_ptr = ptr_gep;
    };

    auto *ptr_diff = SE->getMinusSCEV(curPtr.second, lastPtr.second);
    if (not ptr_diff) {
      raor();
      continue;
    }

    auto *ptr_diff_const = dyn_cast<SCEVConstant>(ptr_diff);
    if (not ptr_diff_const) {
      raor();
      continue;
    }

    const auto const_diff = ptr_diff_const->getAPInt();
    const auto lastSizeVal = lastTsanSize->getValue();
    assert(&const_diff);
    assert(not const_diff.isNegative());
    if (lastSizeVal.getBitWidth() != const_diff.getBitWidth() ||
        lastSizeVal != const_diff) {
      raor();
      continue;
    }

    if (offPtr == offset_ptrs.back()) {
      calls.push_back(call_cur);
      accSize += tsan_size->getZExtValue();
      raor();
    }
  }
}

static void
replace_same(Module &M, unsigned *removed_tsan_calls,
             unsigned *added_tsan_calls, bool isWrite,
             const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values) {
  for (auto p2c : ptr_values) {
    auto tsan_calls = p2c.getSecond();
    if (tsan_calls.size() < 2)
      continue;

    auto *tsan = *tsan_calls.begin();
    auto *tsan_size = get_size_of_tsan_access(tsan);

    for (auto *call : tsan_calls) {
      if (call == tsan)
        continue;

      assert(tsan_size == get_size_of_tsan_access(tsan));
      remove_inst_from_func(call);
      (*removed_tsan_calls)++;
    }
  }
}

static void same_wrapper(common_parameter) {
  auto rrs = [&](const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values,
                 bool isWrite) {
    replace_same(M, removed_tsan_calls, added_tsan_calls, isWrite, ptr_values);
  };
  rrs(ptr_values_write, true);
  rrs(ptr_values_read, false);
}

static std::pair<unsigned, unsigned>
remove_wrapper(replace_func_t replace_func, Module &M,
               const SmallDenseSet<CallBase *> &tsan_calls) {
  unsigned removed_tsan_calls = 0;
  unsigned added_tsan_calls = 0;

  b2c_map base_ptr_to_call;
  c2b_map call_to_base_ptr;

  for (auto *ts : tsan_calls) {
    auto arg0 = ts->getArgOperand(0);
    if (replace_func == same_wrapper) {
      base_ptr_to_call[arg0].insert(ts);
    } else {
      if (Instruction *inst0 = dyn_cast<Instruction>(arg0))
        collect_base_ptr_to_tsan_call(base_ptr_to_call, call_to_base_ptr, ts,
                                      inst0);
    }
  }

  for (auto bp2call : base_ptr_to_call) {
    auto call_list = bp2call.getSecond();
    if (call_list.size() < 2)
      continue;

    DenseMap<Value *, DenseSet<CallBase *>> ptr_values_read, ptr_values_write;

    for (auto call : call_list) {
      auto func_name = getCallName(call).value();
      auto arg0 = call->getArgOperand(0);
      if (func_name.starts_with("__tsan_write"))
        ptr_values_write[arg0].insert(call);
      else if (func_name.starts_with("__tsan_read"))
        ptr_values_read[arg0].insert(call);
    }

    // nothing to do
    if (ptr_values_write.empty() && ptr_values_read.empty())
      continue;

    replace_func(M, base_ptr_to_call, call_to_base_ptr, &removed_tsan_calls,
                 &added_tsan_calls, ptr_values_write, ptr_values_read,
                 bp2call.getFirst());
    if (removed_tsan_calls || added_tsan_calls)
      return std::make_pair(removed_tsan_calls, added_tsan_calls);
  }

  return std::make_pair(removed_tsan_calls, added_tsan_calls);
}

static void struct_wrapper(common_parameter) {
  if (auto *bp = dyn_cast<GetElementPtrInst>(base_ptr)) {
    auto rrs = [&](const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values,
                   bool isWrite) {
      range_replace_struct(M, base_ptr_to_call, call_to_base_ptr,
                           removed_tsan_calls, added_tsan_calls, isWrite,
                           ptr_values, bp);
    };
    rrs(ptr_values_write, true);
    rrs(ptr_values_read, false);
  }
}

static void array_wrapper(common_parameter) {
  auto rra = [&](const DenseMap<Value *, DenseSet<CallBase *>> &ptr_values,
                 bool isWrite) {
    if (ptr_values.empty())
      return;
    auto *pvb = (*ptr_values.begin()).getFirst();
    assert(pvb);
    if (isa<GetElementPtrInst>(pvb)) {
      range_replace_array(M, base_ptr_to_call, call_to_base_ptr,
                          removed_tsan_calls, added_tsan_calls, isWrite,
                          ptr_values, base_ptr);
    }
  };
  rra(ptr_values_write, true);
  rra(ptr_values_read, false);
}

static bool wrap_happens_before_replace(replace_func_t replace_func,
                                        BasicBlock::iterator *start, Module &M,
                                        BasicBlock &BB,
                                        unsigned *removed_tsan_calls,
                                        unsigned *added_tsan_calls) {
  bool hasChanged = false;
  SmallDenseSet<CallBase *> tsan_calls;

  auto end = BB.end();
  for (auto it = *start; it != end; ++it) {
    *start = it;
    auto &inst = *it;
    DenseSet<Function *> alreadyVisited;
    if (mightInfluenceHappensBefore(&inst, alreadyVisited))
      break;

    if (auto *call = dyn_cast<CallBase>(&inst)) {
      if (not isAcceptableTsanCall(call))
        continue;

      auto call_name = getCallName(call);
      assert(call_name.has_value());
      auto func_name = call_name.value();
      assert(func_name.starts_with("__tsan"));
      if (func_name.starts_with("__tsan_unaligned"))
        continue;

      tsan_calls.insert(call);
    }
  }

  if (not tsan_calls.empty()) {
    auto tc = remove_wrapper(replace_func, M, tsan_calls);
    if (tc.first || tc.second)
      hasChanged = true;
    *removed_tsan_calls += tc.first;
    *added_tsan_calls += tc.second;
  }

  return hasChanged;
}

static void wrap_BB_replace(replace_func_t replace_func, Module &M,
                            unsigned *removed_tsan_calls,
                            unsigned *added_tsan_calls) {
  for (Function &Func : M) {
    // do not instrument tsan itself
    if (Func.getName().starts_with("tsan.module_ctor"))
      continue;

    for (BasicBlock &BB : Func) {
      bool hasChanged;
      do {
        hasChanged = false;
        for (auto it = BB.begin(); it != BB.end(); it++) {
          hasChanged |= wrap_happens_before_replace(
              replace_func, &it, M, BB, removed_tsan_calls, added_tsan_calls);
        }

      } while (hasChanged);
    }
  }
}

static inline void opt_cleanup(Module &M, ModuleAnalysisManager &AM) {
  auto inliner = llvm::ModuleInlinerPass();
  inliner.run(M, AM);

  auto dce = llvm::GlobalDCEPass();
  dce.run(M, AM);
}

std::string combine_tsan_calls(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Combine multiple TSAN calls with range call\n";
  unsigned removed_tsan_calls = 0;
  unsigned added_tsan_calls = 0;

  unsigned old_removed, old_added;

  do {
    old_removed = removed_tsan_calls;
    old_added = added_tsan_calls;
    wrap_BB_replace(same_wrapper, M, &removed_tsan_calls, &added_tsan_calls);
    wrap_BB_replace(struct_wrapper, M, &removed_tsan_calls, &added_tsan_calls);
    wrap_BB_replace(array_wrapper, M, &removed_tsan_calls, &added_tsan_calls);
    opt_cleanup(M, AM);
  } while (old_removed != removed_tsan_calls || old_added != added_tsan_calls);

  // print statistics
  return "removed TSAN calls: " + std::to_string(removed_tsan_calls) +
         "\nreplaced with: " + std::to_string(added_tsan_calls);
}
