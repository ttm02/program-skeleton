NUMBER=429
NAME=mcf
SOURCES=mcf.c \
				mcfutil.c \
				readmin.c \
				implicit.c \
				pstart.c \
				output.c \
				treeup.c \
				pbla.c \
				pflowup.c \
				psimplex.c \
				pbeampp.c

BENCH_CFLAGS     = -DWANT_STDC_PROTO -DSPEC_CPU -DNDEBUG
CC               = gcc
COPTIMIZE        = -fno-strict-aliasing # -O2
FPBASE           = yes
OS               = unix
PORTABILITY      = -DSPEC_CPU_LP64


### changed
pass_path = ../../build-libs/alloc_tracking_compiler_pass/libAllocTrackerLTOPass.so
precomputation_lib_path = ../../build-libs/precomputation_compiler_lib/libprecomputation_compiler_lib.so
precomputation_lib_dir = $(dir $(precomputation_lib_path))
runtime_lib_path = ../../build-libs/alloc_tracking_runtime_lib

ADDITIONAL_LINKER_FLAGS = -flto -fuse-ld=lld -Wl,--load-pass-plugin=$(pass_path) -Wl,-mllvm=-load=$(pass_path)
ADDITIONAL_LINKER_FLAGS += -Wl,-mllvm=-at-pass-enable-slicing=false -Wl,-mllvm=-at-pass-enable-logging=true
ADDITIONAL_LINKER_FLAGS += -Wl,-rpath,$(runtime_lib_path) -L$(runtime_lib_path) -lAllocTrackerRuntimeLib
ADDITIONAL_LINKER_FLAGS += -Wl,-rpath=$(precomputation_lib_dir)

###
# CI script compatibility section
###

COMPILER=clang
CFLAGS+= $(COPTIMIZE) $(PORTABILITY) $(BENCH_CFLAGS) -target x86_64-pc-linux-gnu #-O2
CFLAGS += -flto

$(NAME): $(SOURCES)
	$(COMPILER) -o $@ $(CFLAGS) $(SOURCES) $(ADDITIONAL_LINKER_FLAGS)


clean:
	rm -rf mcf
