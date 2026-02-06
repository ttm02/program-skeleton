#pragma once

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "io.hpp"
#include "logging.hpp"
#include "timer.hpp"
#include "utils.hpp"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h" // inlining

#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/Scalar/DCE.h" // Dead code eliminiation pass

// Includes for precomputation compiler libs
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Precompute_insertion.h"
#include "analysis_results.h"
#include "debug.h"
#include "openmp_runtime_functions.h"
#include "precalculation.h"
#include "precompute_backend_funcs.h"
#include "std_funcs.h"
// Includes for precomputation compiler libs
#include "arguments.hpp"

using namespace llvm;

RequiredAnalysisResults *analysis_results;

// removes attribute noinline from every func
// we previously set it to make analysis easier
void remove_noinline_from_module(llvm::Module &M) {
  for (auto &F : M) {
    if (F.hasFnAttribute(llvm::Attribute::NoInline) and
        not F.hasFnAttribute(llvm::Attribute::OptimizeNone)) {
      F.removeFnAttr(llvm::Attribute::NoInline);
    }
  }
}

void run_optimization_passes(llvm::Module &M, ModuleAnalysisManager &MAM) {
  // Clear all cached analyses, this seems to be needed for some cases
  // to recompute all the analyses as they were done on the original module
  // and may cause deleted memory references or an invalid call graph traversal
  // which crashes the pass.
  MAM.clear();

  llvm::errs() << "[AllocTrackerLTOPass] Run inliner Pass\n";
  llvm::ModuleInlinerPass().run(M, MAM);
  llvm::errs() << "[AllocTrackerLTOPass] Finished inliner Pass\n";

  llvm::errs()
      << "[AllocTrackerLTOPass] Run (global) Dead Code Elimination Pass\n";
  // Removes unused functions
  //  i.e. functions from original module that are now replaced with slice
  //  functions
  llvm::GlobalDCEPass().run(M, MAM);
  llvm::errs() << "[AllocTrackerLTOPass] Finished Dead Code Elimination Pass\n";
}

namespace {

/**
 * This struct is used in the instrumnentation functions to instrument different
 * allocation calls. Since these functions have different types and number of
 * arguments this config stores the indices to the important arguments and
 * allows a cleaner implementation without code duplication.
 */
struct AllocationInstrumentationConfig {
  std::string log_function_name;
  std::vector<unsigned int> arg_indices;
};

struct AllocTrackerLTOPass : public PassInfoMixin<AllocTrackerLTOPass> {
private:
  struct arguments arguments_ = {};

  std::shared_ptr<PrecalculationAnalysis> precalc_analysis_;
  std::shared_ptr<PrecomputeInsertion> precalculation_;
  std::vector<llvm::Instruction *> problematic_calls_;

  // std::unique_ptr<llvm::raw_fd_ostream> log_output_stream;
  std::vector<llvm::Value *> problematic_calls_in_slice_;

  bool sliced_ = false;
  bool input_program_uses_mpi_ = false;

public:
  // main entry point
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  bool run_on_module(Module &M, ModuleAnalysisManager &MAM);

  // Instrumentation and Logging
  bool instrument_MPI_Init_and_MPI_Finalize_if_needed(Module &M,
                                                      LLVMContext &Ctx);
  bool
  instrument_allocation_calls(Module &M, LLVMContext &Ctx,
                              const std::vector<CallInst *> &alloc_calls,
                              const AllocationInstrumentationConfig &config);
  bool instrument_start_and_end_of_program(Module &M);
  bool
  instrument_allocations_with_logging_functions(Module &M,
                                                ModuleAnalysisManager &MAM);

  // Slicing
  CallInst *get_mpi_init_call(Module &M);
  CallInst *get_mpi_finalize_call(Module &M);
  void add_required_mpi_function_to_slicing_criterion(
      std::vector<Value *> &to_precompute,
      std::vector<Instruction *> &precompute_locations, CallInst *call_I,
      llvm::StringRef called_F_name);
  void set_problematic_call_args_to(int new_arg_value);
  bool slice_module(Module &M, ModuleAnalysisManager &MAM);

  // Inlining
  bool is_allocation_wrapper(llvm::StringRef name) const;
  bool inline_allocation_wrapper_calls(Module &M);
  bool inline_allocation_wrapper_call(Module &M, CallInst *call_I);

  void check_and_print_pass_options() const {
    llvm::errs()
        << "[AllocTrackerLTOPass] Running pass with the following options: \n";
    if (arguments_.EnableSlicing)
      llvm::errs() << "\t\tslicing enabled\n";
    else
      llvm::errs() << "\t\tslicing NOT enabled\n";

    if (arguments_.EnableLogging)
      llvm::errs() << "\t\tlogging enabled\n";
    else
      llvm::errs() << "\t\tlogging NOT enabled\n";

    // Handling MPI Communication
    if (arguments_.IgnoreMPICommunication && AddAllMPICommunication) {
      llvm::report_fatal_error("Cannot specify both IgnoreMPICommunication "
                               "and AddAllMPICommunication!");
    }

    if (arguments_.IgnoreMPICommunication)
      llvm::errs() << "\t\tignoring MPI communication\n";
    if (arguments_.AddAllMPICommunication)
      llvm::errs() << "\t\tadding all MPI communication\n";
  }

  static bool isRequired() { return true; }
  static StringRef name() { return "AllocTrackerLTOPass"; }

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<ModuleSummaryIndexWrapperPass>();
    AU.addRequiredTransitive<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }
};

} // namespace

//-----------------------------------------------------------------------------
// New Pass Manager Registration
//-----------------------------------------------------------------------------
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "AllocTrackerLTOPass", "1.0.0",
          [](PassBuilder &PB) {
            // Note on when pass is run during optimization pipeline: at the
            // start of LTO pipeline
            // https://llvm.org/doxygen/classllvm_1_1PassBuilder.html#abca5690a1a0abc98824d0f15ce3f4a93
            // early vs last:
            //  - registerFullLinkTimeOptimizationEarlyEPCallback: pass should
            //  run before other optimizations and thus has a clear view on the
            //  original code
            //  - registerFullLinkTimeOptimizationLastEPCallback: pass is run
            //  after other optimizations and thus views a potentially highly
            //  transformed code due to different optimizations
            PB.registerOptimizerEarlyEPCallback(
                [&](ModulePassManager &MPM, auto, auto) {
                  MPM.addPass(AllocTrackerLTOPass());
                });

            // needed only for manual invokation of pass by name (within the opt
            // command)? i.e. for testing/debugging without optimizations
            // applied etc.
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "alloc-tracker-lto-pass") {
                    MPM.addPass(AllocTrackerLTOPass());
                    return true;
                  }
                  return false;
                });
          }};
};
