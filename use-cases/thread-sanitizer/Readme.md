# TODO: Heading

This Repository contains the llvm pass that removes computation from an application, while retaining the Tsan
instrumentation, leading to a program skeleton that has only the data race detection.

All commands in code blocks below are executed in the root directory of this repository.
ut not all files or directories meantioned in the text are relativ that. Some are relativ to the directory where this ReadMe is located in.

## Prerequisites

A C/C++ compiler with `libclang-rt`(asan) and `openmp` support and possibly `boost`.

For this Project, we used clang/`llvm 16.0.1`
The `cmake` configure step will download [DataRaceBench](https://github.com/LLNL/dataracebench) for testing

## Building

Building with cmake is straight forward:
```bash
cmake -B 'build' -S . -G 'Ninja' -DMPI_USE_CASE='off'
cmake --build build
```
Without `-G` it defaults to "Unix Makefiles", but "Ninja" is the hot shit to use with LLVM.
If you want to also use the MPI usecase, do not disable it (`MPI_USE_CASE`).

## Usage

The build step creates a ``setup_env.sh`` file, that sets the required environment variables.
To build an application with the pass, replace the compiler to use with ``clang_wrap_cc`` or ``clang_wrap_cxx``
respectively.
The ``setup_env.sh`` defines the envrionment variables `CLANG_WRAP_CC` and `CLANG_WRAP_CXX` that are meant to be used as
a clang/clang++ replacement to enable the pass.
The variable ``export USE_COMPILER_PASS=true`` or `false` determines, if the Pass should be activated.
The compile step needs the following command line arguments to work
correctly: ``-fno-inline -flto -fwhole-program-vtables``.
The ``-fno-inline`` will be removed after the analysis, so that inlining does happen.

## Tests

The ctest tests check the detection accuracy against the original tsan implementation.
As the data race affected testcases include nondeterministic behaviour, it is expected, that some tests may fail.
In particular, `DRB185-barrier1-yes` fails 99% of the time due to a limitation in the Tsan implementation.

Just running the tests:
```bash
cmake --build build -- test
```
This equivilent to running
```bash
ctest --test-dir build
```
Optionally add `--timeout 3` to the arguments

Rerun and check why tests failed:
```bash
ctest --test-dir build --timeout 3 --rerun-failed --output-on-failure
```
If reason "Timeout" remove the arguments. 

Run single test:
```bash
ctest --test-dir build -R "DRB027-taskdependmissing-orig-yes"
```

## Performance

sample_apps/performance_evaluation contains the scripts ew used for performance evaluation.

#### References

TODO!
<table style="border:0px">
<tr>
    <td valign="top"><a name="ref-1"></a>[1]</td>
    <td>
Tim Jammer, Tim Heldmann, Michael Blesel, Michael Kuhn, Christian Bischof, "Compiler-Based Precalculation of MPI Message Envelopes" To Appear In: ISC High Performance 2024 International Workshops
      </td>
</tr>
<tr>
    <td valign="top"><a name="ref-2"></a>[2]</td>
    <td>Tim Jammer and Christian Bischof "Compiler-enabled optimization of persistent MPI Operations" In : 2022 IEEE/ACM International Workshop on Exascale MPI (ExaMPI) https://doi.org/10.1109/ExaMPI56604.2022.00006</td>
</tr>


