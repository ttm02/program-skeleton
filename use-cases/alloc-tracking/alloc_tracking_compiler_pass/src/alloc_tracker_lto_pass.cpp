#include "alloc_tracker_lto_pass.hpp"

bool AllocTrackerLTOPass::instrument_allocation_calls(
    Module &M, LLVMContext &Ctx, const std::vector<CallInst *> &alloc_calls,
    const AllocationInstrumentationConfig &config) {
  bool modified = false;

  for (CallInst *call_I : alloc_calls) {
    IRBuilder<> ir_builder(call_I);

    // Get Function that contains call_I
    Function *call_I_F = call_I->getFunction();

    // Retrieving arguments for the logging function
    Function *log_F = M.getFunction(config.log_function_name);

    // Retrieve important arguments dependent on which allocation function is
    // passed i.e. for malloc retrieve size argument, for calloc retrieve size
    // and num argument etc.
    std::vector<Value *> processed_args;
    std::vector<Value *> constant_flags;

    for (int arg_idx : config.arg_indices) {
      Value *arg_V = call_I->getArgOperand(arg_idx);

      // Note: normal allocation calls use i64 type in LLVM IR for the size
      // parameter (i.e. size_t in the C program)
      //       if someone uses wrapper functions that pass the size as i32 (i.e.
      //       int in the C program) the logging function will have a type
      //       mismatch and IR verification will fail.
      //       -> therefore the type is converted to i64 here
      if (arg_V->getType()->isIntegerTy(32)) {
        arg_V = ir_builder.CreateZExt(arg_V, Type::getInt64Ty(M.getContext()));
      }

      processed_args.push_back(arg_V);
      constant_flags.push_back(ir_builder.getInt1(isa<ConstantInt>(arg_V)));
    }

    // Get name of parent function
    Value *parent_func_name_V = ir_builder.CreateBitCast(
        logging::get_parent_function_name_V(M, Ctx, *call_I_F),
        PointerType::get(Ctx, 0));

    // Check if the allocation call is invalid
    //  i.e. only possible if precalc analysis ignored problematic MPI
    //  communication and marked call as to skip
    Value *valid_V = nullptr;
    auto skip_it = std::find(problematic_calls_in_slice_.begin(),
                             problematic_calls_in_slice_.end(), call_I);
    if (skip_it != problematic_calls_in_slice_.end()) {
      valid_V = ConstantInt::get(Type::getInt1Ty(Ctx), 0);
    } else {
      valid_V = ConstantInt::get(Type::getInt1Ty(Ctx), 1);
    }

    // Check whether the allocation call return value is needed afterwards
    Value *required_to_allocate_V = nullptr;
    if (logging::has_call_I_further_uses(call_I)) {
      required_to_allocate_V = ConstantInt::get(Type::getInt1Ty(Ctx), 1);
    } else {
      required_to_allocate_V = ConstantInt::get(Type::getInt1Ty(Ctx), 0);
    }

    // Create arguments vector for logging function
    std::vector<Value *> args;
    for (auto *arg : processed_args)
      args.push_back(arg);
    for (auto *flag : constant_flags)
      args.push_back(flag);
    args.push_back(parent_func_name_V);
    args.push_back(valid_V);
    args.push_back(required_to_allocate_V);

    // Create call to runtime logging function
    ir_builder.CreateCall(log_F, args);

    // Remove original allocation call if there are no further uses
    // only keep it if the allocation is actually required to maintain
    // correctness.
    if (!logging::has_call_I_further_uses(call_I))
      call_I->eraseFromParent();

    modified = true;
  }

  return modified;
}

bool AllocTrackerLTOPass::instrument_MPI_Init_and_MPI_Finalize_if_needed(
    Module &M, LLVMContext &Ctx) {
  bool modified = false;

  CallInst *mpi_init_CI = get_mpi_init_call(M);
  CallInst *mpi_finalize_CI = get_mpi_finalize_call(M);
  if (mpi_init_CI && mpi_finalize_CI) {
    // Declare instrumentation functions
    logging::declare_function(
        M, Ctx, "alloc_tracker_mpi_init_called",
        FunctionType::get(Type::getVoidTy(Ctx), {}, false));
    logging::declare_function(
        M, Ctx, "alloc_tracker_mpi_finalize_called",
        FunctionType::get(Type::getVoidTy(Ctx), {}, false));

    // Instrument both calls with respective instrumentation function from
    // runtime library
    Function *init_instrumentation_F =
        M.getFunction("alloc_tracker_mpi_init_called");
    Function *finalize_instrumentation_F =
        M.getFunction("alloc_tracker_mpi_finalize_called");

    IRBuilder<> builder1(mpi_init_CI->getNextNode());
    builder1.CreateCall(init_instrumentation_F, {});

    IRBuilder<> builder2(mpi_finalize_CI);
    builder2.CreateCall(finalize_instrumentation_F, {});

    llvm::errs() << "[AllocTrackerLTOPass::instrument_allocations_with_logging_"
                    "functions] Added instrumentation for MPI_Init() and "
                    "MPI_Finalize().\n";

    modified = true;
  } else {
    assert((!mpi_init_CI && !mpi_finalize_CI) &&
           "The program should not contain any MPI calls.");
  }

  return modified;
}

bool AllocTrackerLTOPass::instrument_start_and_end_of_program(Module &M) {
  Function *main_F = M.getFunction("main");
  if (!main_F) {
    llvm::errs() << "[AllocTrackerLTOPass] Could not find main function.\n";
    return false;
  }

  // Get IR builder directly as first instruction in main
  BasicBlock &entry_BB = main_F->getEntryBlock();
  IRBuilder<> builder_start(&entry_BB, entry_BB.begin());

  logging::instrument_start_of_program(M, builder_start);

  // Get IR builder directly before each return instruction in main
  for (auto &I : llvm::instructions(main_F)) {
    if (auto *ret_I = llvm::dyn_cast<ReturnInst>(&I)) {
      IRBuilder<> builder_end(ret_I);

      logging::instrument_end_of_program(M, builder_end);
    }
  }

  return true;
}

bool AllocTrackerLTOPass::instrument_allocations_with_logging_functions(
    Module &M, ModuleAnalysisManager &MAM) {
  bool modified = false;

  LLVMContext &Ctx = M.getContext();
  logging::declare_logging_functions(M, Ctx);

  // Instrument start and end of program with logging functions (but only if not
  // already done in slice_module)
  if (!sliced_) {
    if (instrument_start_and_end_of_program(M))
      modified = true;
  }

  // Check whether MPI_Init and MPI_Finalize exist and then add instrumentation
  // for logging library
  if (instrument_MPI_Init_and_MPI_Finalize_if_needed(M, Ctx))
    modified = true;

  std::vector<CallInst *> malloc_calls;
  std::vector<CallInst *> calloc_calls;
  std::vector<CallInst *> realloc_calls;
  std::vector<CallInst *> aligned_alloc_calls;

  // Traverse complete module, collect dynamic allocation functions
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    for (auto &I : llvm::instructions(F)) {
      if (auto *call_I = llvm::dyn_cast<CallInst>(&I)) {

        if (auto *called_F = call_I->getCalledFunction()) {
          if (called_F->isIntrinsic())
            continue;

          llvm::StringRef called_F_name = called_F->getName();

          if (called_F_name == "malloc")
            malloc_calls.push_back(call_I);
          else if (called_F_name == "calloc")
            calloc_calls.push_back(call_I);
          else if (called_F_name == "realloc")
            realloc_calls.push_back(call_I);
          else if (called_F_name == "aligned_alloc")
            aligned_alloc_calls.push_back(call_I);
        }
      }
    }
  }

  unsigned int fst_arg_idx = 0;
  unsigned int snd_arg_idx = 1;

  // Configuration for malloc
  // signature: void *malloc( size_t size );
  // interested in first argument (size, index 0)
  AllocationInstrumentationConfig malloc_config = {
      "alloc_tracker_log_malloc_call", {fst_arg_idx}};

  // Configuration for calloc
  // signature: void* calloc( size_t num, size_t size );
  // interested in first two arguments (num and size, index 0 and 1)
  AllocationInstrumentationConfig calloc_config = {
      "alloc_tracker_log_calloc_call", {fst_arg_idx, snd_arg_idx}};

  // Configuration for realloc
  // signature: void *realloc( void *ptr, size_t new_size );
  // interested only in second argument (size, index 1)
  AllocationInstrumentationConfig realloc_config = {
      "alloc_tracker_log_realloc_call", {snd_arg_idx}};

  // Configuration for aligned_alloc
  // signature: void *aligned_alloc( size_t alignment, size_t size );
  // interested only in second argument (size, index 1)
  AllocationInstrumentationConfig aligned_alloc_config = {
      "alloc_tracker_log_aligned_alloc_call", {snd_arg_idx}};

  if (instrument_allocation_calls(M, Ctx, malloc_calls, malloc_config))
    modified = true;
  if (instrument_allocation_calls(M, Ctx, calloc_calls, calloc_config))
    modified = true;
  if (instrument_allocation_calls(M, Ctx, realloc_calls, realloc_config))
    modified = true;
  if (instrument_allocation_calls(M, Ctx, aligned_alloc_calls,
                                  aligned_alloc_config))
    modified = true;

  llvm::errs() << "[AllocTrackerLTOPass::instrument_allocations_with_logging_"
                  "functions] Finished instrumenting with logging functions.\n";

  // Verify module
  if (!utils::verify_module(M))
    throw std::runtime_error(
        "[AllocTrackerLTOPass::instrument_allocations_with_logging_functions] "
        "ERROR: Module could not be verfied!");

  // Save instrumented module dump to file
  io::write_module_dump_to_file(M, "instrumented_module_dump.ll");

  return modified;
}

CallInst *AllocTrackerLTOPass::get_mpi_init_call(Module &M) {
  Function *mpi_init_F = M.getFunction("MPI_Init");

  if (mpi_init_F) {
    for (User *U : mpi_init_F->users()) {
      if (auto *CI = llvm::dyn_cast<CallInst>(U)) {
        if (CI->getCalledFunction() == mpi_init_F) {
          return CI;
        }
      }
    }
  }

  return nullptr;
}

CallInst *AllocTrackerLTOPass::get_mpi_finalize_call(Module &M) {
  Function *mpi_finalize_F = M.getFunction("MPI_Finalize");

  if (mpi_finalize_F) {
    for (User *U : mpi_finalize_F->users()) {
      if (auto *CI = llvm::dyn_cast<CallInst>(U)) {
        if (CI->getCalledFunction() == mpi_finalize_F) {
          return CI;
        }
      }
    }
  }

  return nullptr;
}

void AllocTrackerLTOPass::add_required_mpi_function_to_slicing_criterion(
    std::vector<Value *> &to_precompute,
    std::vector<Instruction *> &precompute_locations, CallInst *call_I,
    llvm::StringRef called_F_name) {
  // Note: Currently, it is only tested to add MPI_{Send, Recv, Bcast} to the
  // slicing criterion.
  //      This can be modified here to also include other MPI functions.
  //      However, these must be then also handled in the precompute library (in
  //      precalculations.cpp) in the function visit_call_from_ptr().
  std::vector<llvm::StringRef> required_mpi_functions;

  if (arguments_.AddAllMPICommunication) {
    // In this case we want to add all the MPI_{Send, Recv, Bcast} calls to the
    // slice
    required_mpi_functions.push_back("MPI_Send");
    required_mpi_functions.push_back("MPI_Recv");

    required_mpi_functions.push_back("MPI_Bcast");
  }

  bool found =
      std::find(required_mpi_functions.begin(), required_mpi_functions.end(),
                called_F_name) != required_mpi_functions.end();

  if (found) {
    // Simply add all the arguments to the slicing criterion
    for (auto &arg : call_I->args()) {
      to_precompute.push_back(arg.get());
    }

    precompute_locations.push_back(call_I);
  }
}

void AllocTrackerLTOPass::set_problematic_call_args_to(int new_arg_value) {
  llvm::errs()
      << "[AllocTrackerLTOPass] Setting the arguments of the problematic "
         "allocation calls found during slicing analysis to "
      << new_arg_value << "\n";

  for (auto *I : problematic_calls_in_slice_) {
    if (auto *call_I = llvm::dyn_cast<CallInst>(I)) {
      if (auto *called_F = call_I->getCalledFunction()) {
        llvm::StringRef called_F_name = called_F->getName();

        if (called_F_name == "malloc") {
          Value *malloc_size_V =
              call_I->getArgOperand(0); // size is first argument

          auto *zero_size_CI =
              llvm::ConstantInt::get(malloc_size_V->getType(), new_arg_value);

          call_I->setArgOperand(0, zero_size_CI);
        } else if (called_F_name == "calloc") {
          Value *calloc_num_V =
              call_I->getArgOperand(0); // num is first argument
          Value *calloc_size_V =
              call_I->getArgOperand(1); // size is second argument

          auto *zero_num_CI =
              llvm::ConstantInt::get(calloc_num_V->getType(), new_arg_value);
          auto *zero_size_CI =
              llvm::ConstantInt::get(calloc_size_V->getType(), new_arg_value);

          call_I->setArgOperand(0, zero_num_CI);
          call_I->setArgOperand(1, zero_size_CI);
        } else if (called_F_name == "realloc") {
          Value *realloc_size_V =
              call_I->getArgOperand(1); // new size is second argument

          auto *zero_size_CI =
              llvm::ConstantInt::get(realloc_size_V->getType(), new_arg_value);

          call_I->setArgOperand(1, zero_size_CI);
        } else if (called_F_name == "aligned_alloc") {
          Value *aligned_alloc_alignment_V =
              call_I->getArgOperand(0); // alignment is first argument
          Value *aligned_alloc_size_V =
              call_I->getArgOperand(1); // size is second argument

          auto *zero_alignment_CI = llvm::ConstantInt::get(
              aligned_alloc_alignment_V->getType(), new_arg_value);
          auto *zero_size_CI = llvm::ConstantInt::get(
              aligned_alloc_size_V->getType(), new_arg_value);

          call_I->setArgOperand(0, zero_alignment_CI);
          call_I->setArgOperand(1, zero_size_CI);
        }
      }
    }
  }
}

bool AllocTrackerLTOPass::slice_module(Module &M, ModuleAnalysisManager &MAM) {
  // This function is based on the "sanitizer_precompute_pass.cpp"
  // example of the precompute compiler library.
  llvm::errs() << "[AllocTrackerLTOPass] SLICING ...\n";

  PrecomputeFunctions::create_instance(M);
  analysis_results = new RequiredAnalysisResults(MAM, M);

  Function *main_F = M.getFunction("main");
  if (!main_F) {
    llvm::errs()
        << "[AllocTrackerLTOPass] Aborting: Could not find main function.\n";
    return false;
  }

  // combined slicing criterion <S, V>
  std::vector<Value *> to_precompute; // these are the values v in set V
  std::vector<Instruction *>
      precompute_locations; // these are the locations s in set S

  // Traverse all instructions within the function and find call instructions
  // that should be added to slicing criterion
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    for (auto &I : llvm::instructions(F)) {
      if (auto *call_I = llvm::dyn_cast<CallInst>(&I)) {
        CallBase *call = call_I; // get call base by implicit conversion

        if (auto *called_F = call_I->getCalledFunction()) {
          llvm::StringRef called_F_name = called_F->getName();

          // Check if current call instruction is a required MPI function call,
          // then add it to the slicing criterion. This is only needed if
          // AddAllMPICommunication is set
          add_required_mpi_function_to_slicing_criterion(
              to_precompute, precompute_locations, call_I, called_F_name);

          if (called_F_name == "malloc") {

            Value *malloc_size_V =
                call_I->getArgOperand(0); // size is first argument

            to_precompute.push_back(malloc_size_V);
            precompute_locations.push_back(call);

          } else if (called_F_name == "calloc") {

            Value *calloc_num_V =
                call_I->getArgOperand(0); // num is first argument
            Value *calloc_size_V =
                call_I->getArgOperand(1); // size is second argument

            to_precompute.push_back(calloc_num_V);
            to_precompute.push_back(calloc_size_V);
            precompute_locations.push_back(call);

          } else if (called_F_name == "realloc") {

            Value *realloc_size_V =
                call_I->getArgOperand(1); // new size is second argument

            to_precompute.push_back(realloc_size_V);
            precompute_locations.push_back(call);

          } else if (called_F_name == "aligned_alloc") {

            Value *aligned_alloc_size_V =
                call_I->getArgOperand(1); // size is second argument

            to_precompute.push_back(aligned_alloc_size_V);
            precompute_locations.push_back(call);
          }
        }
      }
    }
  }

  llvm::errs() << "[AllocTrackerLTOPass] Done adding precomputation values: ";
  llvm::errs() << "to_precompute size = " << to_precompute.size()
               << ", precompute_locations size = "
               << precompute_locations.size() << "\n";

  // Nothing to do
  if (precompute_locations.empty()) {
    llvm::errs() << "[AllocTrackerLTOPass] Note: No dynamic memory allocation "
                    "functions found. Abort Slicing.\n";
    return false;
  }

  timer::CustomTimer slicing_timer;
  slicing_timer.start();

  // Check if call instruction of MPI_Init() call can be found
  // -> need to add MPI_Init() and MPI_Finalize() again after slicing
  CallInst *mpi_init_call = get_mpi_init_call(M);
  Value *argc_ptr = nullptr;
  Value *argv_ptr = nullptr;
  if (mpi_init_call) {
    llvm::errs() << "[AllocTrackerLTOPass] Input program uses MPI.\n";
    input_program_uses_mpi_ = true;
    assert(mpi_init_call->arg_size() == 2 &&
           "MPI Init does not have two arguments.");

    argc_ptr = mpi_init_call->getArgOperand(0); // int *argc
    argv_ptr = mpi_init_call->getArgOperand(1); // char ***argv
  }

  // Analyse module for slicing
  precalc_analysis_ = std::make_shared<PrecalculationAnalysis>(
      M, main_F, to_precompute, precompute_locations, problematic_calls_,
      arguments_.IgnoreMPICommunication, arguments_.AddAllMPICommunication);

  // Remove duplicates from problematic allocation call vector (are sometimes
  // added in slicing analysis)
  utils::remove_duplicates_from_vector(problematic_calls_);

  // Actually slice the module
  precalculation_ =
      std::make_shared<PrecomputeInsertion>(M, precalc_analysis_, false, false);

  // Retrieve and save all Instructions from problematic_calls (that are
  // problematic allocations) in the new slice
  llvm::errs()
      << "[AllocTrackerLTOPass] Instructions / dynamic allocations to skip:\n";
  for (auto *e : problematic_calls_) {
    problematic_calls_in_slice_.push_back(
        precalculation_->get_precomputed_value(e));
  }

  // Set all the arguments of the problematic allocation calls to 0.
  // This ensures no problematic call that still needs to be kept in slice will
  // actually allocate uninitialized memory, i.e. random values most likely very
  // large numbers that would let the allocation fail.
  set_problematic_call_args_to(0);

  // Finalize module
  auto precompute_main = precalculation_->get_precompute_main();
  auto it = precompute_main->begin()->begin();
  //++it; // second instruction is NOT call to precomputed main since we removed
  //some precompute specific functions
  auto *call = cast<CallBase>(it);
  auto precomputed_main = call->getCalledFunction();

  // remove old main
  auto orig_linkeage = main_F->getLinkage();
  main_F->deleteBody(); // will set linkeage to weak, which we dont want
  main_F->setLinkage(orig_linkeage);

  // create new function body
  auto *bb = BasicBlock::Create(M.getContext(), "entry", main_F);
  // add call to precompute
  IRBuilder<> builder = IRBuilder<>(bb);

  // only for evaluation: calls runtime function that logs program start
  // timestamp
  logging::instrument_start_of_program(M, builder);

  // forward args of main
  std::vector<Value *> args;
  std::vector<Value *> args_as_allocas;
  for (auto &arg : main_F->args()) {
    args.push_back(&arg);

    AllocaInst *arg_Alloca = builder.CreateAlloca(arg.getType());
    builder.CreateStore(&arg, arg_Alloca);
    args_as_allocas.push_back(arg_Alloca);
  }

  // Add call to MPI_Init() if the application originally used MPI.
  // They are (potentially) removed while slicing and thus must be added again
  // afterwards if MPI calls are still presenent. Need to do that for every
  // return?
  if (input_program_uses_mpi_ && argc_ptr && argv_ptr) {
    llvm::errs()
        << "[AllocTrackerLTOPass] Adding MPI Init again after slicing.\n";

    Function *mpi_init_F = M.getFunction("MPI_Init");
    assert(mpi_init_F &&
           "Could not find MPI_Init() function declaration after slicing!");

    // Create call to MPI_Init()
    builder.CreateCall(mpi_init_F, args_as_allocas);
  }

  // Create call to new main
  builder.CreateCall(precomputed_main, args);

  // Add call to MPI_Finalize() if the application originally used MPI.
  // NOTE: if MPI_Init() was found in the original application, then
  //       its assumed that MPI_Finalize() was also present.
  if (input_program_uses_mpi_ && argc_ptr && argv_ptr) {
    llvm::errs()
        << "[AllocTrackerLTOPass] Adding MPI Finalize again after slicing.\n";

    Function *mpi_finalize_F = M.getFunction("MPI_Finalize");
    assert(mpi_finalize_F &&
           "Could not find MPI_Finalize() function declaration after slicing!");

    builder.CreateCall(mpi_finalize_F);
  }

  // only for evaluation: calls runtime function that logs program end timestamp
  logging::instrument_end_of_program(M, builder);

  // Create return similar to old main
  builder.CreateRet(Constant::getNullValue(main_F->getReturnType()));

  // Mark precompute main (that is not needed) as internal linkage
  // so that the DCE pass will remove it
  if (Function *precompute_main_F = M.getFunction("precompute_main")) {
    precompute_main_F->setLinkage(GlobalValue::InternalLinkage);
  }

  // Cleanup
  remove_noinline_from_module(M);

  PrecomputeFunctions::delete_instance();
  delete analysis_results;

  llvm::errs() << "[AllocTrackerLTOPass] Slicing complete.\n";

  if (!utils::verify_module(M))
    throw std::runtime_error(
        "[AllocTrackerLTOPass] ERROR: Module could not be verfied!");

  // run analysis passes like dead code elim to remove unnecessary function
  // copies
  run_optimization_passes(M, MAM);

  // Save slice module dump to file
  io::write_module_dump_to_file(M, "only_slice_module_dump.ll");

  // Time successfull execution
  double slicing_duration = slicing_timer.stop();

  io::log_time_for("Slicing", slicing_duration);

  return true;
}

bool AllocTrackerLTOPass::inline_allocation_wrapper_call(Module &M,
                                                         CallInst *call_I) {
  // Only inline valid call instruction that is not marked no inline
  if (!call_I)
    return false;

  Function *called_F = call_I->getCalledFunction();

  // Only proceed if called function exists and is defined
  if (!called_F || called_F->isDeclaration())
    return false;

  // Remove attributes optnon and noinline if present
  if (called_F->hasFnAttribute(Attribute::OptimizeNone))
    called_F->removeFnAttr(Attribute::OptimizeNone);
  if (called_F->hasFnAttribute(Attribute::NoInline))
    called_F->removeFnAttr(Attribute::NoInline);

  // Inline the function
  InlineFunctionInfo IFI;
  llvm::InlineResult IR = InlineFunction(*call_I, IFI);

  if (!IR.isSuccess()) {
    llvm::errs() << "[AllocTrackerLTOPass] Failed to inline function: "
                 << called_F->getName() << "\n";
    return false;
  }

  return true;
}

bool AllocTrackerLTOPass::is_allocation_wrapper(llvm::StringRef name) const {
  if (arguments_.AllocationWrapperFunctions.empty())
    return false;

  auto it = std::find(arguments_.AllocationWrapperFunctions.begin(),
                      arguments_.AllocationWrapperFunctions.end(), name.str());

  return it != arguments_.AllocationWrapperFunctions.end();
}

bool AllocTrackerLTOPass::inline_allocation_wrapper_calls(Module &M) {
  timer::CustomTimer inline_timer;
  inline_timer.start();

  llvm::errs() << "[AllocTrackerLTOPass] Inlining all wrapper function calls "
                  "for dynamic memory allocation functions ...\n";

  bool modified = false;
  unsigned int n_inlined = 0;

  // Two phase approach to avoid invalidating iterators when iterating over
  // module and transforming module at the same time
  std::vector<llvm::CallInst *> call_I_to_be_inlined;

  // Phase 1: Collect all call instructions that should be inlined
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    for (auto &I : llvm::instructions(F)) {
      if (auto *call_I = llvm::dyn_cast<CallInst>(&I)) {

        if (auto *called_F = call_I->getCalledFunction()) {
          if (called_F->isIntrinsic())
            continue;

          llvm::StringRef called_F_name = called_F->getName();

          if (is_allocation_wrapper(called_F_name)) {
            call_I_to_be_inlined.push_back(call_I);
          }
        }
      }
    }
  }

  // Phase 2: Actually inline all collected call instructions
  for (auto *call_I : call_I_to_be_inlined) {
    bool inlined = inline_allocation_wrapper_call(M, call_I);

    if (inlined) {
      ++n_inlined;
      modified = true;
    }
  }

  double inline_duration = inline_timer.stop();
  llvm::errs() << "[AllocTrackerLTOPass] Finished inlining " << n_inlined
               << " wrapper function calls in " << inline_duration
               << " seconds.\n";

  io::log_time_for("Inline", inline_duration);

  io::write_module_dump_to_file(M, "original_module_inlined_dump.ll");

  return modified;
}

bool AllocTrackerLTOPass::run_on_module(Module &M, ModuleAnalysisManager &MAM) {
  timer::CustomTimer module_timer;
  module_timer.start();

  io::cleanup_previously_generated_files();

  io::write_module_dump_to_file(M, "original_module_dump.ll");

  // Inline wrapper functions if present
  bool inlined = inline_allocation_wrapper_calls(M);

  sliced_ = arguments_.EnableSlicing ? slice_module(M, MAM) : false;

  bool replaced = arguments_.EnableLogging
                      ? instrument_allocations_with_logging_functions(M, MAM)
                      : false;

  double module_total_duration = module_timer.stop();
  llvm::errs() << "\n[AllocTrackerLTOPass] Time to run AllocTrackerLTOPass: "
               << module_total_duration << " seconds\n\n";

  io::log_time_for("Module total", module_total_duration);

  return inlined || sliced_ || replaced;
}

PreservedAnalyses AllocTrackerLTOPass::run(Module &M,
                                           ModuleAnalysisManager &MAM) {
  arguments_ = get_arguments();
  llvm::errs() << "[AllocTrackerLTOPass] Running pass on Module with name: "
               << M.getName() << "\n";
  check_and_print_pass_options();

  bool modified = run_on_module(M, MAM);

  return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
