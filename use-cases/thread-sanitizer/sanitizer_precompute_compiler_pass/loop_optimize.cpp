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

static uint64_t
openMPboundFix(Function *func,
               DenseMap<Value *, SmallDenseSet<Use *>> &boundReplacement,
               Loop **loop) {
  if (not func->getName().contains(".omp_outlined."))
    return 0;

  auto call_pair = getCallInFunc(func, "__kmpc_for_static_init", true);
  auto *omp_for_static = call_pair.second;
  if (not omp_for_static)
    return 0;

  // because we are reducing `a[i] = a[i+1]` to `a[0] = a[1]`,
  // there exists a risk the first thread executes both conflicting pointers,
  // if `1 < chunk_size`.
  auto *chunk_size = dyn_cast<ConstantInt>(omp_for_static->getArgOperand(8));
  if (not chunk_size)
    return 0;
  assert(not chunk_size->isNegative());
  auto cs = chunk_size->getZExtValue();
  if (cs != 1)
    return cs;
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

static bool replace_tsan_ranges(Module &M, ScalarEvolution *SE, Loop *loop,
                                CallBase *call, const unsigned chunk_size) {
  auto called_func = call->getCalledFunction();
  auto func_name = called_func->getName();

  auto *call_arg_0 = call->getArgOperand(0);
  auto *tsan_size = get_size_of_tsan_access(call);
  if (not tsan_size) {
    return false;
  }

  auto runTSANonlyOnce =
      [&](std::function<void(IRBuilder<> & tsanBuilder)> tsanInserter) {
        // There was the idea to just use a PhiNode, but sometimes the loop
        // inside the OpenMP outlined function is nested. Therefore, we flip a
        // boolean on first use.
        auto *bbEntry = &call->getFunction()->getEntryBlock();
        auto insertEntry = bbEntry->getFirstNonPHIOrDbgOrAlloca();
        IRBuilder<> entryBuilder(bbEntry);
        entryBuilder.SetInsertPoint(insertEntry);

        auto *ctx = &call->getContext();
        auto *i1Ty = entryBuilder.getInt1Ty();
        auto *constTrue = entryBuilder.getTrue();
        auto *flag = entryBuilder.CreateAlloca(i1Ty);
        entryBuilder.CreateStore(constTrue, flag);

        auto origInserter = [&](IRBuilder<> &origBuilder) {
          auto *flagLoad = origBuilder.CreateLoad(i1Ty, flag);
          Value *isEQ = origBuilder.CreateICmpEQ(flagLoad, constTrue);
          return isEQ;
        };
        auto tsanInserterGeneric = [&](IRBuilder<> &tsanBuilder) {
          tsanBuilder.CreateStore(ConstantInt::getFalse(*ctx), flag);
          tsanInserter(tsanBuilder);
        };
        splitBBexecOnce(call, origInserter, tsanInserterGeneric);
      };

  if (loop->isLoopInvariant(call_arg_0)) {
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      call->moveAfter(tsanBuilder.GetInsertPoint());
    };
    // runTSANonlyOnce(tsanInserter);
    // return true;
    return false;
  }

  auto scev = SE->getSCEV(call_arg_0);
  if (not SE->hasComputableLoopEvolution(scev, loop))
    return false;

  auto *addRec = dyn_cast<SCEVAddRecExpr>(scev);
  if (not addRec)
    return false;
  if (addRec->getLoop() != loop) {
    // could be AddRec for different loop
    return false;
  }

  // only if contingous address range
  auto *stepConstant = dyn_cast<SCEVConstant>(addRec->getStepRecurrence(*SE));
  if (not stepConstant)
    return false;
  if (stepConstant->getAPInt().abs() != tsan_size->getValue())
    return false;

  // TODO i64 might not always be applicable
  auto *ctx = &M.getContext();
  auto ptrTy = PointerType::get(*ctx, 0);
  auto int64Ty = Type::getInt64Ty(*ctx);
  auto constOne = ConstantInt::get(int64Ty, 1);

  const SCEV *tripCount;
  if (1 < chunk_size) {
    // tsanInserter adds the one back later
    tripCount = SE->getConstant(int64Ty, chunk_size - 1);
  } else {
    tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);
    assert(tripCount);
    assert(not isa<SCEVCouldNotCompute>(tripCount));
  }
  auto *start = addRec->getStart();
  auto *stop = addRec->evaluateAtIteration(tripCount, *SE);

  // TODO non-constant iteration counts
  if (not isa<SCEVConstant>(tripCount))
    return false;

  SCEVExpander seExpander(*SE, M.getDataLayout(), "scev");
  seExpander.setInsertPoint(call);

  auto *lower_bound = stepConstant->getAPInt().isNegative() ? stop : start;
  auto *val_min = seExpander.expandCodeFor(lower_bound, int64Ty);
  if (isa<PoisonValue>(val_min))
    return false;

  CallBase *newCall;
  auto createTSANcall = [&](IRBuilder<> &builder, Value *base_ptr) {
    // length = size_of(element) * (abs(last - base) + 1)
    seExpander.setInsertPoint(builder.GetInsertPoint());
    auto *iter_count = seExpander.expandCodeFor(tripCount, int64Ty);
    auto *count_full = builder.CreateAdd(iter_count, constOne);
    auto *range_full = builder.CreateMul(count_full, tsan_size);
    bool isWrite = func_name.starts_with("__tsan_write") ||
                   func_name.starts_with("__tsan_unaligned_write");
    newCall = createTSANrange(M, builder, base_ptr, range_full, isWrite);
  };

  Value *base_ptr;
  if (1 < chunk_size) {
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      base_ptr = tsanBuilder.CreateIntToPtr(val_min, ptrTy);
      createTSANcall(tsanBuilder, base_ptr);
    };
    runTSANonlyOnce(tsanInserter);
  } else {
    return false;

    auto origInserter = [&](IRBuilder<> &origBuilder) {
      base_ptr = origBuilder.CreateIntToPtr(val_min, ptrTy);
      Value *isEQ = origBuilder.CreateICmpEQ(call_arg_0, base_ptr);
      return isEQ;
    };
    auto tsanInserter = [&](IRBuilder<> &tsanBuilder) {
      createTSANcall(tsanBuilder, base_ptr);
    };
    splitBBexecOnce(call, origInserter, tsanInserter);
  }

  newCall->setDebugLoc(call->getDebugLoc());
  remove_inst_from_func(call);
  return true;
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
  for (auto *call : tsan_in_loop)
    if (replace_tsan_ranges(M, SE, loop, call, chunk_size))
      removed_tsan_calls++;

  // Rollback: Do not break OpenMP thread handling
  for (auto br : boundReplacement) {
    auto *load = br.getFirst();
    for (Use *u : br.getSecond())
      u->set(load);
  }

  return removed_tsan_calls;
}

std::string optimize_loops(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Optimize Loops\n";
  unsigned removed_tsan_calls = 0;

  for (auto &f : M) {
    if (not f.isDeclaration() && not is_func_from_std(&f)) {
      auto li = analysis_results->getLoopInfo(f);
      for (auto loop : li->getLoopsInPreorder()) {
        std::vector<llvm::CallBase *> tsan_calls;

        for (auto &bb : loop->getBlocks())
          for (auto &inst : *bb)
            if (auto *call = dyn_cast<CallBase>(&inst))
              if (isAcceptableTsanCall(call))
                tsan_calls.push_back(call);

        removed_tsan_calls += perform_tsan_licm(M, loop, tsan_calls);
      }
    }
  }

  // print statistics
  return "invariant/unrolled TSAN calls: " + std::to_string(removed_tsan_calls);
}
