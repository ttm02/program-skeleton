# Program skeleton slicing

This Repository contains the llvm pass that computes a program skeleton needed to

## Prerequisites

For this Project, we used clang/`llvm 21.1.0`

## Building / Usage

Currently. There are three different use cases:

* Removed for anonymity
* Thread Sanitizer: Skeleton based on Tsan Instrumentation without computation. Camke Option: `SANITIZER_USE_CASE`
  Use The Cmake Options to build them.
* Alloc Tracking: Skeleton that calculates the allocation size of a program

Refer to the Readme in each use cases directory for more information about its usage.