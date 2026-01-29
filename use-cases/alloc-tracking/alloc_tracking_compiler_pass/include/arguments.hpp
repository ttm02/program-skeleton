#ifndef ALLOCTRACKING_ARGUMENTS_H
#define ALLOCTRACKING_ARGUMENTS_H

#include "llvm/Support/CommandLine.h"

/**
 * Command Line Parameter Options
 */


namespace {


// -at-pass-allocation-wrapper-functions command line option
llvm::cl::list<std::string> AllocationWrapperFunctions(
    "at-pass-allocation-wrapper-functions",
    llvm::cl::desc("List of allocation wrapper function names"),
    llvm::cl::CommaSeparated, // separate names by comma
    llvm::cl::ZeroOrMore // allows for zero or more names
);

// -at-pass-enable-slicing command line option
llvm::cl::opt<bool> EnableSlicing(
    "at-pass-enable-slicing",
    llvm::cl::desc("Enable slicing"),
    llvm::cl::init(false) // default no slicing
);

// -at-pass-enable-logging command line option
llvm::cl::opt<bool> EnableLogging(
    "at-pass-enable-logging",
    llvm::cl::desc("Enable logging"),
    llvm::cl::init(false) // default no logging
);

// -at-pass-ignore-mpi-communication command line option
llvm::cl::opt<bool> IgnoreMPICommunication(
    "at-pass-ignore-mpi-communication",
    llvm::cl::desc("Ignore MPI Communication"),
    llvm::cl::init(false)
);

// -at-pass-add-all-mpi-communication command line option
llvm::cl::opt<bool> AddAllMPICommunication(
    "at-pass-add-all-mpi-communication",
    llvm::cl::desc("Add all MPI Communication"),
    llvm::cl::init(false)
);

} // end namespace


struct arguments {
  std::vector<std::string> AllocationWrapperFunctions;
  bool EnableSlicing;
  bool EnableLogging;
  bool IgnoreMPICommunication;
  bool AddAllMPICommunication;
};


struct arguments get_arguments() {
  struct arguments args;
  args.AllocationWrapperFunctions = AllocationWrapperFunctions;
  args.EnableSlicing = EnableSlicing;
  args.EnableLogging = EnableLogging;
  args.IgnoreMPICommunication = IgnoreMPICommunication;
  args.AddAllMPICommunication = AddAllMPICommunication;

  return args;

}

#endif // ALLOCTRACKING_ARGUMENTS_H
