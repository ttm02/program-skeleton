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

static inline CallBase *getCallInFunc(Function *func,
                                      std::string func_target_name,
                                      bool starts_with = false) {
  for (BasicBlock &bb : *func) {
    for (Instruction &inst : bb) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {
        auto call_name = getCallName(call);
        if (not call_name.has_value())
          continue;
        auto func_name = call_name.value();
        if (starts_with) {
          if (func_name.starts_with(func_target_name))
            return call;
        } else {
          if (func_name == func_target_name)
            return call;
        }
      }
    }
  }
  return nullptr;
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

static uint64_t
openMPboundFix(Function *func,
               DenseMap<Value *, SmallDenseSet<Use *>> &boundReplacement,
               Loop **loop) {
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

  // TODO chunk size of 1
  // TODO non-constant iteration counts
  return 0;
  auto omp_lower = omp_for_static->getArgOperand(4);
  getBoundLoadStoreReplacement(omp_lower, boundReplacement);
  auto omp_upper = omp_for_static->getArgOperand(5);
  getBoundLoadStoreReplacement(omp_upper, boundReplacement);
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

struct loopTSANdata {
  bool isWrite;
  Value *val_min;
  Value *call_arg_0;
  CallBase *call;
  ConstantInt *tsan_size;
  const SCEV *tripCount;

  bool operator==(const loopTSANdata &other) const {
    return isWrite == other.isWrite && val_min == other.val_min &&
           call_arg_0 == other.call_arg_0 && call == other.call &&
           tsan_size == other.tsan_size && tripCount == other.tripCount;
  }
};

template <> struct DenseMapInfo<loopTSANdata> {
  static inline loopTSANdata getEmptyKey() { return {}; }
  static inline loopTSANdata getTombstoneKey() { return {}; }

  static unsigned getHashValue(const loopTSANdata &V) {
    return llvm::hash_combine(V.isWrite, V.val_min, V.call_arg_0, V.call,
                              V.tsan_size, V.tripCount);
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

  SCEVExpander seExpander(*SE, M.getDataLayout(), "scev");
  seExpander.setInsertPoint(data.call);

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
            if (auto *call = dyn_cast<CallBase>(u)) {
              auto call_name = getCallName(call);
              if (not call_name.has_value())
                continue;
              if (not call_name.value().starts_with("__kmpc_dispatch_next"))
                continue;

              IRBuilder<> dispatchBuilder(call);
              dispatchBuilder.CreateStore(constTrue, flag);
            }
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
    seExpander.setInsertPoint(builder.GetInsertPoint());
    auto *iter_count = seExpander.expandCodeFor(data.tripCount, int64Ty);
    auto *count_full = builder.CreateAdd(iter_count, constOne);
    auto *range_full = builder.CreateMul(count_full, data.tsan_size);
    newCall = createTSANrange(M, builder, base_ptr, range_full, data.isWrite);
  };

  Value *base_ptr;
  if (1 < chunk_size) {
    // only first iteration of chunk range
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      base_ptr = tsanBuilder.CreateIntToPtr(data.val_min, ptrTy);
      createTSANcall(tsanBuilder, base_ptr);
    };
    runTSANonlyOnce(tsanInserter);
  } else {
    // TODO unroll whole loop (if chunk size is 1)
    return false;

    auto origInserter = [&](IRBuilder<> &origBuilder) {
      base_ptr = origBuilder.CreateIntToPtr(data.val_min, ptrTy);
      Value *isEQ = origBuilder.CreateICmpEQ(data.call_arg_0, base_ptr);
      return isEQ;
    };
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      createTSANcall(tsanBuilder, base_ptr);
    };
    splitBBexecOnce(data.call, origInserter, tsanInserter);
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

  data.call_arg_0 = call->getArgOperand(0);
  data.tsan_size = get_size_of_tsan_access(call);
  if (not data.tsan_size)
    return {};

  // TODO;
  if (loop->isLoopInvariant(data.call_arg_0)) {
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      call->moveAfter(tsanBuilder.GetInsertPoint());
    };
    // runTSANonlyOnce(tsanInserter);
    return {};
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

  // only if contingous address range
  auto *stepConstant = dyn_cast<SCEVConstant>(addRec->getStepRecurrence(*SE));
  if (not stepConstant)
    return {};
  if (stepConstant->getAPInt().abs() != data.tsan_size->getValue())
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

  SCEVExpander seExpander(*SE, M.getDataLayout(), "scev");
  seExpander.setInsertPoint(call);

  auto *lower_bound = stepConstant->getAPInt().isNegative() ? stop : start;
  data.val_min = seExpander.expandCodeFor(lower_bound, int64Ty);
  if (isa<PoisonValue>(data.val_min))
    return {};

  return data;
}

static unsigned perform_tsan_licm(Module &M, Loop *loop,
                                  const std::vector<CallBase *> &tsan_in_loop) {
  unsigned removed_tsan_calls = 0;

  auto *func = loop->getHeader()->getParent();
  DenseMap<Value *, SmallDenseSet<Use *>> boundReplacement;
  auto chunk_size = openMPboundFix(func, boundReplacement, &loop);

  BasicBlock *incoming;
  BasicBlock *backedge;
  if (not loop->getIncomingAndBackEdge(incoming, backedge))
    return 0;

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
  for (auto d : data)
    if (create_tsan_replacement(M, d, SE, chunk_size))
      removed_tsan_calls++;

  // Rollback: Do not break OpenMP thread handling
  for (auto br : boundReplacement) {
    auto *load = br.getFirst();
    for (Use *u : br.getSecond())
      u->set(load);
  }

  return removed_tsan_calls;
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
    auto *called_func = call->getCalledFunction();
    if (not called_func)
      return false; // fingers crossed ....

    if (is_thread_function(called_func))
      return true;

    if (mightInfluenceHappensBefore(called_func, alreadyVisited))
      return true;
  }

  return false;
}

static void loopWrapper(unsigned *removed_tsan_calls, Module &M, Loop *loop) {
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

  (*removed_tsan_calls) += perform_tsan_licm(M, loop, tsan_calls);
}

std::string optimize_loops(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Optimize Loops\n";
  unsigned removed_tsan_calls = 0;

  for (auto &f : M) {
    if (not f.isDeclaration() && not is_func_from_std(&f)) {
      auto li = analysis_results->getLoopInfo(f);
      for (auto *loop : li->getLoopsInPreorder())
        loopWrapper(&removed_tsan_calls, M, loop);
    }
  }

  // print statistics
  return "invariant/unrolled TSAN calls: " + std::to_string(removed_tsan_calls);
}
