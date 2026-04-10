
#include "tsan_slicing_cleanup.h"

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
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/IPO/ModuleInliner.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Instrumentation/ThreadSanitizer.h"
#include "llvm/Transforms/Scalar/EarlyCSE.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <memory>

using namespace llvm;

RequiredAnalysisResults *analysis_results;
std::shared_ptr<PrecalculationAnalysis> precalculation_analysis;

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

void run_cleanup(Module &M, ModuleAnalysisManager &AM) {
  errs() << "Run Cleanup\n";
  llvm::ModulePassManager MPM;
  llvm::FunctionPassManager FPM;
  // FPM.addPass(llvm::EarlyCSEPass());
  // FPM.addPass(llvm::InstCombinePass());
  FPM.addPass(llvm::SimplifyCFGPass());
  MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.addPass(llvm::GlobalDCEPass());
  MPM.addPass(llvm::ModuleInlinerPass());
  MPM.run(M, AM);
}

static bool run_optimization_passes(
    Module &M, ModuleAnalysisManager &AM,
    std::string (*opt_pass_func)(Module &M, ModuleAnalysisManager &AM),
    bool cleanup = true) {
  std::string opt_msg_success = opt_pass_func(M, AM);
  if (opt_msg_success.empty()) {
    errs() << "aborted ..\n\n";
    return false;
  }
#ifndef NDEBUG
  bool has_error = verifyModule(M, &errs(), nullptr);
  assert(not has_error);
#endif
  errs() << opt_msg_success << "\n";

  if (cleanup) {
    run_cleanup(M, AM);
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
      // no need to instrument again after each step
      f->removeFnAttr(Attribute::SanitizeThread);
    }
    return "Successfully instrumented code with TSAN";
  }
  return "TSAN instrumentation already available";
}

static void collectCalls(CallBase *call, DenseSet<Value *> &to_precompute,
                         DenseSet<Instruction *> &precompute_locations) {
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
      // skip, the tsan cleanup part. no need to precompute (slice),
      // as it will only handle fatal exceptions
      return;
    }
  }

  for (auto &it_arg : call->args())
    to_precompute.insert(it_arg);
  precompute_locations.insert(call);
}

static void
collectForPrecompute(Instruction &inst, DenseSet<Value *> &to_precompute,
                     DenseSet<Instruction *> &precompute_locations) {
  if (auto *call = dyn_cast<CallBase>(&inst)) {
    collectCalls(call, to_precompute, precompute_locations);
  } else {
    // TODO do we need other instructions precomputed?
  }
}

static void reset_analysis_results(Module &M, ModuleAnalysisManager &AM) {
  static bool alreadySetup = false;
  if (alreadySetup)
    return;

  analysis_results = new RequiredAnalysisResults(AM, M);

  auto *main_func = M.getFunction("main");
  assert(main_func);

  DenseSet<Value *> to_precompute;
  DenseSet<Instruction *> precompute_locations;

  for (Function &func : M) {
    // do not instrument tsan itself
    if (func.getName().starts_with("tsan.module_ctor"))
      continue;

    for (BasicBlock &bb : func)
      for (Instruction &inst : bb)
        collectForPrecompute(inst, to_precompute, precompute_locations);
  }

  errs() << "Statistics: locations: " << precompute_locations.size()
         << " values: " << to_precompute.size() << "\n";
  // DRB083 does not like that
  // assert(not precompute_locations.empty());

  precalculation_analysis = std::make_shared<PrecalculationAnalysis>(
      M, main_func, to_precompute, precompute_locations);

  alreadySetup = true;
}

static std::string perform_slicing(Module &M, ModuleAnalysisManager &AM) {
  if (precalculation_analysis->get_locations_to_precompute().empty())
    return "";

  auto *main_func = M.getFunction("main");
  assert(main_func);

  PrecomputeFunctions::create_instance(M);
  auto precalcuation =
      std::make_shared<PrecomputeInsertion>(M, precalculation_analysis, false);

  // do NOT call clean_precompute() as we want the tsan calls to stick around

  // we don't need the management stuff, we directly replace our program with
  // precomputed one
  auto precompute_main = precalcuation->get_precompute_main();
  auto it = precompute_main->begin()->begin();
  ++it; // second instruction is call to precomputed main
  auto *call = cast<CallBase>(it);
  auto *precomputed_main = call->getCalledFunction();

  // remove old main
  auto orig_linkeage = main_func->getLinkage();
  main_func->deleteBody(); // will set linkeage to weak, which we dont want
  main_func->setLinkage(orig_linkeage);

  // create new function body
  auto *bb = BasicBlock::Create(M.getContext(), "entry", main_func);
  IRBuilder<> builder = IRBuilder<>(bb);
  // forward args of main
  std::vector<Value *> args;
  for (auto &arg : main_func->args()) {
    args.push_back(&arg);
  }

  builder.CreateCall(precomputed_main, args);
  builder.CreateRet(Constant::getNullValue(main_func->getReturnType()));

  // remove other non-precompute functions now
  std::vector<Function *> to_delete;
  for (Function &func : M) {
    if ((not func.isDeclaration()) && &func != main_func &&
        (not func.getName().starts_with("__tsan")) &&
        (not is_func_from_std(&func))) {
      // not used: remove
      if (func.hasExternalLinkage())
        func.setLinkage(GlobalValue::InternalLinkage);
      // this will prompt GlobalDCE to remove
    }
  }

  remove_noinline_from_module(M);
  analysis_results->invalidate(*precomputed_main);
  analysis_results->invalidate(*main_func);

  llvm::ModulePassManager MPM;
  // remove old function duplicates
  MPM.addPass(
      InternalizePass([&](const GlobalValue &GV) { return &GV == main_func; }));
  MPM.run(M, AM);

  return "Successfully computed the precomputation";
}

static cl::opt<bool> DisableSlicing("disable-slicing",
                                    cl::desc("Disable slicing"),
                                    cl::init(false));
static cl::opt<bool>
    EnableStaticAnalysis("enable-static-analysis",
                         cl::desc("Enable static analysis to help slicing"),
                         cl::init(false));
enum stanMode { SINGLE, MERGE, LOOP, MAX_COUNT };
static cl::list<stanMode> clStanModes(
    "static-analysis-mode",
    cl::desc("Configure combinations of static analysis passes"),
    cl::CommaSeparated,
    cl::values(clEnumValN(SINGLE, "single",
                          "removal of possible single-threaded regions"),
               clEnumValN(MERGE, "merge",
                          "merge contigous memory regions into one TSAN call"),
               clEnumValN(LOOP, "loop",
                          "perform a kind of loop unrolling with TSAN calls")));

namespace {
struct TSANSlicingPass : public PassInfoMixin<TSANSlicingPass> {

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<ModuleSummaryIndexWrapperPass>();
    AU.addRequiredTransitive<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }

  StringRef getPassName() const { return "tsan-slicing"; }

  // Pass starts here
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    PassBuilder PB;
    auto MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
    errs() << "\n";

    run_optimization_passes(M, AM, run_tsan, false);
    allow_function_prefixes_to_be_called_in_precompute({"__tsan_"});

    auto run_slicing = [&]() {
      reset_analysis_results(M, AM);
#ifndef NDEBUG
      auto num_undef = get_num_undefs(M);
#endif
      if (not run_optimization_passes(M, AM, perform_slicing))
        return false;
#ifndef NDEBUG
      // at most: every undef value can be duplicated but this is probably
      // insecure (e.g. if undef is used to calculate the tag) so we go with the
      // stricter assertion that our pass should not use more undef values some
      // undefs are actually duplicated in our test programm (some vector elems
      // are undef)
      double max_undef_factor = 1.0; // between 1.0 and 2.0
      assert(get_num_undefs(M) <= num_undef * max_undef_factor);
#endif
      return true;
    };

    DenseSet<stanMode> stanEnabledModes;
    if (EnableStaticAnalysis || not clStanModes.empty()) {
      reset_analysis_results(M, AM);
      if (clStanModes.empty())
        for (int i = 0; i < static_cast<int>(stanMode::MAX_COUNT); ++i)
          stanEnabledModes.insert(static_cast<stanMode>(i));

      for (auto clsm : clStanModes)
        stanEnabledModes.insert(clsm);
    }

    // slicing or no slicing?
    if (not DisableSlicing) {
      if (not run_slicing())
        return PreservedAnalyses::all();
    } else {
      remove_noinline_from_module(M);
      run_cleanup(M, AM);
#ifndef NDEBUG
      bool has_error = verifyModule(M, &errs(), nullptr);
      assert(not has_error);
#endif
    }

    // static analysis after slicing
    // running this before slicing might not allow certain optimizations
    // and performance for e.g. HPCCG is a lot worse
    {
      if (stanEnabledModes.contains(SINGLE)) {
        run_optimization_passes(M, AM, remove_all_single_thread_regions);
        run_optimization_passes(M, AM, wrap_non_openmp_tsan_calls);
        run_optimization_passes(M, AM, eliminate_only_in_critical);
      }
      if (stanEnabledModes.contains(MERGE))
        run_optimization_passes(M, AM, combine_tsan_calls, false);
      if (stanEnabledModes.contains(LOOP))
        run_optimization_passes(M, AM, optimize_loops);
    }

    // try to eliminate even more things
    errs() << "Run O2 Passes\n";
    MPM.run(M, AM);

    delete analysis_results;
    return PreservedAnalyses::none();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "tsan_slicing", "1.0.0", [](PassBuilder &PB) {
        PB.registerOptimizerEarlyEPCallback(
            [&](ModulePassManager &MPM, OptimizationLevel Level,
                ThinOrFullLTOPhase Phase) { MPM.addPass(TSANSlicingPass()); });
      }};
}
