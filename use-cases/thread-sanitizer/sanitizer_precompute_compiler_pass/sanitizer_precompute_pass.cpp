
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

static void reset_analysis_results(Module &M, ModuleAnalysisManager &AM) {
  analysis_results = new RequiredAnalysisResults(AM, M);
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
  assert(not has_error);
#endif
  errs() << opt_msg_success << "\n";

  if (cleanup) {
    errs() << "Run inliner Pass\n";
    auto inliner = llvm::ModuleInlinerPass();
    inliner.run(M, AM);

    errs() << "Run Global DCE Pass\n";
    auto dce = llvm::GlobalDCEPass();
    dce.run(M, AM);
#ifndef NDEBUG
    has_error = verifyModule(M, &errs(), nullptr);
    assert(not has_error);
    // assert(not has_poisoned_values(M));
#endif
  }
  errs() << "\n";
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
      not(getCallName(cast<CallInst>(it)).value() == "__tsan_func_exit")) {
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

static void collectCalls(CallBase *call, std::vector<Value *> &to_precompute,
                         std::vector<Instruction *> &precompute_locations) {
  if (is_func_from_std(call->getFunction())) {
    // dont analyze internals of std, though tsan may instrument them
    return;
  }

  auto called_func = call->getCalledFunction();
  if (!called_func)
    return;
  auto func_name = called_func->getName();

  // eiter tsan or omp function
  // omp function necessary e.g. to keep synchronization
  bool is_thread_func = is_thread_function(called_func);
  if (not func_name.starts_with("__tsan") && not is_thread_func)
    return;

  if (func_name == "__tsan_func_exit") {
    if (isa<ResumeInst>(call->getNextNonDebugInstruction())) {
      // TODO make this a compiler option!!
      // in cleanup block, dont handle exceptions
      return;
    }
    if (is_tsan_cleanup_block(call->getParent())) {
      // skip, the tsan cleanup part. no need to precompute, as it will only
      // handle fatal exceptions
      return;
    }
  }

  for (auto &it_arg : call->args())
    to_precompute.push_back(it_arg);
  precompute_locations.push_back(call);
}

static void
collectPrecompute(Instruction &inst, std::vector<Value *> &to_precompute,
                  std::vector<Instruction *> &precompute_locations) {
  if (auto *call = dyn_cast<CallBase>(&inst)) {
    collectCalls(call, to_precompute, precompute_locations);
  } else {
    // TODO do we need other instructions precomputed?
  }
}

static std::string run_precompute(Module &M, ModuleAnalysisManager &AM) {
  PrecomputeFunctions::create_instance(M);

  auto *main_func = M.getFunction("main");
  assert(main_func);

  std::vector<Value *> to_precompute;
  std::vector<Instruction *> precompute_locations;

  for (Function &func : M) {
    // do not instrument tsan itself
    if (func.getName().starts_with("tsan.module_ctor"))
      continue;

    for (BasicBlock &bb : func)
      for (Instruction &inst : bb)
        collectPrecompute(inst, to_precompute, precompute_locations);
  }

  errs() << "Statistics: locations: " << precompute_locations.size()
         << " values: " << to_precompute.size() << "\n";

  // no tsan found
  if (precompute_locations.empty()) {
    // no modification
    return "";
  }

  std::vector<Instruction *> problematic;
  auto precalcuation = std::make_shared<PrecomputeInsertion>(
      M,
      std::make_shared<PrecalculationAnalysis>(M, main_func, to_precompute,
                                               precompute_locations,
                                               problematic, false, false),
      false, false);

  // do NOT call clean_precompute() as we want the tsan calls to stick around

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

  auto *precomputed_main = precalcuation->get_precompute_main();
  builder.CreateCall(precomputed_main, args);
  builder.CreateRet(Constant::getNullValue(main_func->getReturnType()));

  std::vector<Function *> to_delete;
  for (Function &func : M) {
    if (precalcuation->is_func_part_of_precompute_phase(&func)) {
      // the tsan calls are already part of precompute, no need to instrumente
      // them again
      func.removeFnAttr(Attribute::SanitizeThread);
    }
  }

  remove_noinline_from_module(M);
  analysis_results->invalidate(*precomputed_main);
  analysis_results->invalidate(*main_func);

  return "Successfully computed the precomputation";
}

static cl::opt<bool>
    EnableStaticAnalysis("enable-static-analysis",
                         cl::desc("Enable static analysis to help precompute"),
                         cl::init(false));

namespace {
struct SanitizerPrecomputePass : public PassInfoMixin<SanitizerPrecomputePass> {

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
    PassBuilder PB;
    auto MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
    errs() << "\n";

    run_optimization_passes(M, AM, run_tsan, false);

    allow_function_prefixes_to_be_called_in_precompute({"__tsan_"});
    reset_analysis_results(M, AM);

    // static analysis before slicing
    if (EnableStaticAnalysis) {
      run_optimization_passes(M, AM, combine_tsan_calls, false);
      run_optimization_passes(M, AM, wrap_non_openmp_tsan_calls);
    }
#ifndef NDEBUG
    auto num_undef = get_num_undefs(M);
#endif
    if (not run_optimization_passes(M, AM, run_precompute))
      return PreservedAnalyses::all();
#ifndef NDEBUG
    // at most: every undef value can be duplicated but this is probably
    // insecure (e.g. if undef is used to calculate the tag) so we go with the
    // stricter assertion that our pass should not use more undef values some
    // undefs are actually duplicated in our test programm (some vector elems
    // are undef)
    double max_undef_factor = 1.0; // between 1.0 and 2.0
    assert(get_num_undefs(M) <= num_undef * max_undef_factor);
#endif
    // static analysis after slicing
    if (EnableStaticAnalysis) {
      run_optimization_passes(M, AM, combine_tsan_calls, false);
      // HPCCG does not detect data race when this runs before precompute
      run_optimization_passes(M, AM, remove_all_single_thread_regions);
      // needs single threaded removal + needs analysis_results
      run_optimization_passes(M, AM, eliminate_only_in_critical);
      // precomputation does not allow int2ptr casts
      run_optimization_passes(M, AM, optimize_loops);
    }

    // try to eliminate even more things
    MPM.run(M, AM);

    delete analysis_results;
    return PreservedAnalyses::none();
  }
};

} // namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerOptimizerEarlyEPCallback(
        [&](ModulePassManager &MPM, auto, auto) {
          MPM.addPass(SanitizerPrecomputePass());
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "sanitizer-precompute", "1.0.0", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
