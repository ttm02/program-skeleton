//
// Created by tim on 12.05.25.
//

#include "tsan_slicing_cleanup.h"

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

static inline CallBase *getCallInFunc(Function *func,
                                      std::string func_target_name,
                                      bool starts_with = false) {
  CallBase *callVal = nullptr;
  for (BasicBlock &bb : *func) {
    for (Instruction &inst : bb) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {
        auto call_name = getCallName(call);
        if (not call_name.has_value())
          continue;
        auto func_name = call_name.value();
        if (starts_with) {
          if (func_name.starts_with(func_target_name)) {
            if (callVal)
              return nullptr;
            else
              callVal = call;
          }
        } else {
          if (func_name == func_target_name) {
            if (callVal)
              return nullptr;
            else
              callVal = call;
          }
        }
      }
    }
  }
  return callVal;
}

static uint64_t getOpenMPbounds(Function *func, Loop **loop) {
  if (not func->getName().contains(".omp_outlined."))
    return 0;

  auto omp_for_static = getCallInFunc(func, "__kmpc_for_static_init", true);
  auto omp_for_dynamic = getCallInFunc(func, "__kmpc_dispatch_init", true);
  if (not omp_for_static && not omp_for_dynamic)
    return 0;
  assert(not omp_for_static || not omp_for_dynamic);

  // because we are reducing `a[i] = a[i+1]` to `a[0] = a[1]`,
  // there exists a risk the first thread executes both conflicting pointers,
  // if `1 < chunk_size`.
  ConstantInt *chunk_size = nullptr;
  if (omp_for_static)
    chunk_size = dyn_cast<ConstantInt>(omp_for_static->getArgOperand(8));
  else if (omp_for_dynamic)
    chunk_size = dyn_cast<ConstantInt>(omp_for_dynamic->getArgOperand(6));

  if (not chunk_size)
    return 0;
  assert(not chunk_size->isNegative());
  auto cs = chunk_size->getZExtValue();
  if (cs != 1)
    return cs;

  return 0;
}

void splitBBexecOnce(
    Instruction *inst,
    std::function<Value *(IRBuilder<> &origBuilder)> insertIntoOrigBB,
    std::function<void(IRBuilder<> &tsanBuilder)> insertIntoTsanBB) {
  // fix trailing DbgRecords in BasicBlock
  // we tried adoptDbgRecords(), but this does not work in DRB169
  auto *nextNode = inst->getNextNode();
  assert(nextNode);
  nextNode->dropDbgRecords();

  auto *ctx = &inst->getContext();
  auto *origBB = inst->getParent();
  auto *nextInst = inst->getNextNonDebugInstruction();
  auto *restBB = origBB->splitBasicBlock(nextInst);
  auto *tsanBB = BasicBlock::Create(*ctx, "tsan_bb", inst->getFunction());
  origBB->getTerminator()->eraseFromParent();

  IRBuilder<> origBuilder(origBB);
  origBuilder.SetInsertPoint(inst);
  auto cmp = insertIntoOrigBB(origBuilder);
  origBuilder.CreateCondBr(cmp, tsanBB, restBB);

  IRBuilder<> tsanBuilder(tsanBB);
  tsanBuilder.CreateBr(restBB);
  tsanBuilder.SetInsertPoint(tsanBB->begin());
  insertIntoTsanBB(tsanBuilder);
}

enum loopTSANtype {
  RANGE,
  INVARIANT,
  RESERVED,
};

struct loopTSANdata {
  loopTSANtype type;
  bool isWrite;
  Value *val_min;
  Value *call_arg_0;
  CallBase *call;
  ConstantInt *tsan_size;
  const SCEV *tripCount;
  SCEVExpander *seExpander;

  bool operator==(const loopTSANdata &other) const {
    return type == other.type && isWrite == other.isWrite &&
           val_min == other.val_min && call_arg_0 == other.call_arg_0 &&
           call == other.call && tsan_size == other.tsan_size &&
           tripCount == other.tripCount && seExpander == other.seExpander;
  }
};

template <> struct DenseMapInfo<loopTSANdata> {
  static inline loopTSANdata getEmptyKey() { return {}; }
  static inline loopTSANdata getTombstoneKey() { return {}; }

  static unsigned getHashValue(const loopTSANdata &V) {
    return llvm::hash_combine(V.type, V.isWrite, V.val_min, V.call_arg_0,
                              V.call, V.tsan_size, V.tripCount, V.seExpander);
  }

  static bool isEqual(const loopTSANdata &LHS, const loopTSANdata &RHS) {
    return LHS == RHS;
  }
};

static bool create_tsan_replacement(Module &M, const loopTSANdata data,
                                    ScalarEvolution *SE,
                                    const unsigned chunk_size) {
  auto *ctx = &M.getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto int64Ty = Type::getInt64Ty(*ctx);
  auto constOne = ConstantInt::get(int64Ty, 1);

  auto runTSANonlyOnce =
      [&](std::function<void(IRBuilder<> & tsanBuilder)> tsanInserter) {
        // There was the idea to just use a PhiNode, but sometimes the loop
        // inside the OpenMP outlined function is nested. Therefore, we flip a
        // boolean on first use.
        auto *func = data.call->getFunction();
        auto *bbEntry = &func->getEntryBlock();
        auto insertEntry = bbEntry->getFirstNonPHIOrDbgOrAlloca();
        IRBuilder<> entryBuilder(bbEntry);
        entryBuilder.SetInsertPoint(insertEntry);

        auto *ctx = &data.call->getContext();
        auto *i1Ty = entryBuilder.getInt1Ty();
        auto *constTrue = entryBuilder.getTrue();
        auto *flag = entryBuilder.CreateAlloca(i1Ty);
        entryBuilder.CreateStore(constTrue, flag);

        auto *omp_for_dynamic =
            getCallInFunc(func, "__kmpc_dispatch_init", true);
        if (omp_for_dynamic) {
          for (auto *u : omp_for_dynamic->getArgOperand(0)->users()) {
            auto *call = dyn_cast<CallBase>(u);
            if (not call)
              continue;
            if (call->getFunction() != func)
              continue;

            auto call_name = getCallName(call);
            if (not call_name.has_value())
              continue;
            if (not call_name.value().starts_with("__kmpc_dispatch_next"))
              continue;

            IRBuilder<> dispatchBuilder(call);
            dispatchBuilder.CreateStore(constTrue, flag);
          }
        }

        auto origInserter = [&](IRBuilder<> &origBuilder) {
          auto *flagLoad = origBuilder.CreateLoad(i1Ty, flag);
          Value *isEQ = origBuilder.CreateICmpEQ(flagLoad, constTrue);
          return isEQ;
        };
        auto tsanInserterGeneric = [&](IRBuilder<> &tsanBuilder) {
          tsanBuilder.CreateStore(ConstantInt::getFalse(*ctx), flag);
          tsanInserter(tsanBuilder);
        };
        splitBBexecOnce(data.call, origInserter, tsanInserterGeneric);
      };

  CallBase *newCall = nullptr;
  auto createTSANcall = [&](IRBuilder<> &builder, Value *base_ptr) {
    // length = size_of(element) * (abs(last - base) + 1)
    data.seExpander->setInsertPoint(builder.GetInsertPoint());
    auto *iter_count = data.seExpander->expandCodeFor(data.tripCount, int64Ty);
    auto *count_full = builder.CreateAdd(iter_count, constOne);
    auto *range_full = builder.CreateMul(count_full, data.tsan_size);
    newCall = createTSANrange(M, builder, base_ptr, range_full, data.isWrite);
  };

  auto fixSCEVinsertDomination = [&](IRBuilder<> &tsanBuilder) {
    auto *tsanBB = tsanBuilder.GetInsertBlock();
    auto SEinsertedList = data.seExpander->getAllInsertedInstructions();
    if (not SEinsertedList.empty()) {
      auto *valInst = dyn_cast<Instruction>(data.val_min);
      assert(valInst);
      auto *valBB = valInst->getParent();
      for (auto *inst : SEinsertedList)
        if (inst->getParent() != valBB)
          return;
      sort(SEinsertedList,
           [](Instruction *A, Instruction *B) { return A->comesBefore(B); });
      tsanBB->splice(tsanBuilder.GetInsertPoint(), valBB,
                     SEinsertedList.front()->getIterator(),
                     std::next(SEinsertedList.back()->getIterator()));
    }
  };

  Value *base_ptr;
  if (data.type == INVARIANT) {
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      data.call->moveAfter(tsanBuilder.GetInsertPoint());
    };
    runTSANonlyOnce(tsanInserter);
    return true;
  } else if (1 < chunk_size) {
    // only first iteration of chunk range
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      fixSCEVinsertDomination(tsanBuilder);
      base_ptr = tsanBuilder.CreateIntToPtr(data.val_min, ptrTy);
      createTSANcall(tsanBuilder, base_ptr);
    };
    runTSANonlyOnce(tsanInserter);
  } else {
    assert(data.type == RANGE);
    assert(chunk_size <= 1);
    return false;
  }

  newCall->setDebugLoc(data.call->getDebugLoc());
  remove_inst_from_func(data.call);
  return true;
}

static std::optional<loopTSANdata>
prepare_tsan_ranges(Module &M, ScalarEvolution *SE, Loop *loop, CallBase *call,
                    const unsigned chunk_size) {
  loopTSANdata data;

  // TODO i64 might not always be applicable
  auto *ctx = &M.getContext();
  auto int64Ty = Type::getInt64Ty(*ctx);

  auto func_name = getCallName(call).value();
  data.isWrite = func_name.starts_with("__tsan_write") ||
                 func_name.starts_with("__tsan_unaligned_write");

  data.type = RANGE;
  data.call_arg_0 = call->getArgOperand(0);
  data.tsan_size = get_size_of_tsan_access(call);
  if (not data.tsan_size)
    return {};

  if (loop->isLoopInvariant(data.call_arg_0)) {
    data.type = INVARIANT;
    return data;
  }

  auto scev = SE->getSCEV(data.call_arg_0);
  if (not SE->hasComputableLoopEvolution(scev, loop))
    return {};

  auto *addRec = dyn_cast<SCEVAddRecExpr>(scev);
  if (not addRec)
    return {};
  if (addRec->getLoop() != loop) {
    // could be AddRec for different loop
    return {};
  }

  // only if contigous address range
  auto *stepConstant = dyn_cast<SCEVConstant>(addRec->getStepRecurrence(*SE));
  if (not stepConstant)
    return {};
  // TSAN calls can be overlapping, but they have to be adjacent
  if (data.tsan_size->getValue().ult(stepConstant->getAPInt().abs()))
    return {};

  if (1 < chunk_size) {
    // tsanInserter adds the one back later
    data.tripCount = SE->getConstant(int64Ty, chunk_size - 1);
  } else {
    data.tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);
  }

  // TODO non-constant iteration counts
  assert(data.tripCount);
  if (not isa<SCEVConstant>(data.tripCount))
    return {};

  auto *start = addRec->getStart();
  auto *stop = addRec->evaluateAtIteration(data.tripCount, *SE);

  data.seExpander = new SCEVExpander(*SE, M.getDataLayout(), "scev");
  data.seExpander->setInsertPoint(call);

  auto *lower_bound = stepConstant->getAPInt().isNegative() ? stop : start;
  data.val_min = data.seExpander->expandCodeFor(lower_bound, int64Ty);
  if (isa<PoisonValue>(data.val_min)) {
    delete data.seExpander;
    return {};
  }

  return data;
}

static std::pair<unsigned, unsigned>
perform_tsan_licm(Module &M, Loop *loop,
                  const std::vector<CallBase *> &tsan_in_loop,
                  bool multipleLoopsInFunc) {
  unsigned removed_tsan_calls = 0;
  unsigned invariant_calls = 0;

  auto *func = loop->getHeader()->getParent();
  auto chunk_size = 0;
  if (not multipleLoopsInFunc)
    chunk_size = getOpenMPbounds(func, &loop);

  BasicBlock *incoming;
  BasicBlock *backedge;
  if (not loop->getIncomingAndBackEdge(incoming, backedge))
    std::make_pair(0, 0);

  auto *SE = analysis_results->getSE(*func);
  SmallDenseSet<loopTSANdata, 8> data;

  // first collect analysis data of all TSAN calls
  for (auto *call : tsan_in_loop) {
    auto opt_d = prepare_tsan_ranges(M, SE, loop, call, chunk_size);
    if (opt_d.has_value()) {
      auto d = opt_d.value();
      d.call = call;
      data.insert(d);
    }
  }

  // then apply transformation
  // this avoids destroying analysis data of other TSAN calls
  for (auto d : data) {
    if (create_tsan_replacement(M, d, SE, chunk_size)) {
      if (d.type == RANGE)
        removed_tsan_calls++;
      else
        invariant_calls++;
    }
  }

  return std::make_pair(removed_tsan_calls, invariant_calls);
}

static std::vector<std::string> func_names_whitelist = {
    "__kmpc_dispatch_next", "__kmpc_global_", "__kmpc_master",
    "__kmpc_single",        "omp_get_",       "omp_set_dynamic",
    "omp_set_num_threads",  "pthread_create",
};

static bool mightInfluenceHappensBefore(Function *func) {
  assert(is_thread_function(func));
  auto func_name = func->getName();
  assert(not func_name.empty());
  for (auto fn : func_names_whitelist)
    if (func_name.starts_with(fn))
      return false;
  return true;
}

bool mightInfluenceHappensBefore(Function *func,
                                 DenseSet<Function *> &alreadyVisited) {
  if (alreadyVisited.contains(func))
    return false;
  alreadyVisited.insert(func);

  for (BasicBlock &BB : *func)
    for (Instruction &inst : BB)
      if (mightInfluenceHappensBefore(&inst, alreadyVisited))
        return true;

  return false;
}

bool mightInfluenceHappensBefore(Instruction *inst,
                                 DenseSet<Function *> &alreadyVisited) {
  if (auto *call = dyn_cast<CallBase>(inst)) {
    SmallDenseSet<Function *> function_list;
    get_called_functions(call, function_list);
    for (auto *called_func : function_list) {
      if (is_thread_function(called_func))
        if (mightInfluenceHappensBefore(called_func))
          return true;
      if (called_func->isDeclaration())
        continue;
      if (mightInfluenceHappensBefore(called_func, alreadyVisited))
        return true;
    }
  }

  return false;
}

static void loopWrapper(unsigned *removed_tsan_calls, unsigned *invariant_calls,
                        Module &M, Loop *loop, bool mlif) {
  std::vector<llvm::CallBase *> tsan_calls;

  for (auto &bb : loop->getBlocks()) {
    for (auto &inst : *bb) {
      DenseSet<Function *> alreadyVisited;
      if (mightInfluenceHappensBefore(&inst, alreadyVisited))
        return;

      if (auto *call = dyn_cast<CallBase>(&inst)) {
        if (isAcceptableTsanCall(call))
          tsan_calls.push_back(call);
      }
    }
  }

  auto ptl_pair = perform_tsan_licm(M, loop, tsan_calls, mlif);
  (*removed_tsan_calls) += ptl_pair.first;
  (*invariant_calls) += ptl_pair.second;
}

static bool processLoopsInFunc(Module &M, Function &F,
                               unsigned *removed_tsan_calls,
                               unsigned *invariant_calls) {
  unsigned old_removed_count = *removed_tsan_calls;
  auto li = analysis_results->getLoopInfo(F);
  SmallVector<Loop *> loopList;
  for (auto *loop : li->getLoopsInPreorder())
    loopList.push_back(loop);
  if (loopList.empty())
    return false;

  auto byNestingDepth = [&](const Loop *LHS, const Loop *RHS) {
    assert(LHS && RHS);
    auto LHS_depth = LHS->getLoopDepth();
    auto RHS_depth = RHS->getLoopDepth();
    return LHS_depth > RHS_depth;
  };
  sort(loopList, byNestingDepth);

  unsigned zeroDepthLoops = 0;
  for (auto *loop : llvm::reverse(loopList)) {
    if (loop->getLoopDepth() == 1) // 1 is smallest depth
      zeroDepthLoops++;
    else
      break;
  }
  // TODO support multiple loops in OpenMP parallel regions
  bool mlif = 1 < zeroDepthLoops; // multiple loops in function

  for (auto *loop : loopList) {
    loopWrapper(removed_tsan_calls, invariant_calls, M, loop, mlif);
    if (old_removed_count != *removed_tsan_calls) {
      analysis_results->cleanup(F);
      return true;
    }
  }
  return false;
}

std::string optimize_loops(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Optimize Loops\n";
  unsigned removed_tsan_calls = 0;
  unsigned invariant_calls = 0;

  for (auto &F : M) {
    if (F.isDeclaration() || is_func_from_std(&F))
      continue;

    bool again = false;
    do {
      again = processLoopsInFunc(M, F, &removed_tsan_calls, &invariant_calls);
    } while (again);
  }

  // print statistics
  return "unrolled TSAN calls: " + std::to_string(removed_tsan_calls) + "\n" +
         "invariant TSAN calls: " + std::to_string(invariant_calls);
}
