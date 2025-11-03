
#include "tsan_precompute_cleanup.h"

#include "precompute/compiler/Precompute_insertion.h"
#include "precompute/compiler/analysis_results.h"
#include "precompute/compiler/debug.h"
#include "precompute/compiler/openmp_runtime_functions.h"
#include "precompute/compiler/precalculation.h"
#include "precompute/compiler/precompute_backend_funcs.h"
#include "precompute/compiler/std_funcs.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/ModuleInliner.h"
#include "llvm/Transforms/Instrumentation/ThreadSanitizer.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

RequiredAnalysisResults *analysis_results;

// removes attribute noinline from every func
// we previously set it to make analysis easier
static void remove_noinline_from_module(Module &M) {
  for (auto &F : M) {
    if (F.hasFnAttribute(Attribute::NoInline) and
        not F.hasFnAttribute(Attribute::OptimizeNone)) {
      F.removeFnAttr(Attribute::NoInline);
    }
  }
}

static bool run_optimization_passes(
    Module &M, ModuleAnalysisManager &AM,
    std::string (*opt_pass_func)(Module &M, ModuleAnalysisManager &AM),
    bool cleanup = true) {
  std::string opt_msg_success = opt_pass_func(M, AM);
  if (opt_msg_success.empty())
    return false;
#ifndef NDEBUG
  bool has_error = verifyModule(M, &errs(), nullptr);
  assert(!has_error);
#endif
  errs() << opt_msg_success << "\n";

  if (cleanup) {
    errs() << "Run inliner Pass\n";
    auto inliner = llvm::ModuleInlinerPass();
    inliner.run(M, AM);

    errs() << "Run Global DCE Pass\n";
    auto dce = llvm::GlobalDCEPass();
    dce.run(M, AM);

    errs() << "\n";
#ifndef NDEBUG
    has_error = verifyModule(M, &errs(), nullptr);
    assert(!has_error);
#endif
  }
  return true;
}

static bool is_tsan_cleanup_block(BasicBlock *block) {
  auto it = block->begin();
  if (it == block->end() || not isa<LandingPadInst>(it)) {
    return false;
  }
  ++it;
  if (it == block->end() || not isa<CallInst>(it) ||
      not cast<CallInst>(it)->isIndirectCall() ||
      not(cast<CallInst>(it)->getCalledFunction()->getName() ==
          "__tsan_func_exit")) {
    return false;
  }
  ++it;
  if (it == block->end() || not isa<ResumeInst>(it)) {
    return false;
  }
  ++it;
  return it == block->end();
}

// find compiler used in metadata
static bool is_compiled_with_flang(const Module &M) {
  if (auto *NMD = M.getNamedMetadata("llvm.ident")) {
    for (auto *Op : NMD->operands()) {
      if (auto *MDStr = dyn_cast<MDString>(Op->getOperand(0))) {
        return MDStr->getString().starts_with("flang");
      }
    }
  }
  errs() << "WARNING: Module contains no Metadata about compiler used!\n";
  return false;
}

static std::string run_tsan(Module &M, ModuleAnalysisManager &AM) {
  auto tsan_func_entry = M.getFunction("__tsan_func_entry");
  if (tsan_func_entry == nullptr || tsan_func_entry->users().empty()) {
    errs() << "Run TSAN pass\n";
    bool is_fortran_code = is_compiled_with_flang(M);
    //  make sure TSAN pass runs
    auto *FAM =
        &AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    auto tsan_pass = ThreadSanitizerPass();
    for (auto it = M.begin(); it != M.end(); ++it) {
      Function *f = &*it;
      if (is_fortran_code) {
        f->addFnAttr(Attribute::SanitizeThread);
        // work around for fortran, as fortran does currently not support
        // -fsanitize=thread flag
      }
      tsan_pass.run(*f, *FAM);
    }
    return "Successfully instrumented code with TSAN";
  }
  return "TSAN instrumentation already available";
}

static std::string run_precompute(Module &M, ModuleAnalysisManager &AM) {
  allow_function_prefixes_to_be_called_in_precompute({"__tsan_"});

  PrecomputeFunctions::create_instance(M);
  analysis_results = new RequiredAnalysisResults(AM, M);

  auto *main_func = M.getFunction("main");
  assert(main_func);

  DenseSet<Value *> to_precompute;
  DenseSet<Instruction *> precompute_locations;

  for (Function &f : M) {
    if (not f.getName().starts_with("tsan.module_ctor")) {
      // do not instrument tsan itself
      for (BasicBlock &bb : f) {
        for (Instruction &inst : bb) {
          if (auto call = dyn_cast<CallBase>(&inst)) {
            auto called_func = call->getCalledFunction();
            if (!called_func)
              continue;
            auto func_name = called_func->getName();
            // eiter tsan or omp function
            // omp function necessary e.g. to keep synchronization
            bool is_thread_func = is_thread_function(called_func);
            if (func_name.starts_with("__tsan") || is_thread_func) {
              if (func_name == "__tsan_func_exit") {
                if (isa<ResumeInst>(call->getNextNonDebugInstruction())) {
                  // TODO make this a compiler option!!
                  //  in cleanup block, dont handle exceptions
                  continue;
                }
                if (is_tsan_cleanup_block(call->getParent())) {
                  // skip, the tsan cleanup part.
                  // no need to precompute, as it will only handle fatal
                  // exceptions
                  continue;
                }
              }
              if (is_func_from_std(call->getFunction())) {
                // dont analyze internals of std, though tsan may instrument
                // them
                continue;
              }

              for (auto &it_arg : call->args())
                to_precompute.insert(it_arg);
              precompute_locations.insert(call);
            }
          }
        }
      }
    }
  }

  errs() << "Statistics: locations: " << precompute_locations.size()
         << " values: " << to_precompute.size() << "\n";

  // no tsan found
  if (precompute_locations.empty()) {
    // no modification
    return "";
  }

  auto precalcuation = std::make_shared<PrecomputeInsertion>(
      M,
      std::make_shared<PrecalculationAnalysis>(M, main_func, to_precompute,
                                               precompute_locations),
      false);

  // do NOT call clean_precompute() as we want the tsan calls to stick around

  // we don't need the management stuff, we directly replace our program with
  // precomputed one
  auto precompute_main = precalcuation->get_precompute_main();
  auto it = precompute_main->begin()->begin();
  ++it; // second instruction is call to precomputed main
  auto *call = cast<CallBase>(it);
  auto precomputed_main = call->getCalledFunction();

  // remove old main
  auto orig_linkeage = main_func->getLinkage();
  main_func->deleteBody(); // will set linkeage to weak, which we dont want
  main_func->setLinkage(orig_linkeage);

  // create new function body
  auto *bb = BasicBlock::Create(M.getContext(), "entry", main_func);
  // add call to precompute
  IRBuilder<> builder = IRBuilder<>(bb);
  // forward args of main
  std::vector<Value *> args;
  for (auto &arg : main_func->args()) {
    args.push_back(&arg);
  }

  builder.CreateCall(precomputed_main, args);
  builder.CreateRet(Constant::getNullValue(main_func->getReturnType()));

  remove_noinline_from_module(M);

  // remove other non-precompute functions now
  std::vector<Function *> to_delete;
  for (auto it_f = M.begin(); it_f != M.end(); ++it_f) {
    Function *f = &*it_f;
    if (precalcuation->is_func_part_of_precompute_phase(f)) {
      // the tsan calls are already part of precompute, no need to instrumente
      // them again
      f->removeFnAttr(Attribute::SanitizeThread);
    } else if ((not f->isDeclaration()) && f != main_func &&
               (not f->getName().starts_with("__tsan")) &&
               (not is_func_from_std(f))) {
      // not used: remove
      if (f->hasExternalLinkage())
        f->setLinkage(GlobalValue::InternalLinkage);
      // this will prompt GlobalDCE to remove
    }
  }

  return "Successfully computed the precomputation";
}

namespace {
struct SanitizerPrecomputePass : public PassInfoMixin<SanitizerPrecomputePass> {

  // register that we require this analysis

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<ModuleSummaryIndexWrapperPass>();
    AU.addRequiredTransitive<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }

  StringRef getPassName() const { return "sanitizer-precompute"; }

  // Pass starts here
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    errs() << "\n";
    run_optimization_passes(M, AM, run_tsan, false);

#ifndef NDEBUG
    auto num_undef = get_num_undefs(M);
#endif
    if (not run_optimization_passes(M, AM, run_precompute))
      return PreservedAnalyses::all();
#ifndef NDEBUG
    // at most: every undef value can be duplicated
    // TODO re-enable assertions for no openmp programs
    // assert(get_num_undefs(M) <= num_undef * 2);
    // but this is probably insecure (e.g. if undef is used to calculate the
    // tag)// so we go with the stricter assertion that our pass should not use
    // more undef values
    // assert(get_num_undefs(M) <= num_undef);
    // some undefs are actually duplicated in our test programm (some vector
    // elems are undef)
#endif

#ifdef PRECOMPUTE_TSAN_OPTIMIZE_LOOPS
    run_optimization_passes(M, AM, Optimize_loops);
    run_optimization_passes(M, AM, eliminate_single_thread);
    run_optimization_passes(M, AM, reduce_tsan_calls);
#endif

    delete analysis_results;
    return PreservedAnalyses::none();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "sanitizer_precompute", "1.0.0",
          [](PassBuilder &PB) {
            PB.registerOptimizerEarlyEPCallback([&](ModulePassManager &MPM,
                                                    OptimizationLevel Level,
                                                    ThinOrFullLTOPhase Phase) {
              MPM.addPass(SanitizerPrecomputePass());
            });
          }};
}
