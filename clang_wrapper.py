#!/usr/bin/env python3
import os
import sys
import subprocess
import tempfile
import shutil

program_name = os.path.basename(sys.argv[0])
if program_name in ("clang_wrap_cc", "clang_wrapper", "clang"):
    compiler = "clang"
elif program_name in ("clang_wrap_cxx", "clang++_wrapper", "clang++"):
    compiler = "clang++"
elif program_name in ("flang_wrap", "flang_wrapper", "flang"):
    compiler = "flang"
elif program_name in ("flang_wrap_new", "flang-new_wrapper", "flang-new"):
    compiler = "flang-new"
else:
    print(
        f"Error: unknown wrapper invocation '{program_name}' - expected clang_wrap_cc, clang_wrap_cxx, or flang_wrap"
    )
    sys.exit(1)

if "flang" in compiler:
    compiler_type = "fortran"
elif "clang" in compiler:
    compiler_type = "cpp"
else:
    print(f'unknown compiler used: "{compiler}"')
    sys.exit(1)

ld_preload_prev = os.environ.get("LD_PRELOAD", "")
debug_wrapper = os.environ.get("DEBUG_CLANG_WRAPPER", "false").lower() == "true"

use_compiler_pass_env = os.environ.get("USE_COMPILER_PASS", "0")
use_compiler_pass = (
    use_compiler_pass_env == "1" or use_compiler_pass_env.lower() == "true"
)
use_static_analysis_env = os.environ.get("USE_STATIC_ANALYSIS", "0")
use_static_analysis = (
    use_static_analysis_env == "1" or use_static_analysis_env.lower() == "true"
)
pass_plugin_opts = []


if debug_wrapper:
    print("INVOKE CLANG_WRAPPER")
    print(program_name, " ".join(sys.argv[1:]))
    try:
        libasan = subprocess.check_output(
            ["clang", "-print-file-name=libclang_rt.asan.so"], text=True
        ).strip()
        os.environ["LD_PRELOAD"] = f"{libasan}:{ld_preload_prev}"
    except subprocess.CalledProcessError:
        pass

args = sys.argv[1:]
is_to_obj = False
has_o_option = False
has_o_files = False
has_flto = False
has_fwhole_program_vtables = False
has_opt_lvl = False
has_src_file = False
has_multiple_src_file = False

arg_remove_list = []

for arg in args:
    if arg == "-c":
        is_to_obj = True
    elif arg == "-o":
        has_o_option = True
    elif arg.endswith(".o"):
        has_o_files = True
    elif arg == "-flto":
        has_flto = True
    elif arg == "-fwhole-program-vtables":
        has_fwhole_program_vtables = True
    elif arg in ("-O1", "-O2", "-O3"):
        has_opt_lvl = True
    elif any(arg.endswith(ext) for ext in [".c", ".cpp", ".cc", ".cxx"]):
        if has_src_file:
            has_multiple_src_file = True
        has_src_file = True
    elif arg == "--enable-static-analysis":
        use_static_analysis = True
        arg_remove_list.append(arg)
    elif arg == "--disable-slicing" or arg.startswith("--static-analysis-mode="):
        arg_remove_list.append(arg)
        pass_plugin_opts.append(arg)
    elif arg == "--disable-precompute-pass":
        use_compiler_pass = False
        arg_remove_list.append(arg)

for arg in arg_remove_list:
    args.remove(arg)

# check if necessary flags are given
if use_compiler_pass and (
    not has_flto
    or (not has_fwhole_program_vtables and compiler_type != "fortran")
    or not has_opt_lvl
):
    print(
        "Error, need -flto and -fwhole-program-vtables and at least -O1 for pass to work correctly"
    )
    sys.exit(1)

if use_compiler_pass and "COMPILER_PASS" not in os.environ:
    print("The COMPILER_PASS environment variable is not set")
    sys.exit(1)

pass_plugin_arg = False


def load_pass_args():
    global pass_plugin_arg, pass_args
    if pass_plugin_arg:
        return
    # arguments to opt pass need old `-load` syntax for some reason
    # https://github.com/llvm/llvm-project/issues/56137
    pass_args += ["-Xclang", "-load", "-Xclang", os.environ["COMPILER_PASS"]]
    pass_plugin_arg = True


pass_args = ["-fpass-plugin=" + os.environ["COMPILER_PASS"], "-lprecompute"]
if use_static_analysis:
    load_pass_args()
    pass_args += ["-mllvm", "--enable-static-analysis"]

for opt in pass_plugin_opts:
    load_pass_args()
    pass_args += ["-mllvm", opt]


def run_command(cmd, resume_after=False):
    if debug_wrapper:
        print("RUN:", " ".join(cmd))
    try:
        subprocess.run(cmd, check=True)
    except KeyboardInterrupt:
        sys.exit(130)
    except subprocess.CalledProcessError:
        print("compilation failed")
        print(cmd)
        sys.exit(255)

    if not resume_after:
        sys.exit(0)


if is_to_obj:
    if debug_wrapper:
        print("MODE: to obj file")
    if has_multiple_src_file:
        print(
            "ERROR linking multiple src files directly into one object file is not supported"
        )
        print("Compile one by one and link afterwards")
        sys.exit(1)

    print([compiler] + args)
    subprocess.call([compiler] + args)

    obj_list = []
    cmd = [compiler]
    for i, arg in enumerate(args):
        if arg == "-c":
            cmd += ["-c", "-emit-llvm"]
        elif arg.endswith(".o"):
            # Remove the ".o" suffix and append ".bc"
            arg_basename = arg[:-2]
            new_file = arg_basename + ".bc"
            cmd.append(new_file)
            # if expected output, actually compile it
            if args[i - 1] == "-o":
                obj_list.append(arg_basename)
        else:
            cmd.append(arg)

    run_command(cmd, True)
    for obj_base in obj_list:
        run_command([compiler, "-c", f"{obj_base}.bc", "-o", f"{obj_base}.o"], True)
    sys.exit(0)

if has_o_files:
    if debug_wrapper:
        print("MODE: Link .o files")

    # -x ir - : read ir from stdin
    tmp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".bc").name
    cmd = [compiler, tmp_file]
    if use_compiler_pass:
        cmd += pass_args

    llvm_link = ["llvm-link", "-o", tmp_file]

    for arg in args:
        if arg.endswith(".o"):
            # Remove the ".o" suffix and append ".bc"
            new_file = arg[:-2] + ".bc"
            # remove from compiler invocation and add to file list
            llvm_link.append(new_file)
        elif arg.endswith(".so"):
            # in our mode we cannot enter .o and .so files so we need to tell it to link it with -l
            base = os.path.basename(arg)
            # Use parameter expansion to remove file extensions
            lib_fname = os.path.splitext(base)[0]
            # Use parameter expansion to remove "lib" from the beginning
            lib_name = lib_fname[3:] if lib_fname.startswith("lib") else lib_fname
            # Use dirname to get the directory part (will at least result in ".")
            directory = os.path.dirname(arg) or "."
            cmd += [f"-L{directory}", f"-l{lib_name}"]
        else:
            cmd.append(arg)

    if debug_wrapper:
        print(f"{llvm_link} && {cmd}")

    try:
        subprocess.check_call(llvm_link)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)

    run_command(cmd)

if debug_wrapper:
    print("MODE: direct to Binary")

if has_multiple_src_file:
    print(
        "ERROR linking multiple src files directly into one binary file is not supported"
    )
    print("Compile one by one and link afterwards")
    sys.exit(1)

cmd = [compiler]
if use_compiler_pass:
    cmd += pass_args
cmd += args

run_command(cmd)
