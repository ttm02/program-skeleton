# TODO: Heading

This Repository contains the llvm pass that removes computation from an application, while retaining the Tsan
instrumentation, leading to a program skeleton that has only the data race detection.

## Prerequisites

For this Project, we used clang/`llvm 16.0.1`
The cmake configure step will download DataRaceBench for testing

## Building

Building with cmake is straight forward:

```
mkdir build && cd build
cmake ..
make -j
source setup_env.sh
ctest --timeout 3 # run the tests to check if build was successful
```

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

## Performance

sample_apps/performance_evaluation contains the scripts ew used for performance evaluation.

#### References

