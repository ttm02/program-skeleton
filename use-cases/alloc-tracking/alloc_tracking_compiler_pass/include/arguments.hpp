#ifndef ALLOCTRACKING_ARGUMENTS_H
#define ALLOCTRACKING_ARGUMENTS_H

#include "llvm/Support/CommandLine.h"
#include <string>
#include <vector>
#include <cstdlib>

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
  auto env_var = std::getenv("AT_PASS_ALLOCATION_WRAPPER_FUNCTIONS");
  if (env_var) {
    const char *delimiter = ",";
    char *token = strtok(env_var, delimiter);
    while (token != nullptr) {
      AllocationWrapperFunctions.push_back(token);
      // Get the next substring
      token = strtok(nullptr, delimiter);
    }
  }

  args.EnableSlicing = EnableSlicing;
  env_var = std::getenv("AT_PASS_ENABLE_SLICING");
  if (env_var) {
    args.EnableSlicing = true;
  }
  args.EnableLogging = EnableLogging;
  env_var = std::getenv("AT_PASS_ENABLE_LOGGING");
  if (env_var) {
    args.EnableLogging = true;
  }
  args.IgnoreMPICommunication = IgnoreMPICommunication;
  env_var = std::getenv("AT_PASS_IGNORE_MPI_COMMUNICATION");
  if (env_var) {
    args.IgnoreMPICommunication = true;
  }
  args.AddAllMPICommunication = AddAllMPICommunication;
  env_var = std::getenv("AT_PASS_ADD_ALL_MPI_COMMUNICATION");
  if (env_var) {
    args.AddAllMPICommunication = true;
  }

  return args;

}

#endif // ALLOCTRACKING_ARGUMENTS_H
