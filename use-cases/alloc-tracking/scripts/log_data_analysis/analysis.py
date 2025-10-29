#!/usr/bin/env python3
"""
HPC Test Suite Analysis Script
Compares slicing on/off configurations and extracts performance metrics
Enhanced version that tracks memory amounts for invalid and required calls
Now also records per-process (per-rank) metrics so you can look up rank 0.
"""

import os
import glob
import re
from pathlib import Path
from collections import defaultdict

# Hardcoded test cases
TEST_CASES = [
    "036_heat_eq",
    "037_matrix",
    "042_poisson_mpi",
    "043_wave_mpi",
    "044_walsh_haar",
    "045_hypersphere_monte_carlo",
    "048_mpi_heat_eq_solver",
    "048_mpi_heat_eq_solver2",
    "048_mpi_heat_eq_solver3",
    "048_mpi_heat_eq_solver4",
    "048_mpi_heat_eq_solver5",
    "048_mpi_heat_eq_solver6",
    "spec_cpu"
]

# Build configurations
BUILD_SLICING_OFF = "build_release_slicing_off_logging_on"
BUILD_SLICING_ON = "build_release_slicing_on_logging_on"


def detect_timestamp_format(s: str) -> str:
    # Regex for seconds.microseconds (must contain a dot)
    if re.search(r"\b\d+\.\d+\b", s):
        return "seconds.microseconds"
    
    # Regex for pure integer
    match = re.search(r"\b\d+\b", s)
    if match:
        num_str = match.group()
        return "nanoseconds"
    
    return "unknown"


def extract_slicing_duration(slicing_info_path):
    """Extract slicing duration from slicing_information.txt"""
    if not os.path.exists(slicing_info_path):
        return None

    with open(slicing_info_path, 'r') as f:
        content = f.read()
        # Look for "Slicing duration" followed by scientific notation or float
        match = re.search(r'Slicing duration[:\s]+([0-9]+\.?[0-9]*[eE]?[+-]?[0-9]*)', content)
        if match:
            return float(match.group(1))
    return None

def count_lines_in_file(file_path):
    """Count lines in a file"""
    if not os.path.exists(file_path):
        return None

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        return sum(1 for _ in f)

def get_binary_size(binary_path):
    """Get binary size in bytes"""
    if not os.path.exists(binary_path):
        return None
    return os.path.getsize(binary_path)

def _extract_rank_from_name(name):
    """Try a few filename-based patterns to extract an integer rank (or None)."""
    # common patterns: rank0, rank_0, rank-0, mpi_rank0, r0, _r0_
    patterns = [
        r'(?:^|[_\-.])(?:rank|mpi_rank|mpirank)[_\-:=]?(\d+)(?:[_\-.]|$)',  # rank0, _rank0_, ...
        r'(?:^|[_\-.])r(\d+)(?:[_\-.]|$)',  # _r0_, -r0-
        r'(?:^|[_\-.])proc[_\-.]?(\d+)(?:[_\-.]|$)',
        r'(?:^|[_\-.])pe[_\-.]?(\d+)(?:[_\-.]|$)',
    ]
    for pat in patterns:
        m = re.search(pat, name, re.IGNORECASE)
        if m:
            try:
                return int(m.group(1))
            except Exception:
                continue
    return None

def _extract_rank_from_lines(lines):
    """Search the file content for a rank indicator."""
    for line in lines:
        # look for e.g. "rank: 0", "rank = 0", "MPI rank: 0", "mpi_rank:0"
        m = re.search(r'\b(?:mpi[_\-\s]?rank|mpirank|rank)\b\s*[:=]?\s*(\d+)\b', line, re.IGNORECASE)
        if m:
            try:
                return int(m.group(1))
            except Exception:
                continue
    return None

def parse_allocation_log(log_path):
    """Parse allocation log file and extract metrics. Also attempt to detect the MPI rank."""
    if not os.path.exists(log_path):
        return None

    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = [l.rstrip("\n") for l in f]

    # Try to get rank from file name first
    filename = os.path.basename(log_path)
    rank = _extract_rank_from_name(filename)

    # If not found, search inside the file
    if rank is None:
        rank = _extract_rank_from_lines(lines)

    # Extract start and end times
    start_time = None
    end_time = None

    malloc_count = 0
    calloc_count = 0
    realloc_count = 0
    aligned_alloc_count = 0
    total_memory = 0
    invalid_calls = 0
    required_calls = 0
    invalid_memory = 0  # NEW: Track memory for invalid calls
    required_memory = 0  # NEW: Track memory for required calls

    for line in lines:
        line = line.strip()

        # Extract program start/end times
        if line.startswith("program start:"):
            try:
                format = detect_timestamp_format(line)
                if format == "seconds.microseconds":
                    start_time = float(line.split(":", 1)[1].strip())
                else:
                    start_time = int(line.split(":", 1)[1].strip()) / 1_000_000_000.0 # convert to seconds
            except Exception:
                pass
        elif line.startswith("program end:"):
            try:
                format = detect_timestamp_format(line)
                if format == "seconds.microseconds":
                    end_time = float(line.split(":", 1)[1].strip())
                else:
                    end_time = int(line.split(":", 1)[1].strip()) / 1_000_000_000.0 # convert to seconds
                
            except Exception:
                pass

        # Parse allocation lines
        elif any(line.startswith(alloc_type) for alloc_type in ["malloc:", "calloc:", "realloc:", "aligned_alloc:"]):
            parts = [p.strip() for p in line.split(",")]

            # Count allocation types
            if line.startswith("malloc:"):
                malloc_count += 1
            elif line.startswith("calloc:"):
                calloc_count += 1
            elif line.startswith("realloc:"):
                realloc_count += 1
            elif line.startswith("aligned_alloc:"):
                aligned_alloc_count += 1

            # Extract allocation size
            allocation_size = 0
            for part in parts:
                if "allocation size" in part:
                    # Handle cases like "allocation size 20( 5 * 4 )" or "allocation size 20"
                    size_match = re.search(r'allocation size\s*(\d+)', part)
                    if size_match:
                        try:
                            allocation_size = int(size_match.group(1))
                        except Exception:
                            allocation_size = 0
                        total_memory += allocation_size
                    break

            # Check if invalid and add to invalid memory
            is_invalid = False
            for part in parts:
                if "valid false" in part or re.search(r'\bvalid\s*[:=]?\s*false\b', part, re.IGNORECASE):
                    invalid_calls += 1
                    invalid_memory += allocation_size  # NEW: Add to invalid memory sum
                    is_invalid = True
                    break

            # Check if required and add to required memory
            for part in parts:
                if "required to allocate true" in part or re.search(r'\brequired(?:\s*to\s*allocate)?\s*[:=]?\s*true\b', part, re.IGNORECASE):
                    required_calls += 1
                    required_memory += allocation_size  # NEW: Add to required memory sum
                    break

    execution_time = None
    if start_time is not None and end_time is not None:
        execution_time = end_time - start_time

    return {
        'rank': rank,
        'execution_time': execution_time,
        'total_memory': total_memory,
        'malloc_count': malloc_count,
        'calloc_count': calloc_count,
        'realloc_count': realloc_count,
        'aligned_alloc_count': aligned_alloc_count,
        'total_allocation_calls': malloc_count + calloc_count + realloc_count + aligned_alloc_count,
        'invalid_calls': invalid_calls,
        'required_calls': required_calls,
        'invalid_memory': invalid_memory,  # NEW: Total memory for invalid calls
        'required_memory': required_memory  # NEW: Total memory for required calls
    }

def analyze_test_case(test_case, results_dir):
    """Analyze a single test case. Returns a dict keyed by build config."""
    results = {}

    for build_config in [BUILD_SLICING_OFF, BUILD_SLICING_ON]:
        test_path = os.path.join(results_dir, build_config, "tests", test_case)

        if not os.path.exists(test_path):
            print(f"Warning: Test path does not exist: {test_path}")
            continue

        config_results = {}

        # Extract slicing duration (only for slicing_on)
        if build_config == BUILD_SLICING_ON:
            slicing_info_path = os.path.join(test_path, "slicing_information.txt")
            config_results['slicing_duration'] = extract_slicing_duration(slicing_info_path)

        # Count lines in LLVM dumps
        original_dump_path = os.path.join(test_path, "original_module_dump.ll")
        config_results['original_module_lines'] = count_lines_in_file(original_dump_path)

        if build_config == BUILD_SLICING_ON:
            slice_dump_path = os.path.join(test_path, "only_slice_module_dump.ll")
            config_results['slice_module_lines'] = count_lines_in_file(slice_dump_path)

        # Get binary size
        binary_path = os.path.join(test_path, test_case)
        config_results['binary_size'] = get_binary_size(binary_path)

        # Parse allocation logs
        allocation_logs = glob.glob(os.path.join(test_path, "*_allocation_log_*.txt"))

        if allocation_logs:
            log_results = []
            logs_by_rank = {}
            max_execution_time = 0

            for i, log_file in enumerate(sorted(allocation_logs)):
                log_data = parse_allocation_log(log_file)
                if log_data:
                    log_results.append(log_data)
                    # update max execution time across processes
                    if log_data['execution_time'] and log_data['execution_time'] > max_execution_time:
                        max_execution_time = log_data['execution_time']

                    # Put into rank mapping if rank detected
                    detected_rank = log_data.get('rank')
                    if detected_rank is not None:
                        # if duplicate rank logs exist, store them in a list (rare, but safe)
                        if detected_rank in logs_by_rank:
                            # coalesce multiple logs for same rank into a list
                            existing = logs_by_rank[detected_rank]
                            if isinstance(existing, list):
                                existing.append(log_data)
                            else:
                                logs_by_rank[detected_rank] = [existing, log_data]
                        else:
                            logs_by_rank[detected_rank] = log_data
                    else:
                        # fallback unique key for unknown-ranked logs
                        unknown_key = f"unknown_{i}"
                        logs_by_rank[unknown_key] = log_data

            config_results['allocation_logs'] = log_results  # list for aggregation
            config_results['allocation_logs_by_rank'] = logs_by_rank  # dict for per-rank lookup
            config_results['max_execution_time'] = max_execution_time if max_execution_time > 0 else None

        results[build_config] = config_results

    return results

def format_size(size_bytes):
    """Format size in bytes to KB and MB"""
    if size_bytes is None:
        return "N/A", "N/A", "N/A"
    try:
        kb = size_bytes / 1000
        mb = size_bytes / 1000000
        return f"{size_bytes}", f"{kb:.2f}", f"{mb:.4f}"
    except Exception:
        return str(size_bytes), "N/A", "N/A"

def format_time(time_seconds):
    """Format time in seconds to seconds and milliseconds"""
    if time_seconds is None:
        return "N/A", "N/A"
    try:
        ms = time_seconds * 1000
        return f"{time_seconds:.6f}", f"{ms:.3f}"
    except Exception:
        return str(time_seconds), "N/A"

def _write_agg_and_per_rank(f, config_name, results_config):
    """Write aggregated totals and per-rank breakdown for a build config"""
    if 'max_execution_time' in results_config:
        exec_time = results_config['max_execution_time']
        time_sec, time_ms = format_time(exec_time)
        f.write(f"    Max execution time: {time_sec} seconds ({time_ms} ms)\n")

    if 'allocation_logs' in results_config:
        logs = results_config['allocation_logs']
        if logs:
            # Aggregate data across all processes (same as before)
            total_memory = sum(log.get('total_memory', 0) for log in logs)
            total_malloc = sum(log.get('malloc_count', 0) for log in logs)
            total_calloc = sum(log.get('calloc_count', 0) for log in logs)
            total_realloc = sum(log.get('realloc_count', 0) for log in logs)
            total_aligned = sum(log.get('aligned_alloc_count', 0) for log in logs)
            total_calls = sum(log.get('total_allocation_calls', 0) for log in logs)
            total_invalid = sum(log.get('invalid_calls', 0) for log in logs)
            total_required = sum(log.get('required_calls', 0) for log in logs)
            total_invalid_memory = sum(log.get('invalid_memory', 0) for log in logs)
            total_required_memory = sum(log.get('required_memory', 0) for log in logs)

            # Format memory
            mem_bytes, mem_kb, mem_mb = format_size(total_memory)
            invalid_mem_bytes, invalid_mem_kb, invalid_mem_mb = format_size(total_invalid_memory)
            required_mem_bytes, required_mem_kb, required_mem_mb = format_size(total_required_memory)

            f.write(f"    Total allocated memory: {mem_bytes} bytes ({mem_kb} KB, {mem_mb} MB)\n")
            f.write(f"    Allocation calls:\n")
            f.write(f"      malloc: {total_malloc}\n")
            f.write(f"      calloc: {total_calloc}\n")
            f.write(f"      realloc: {total_realloc}\n")
            f.write(f"      aligned_alloc: {total_aligned}\n")
            f.write(f"      Total: {total_calls}\n")

            # Enhanced invalid and required call reporting
            f.write(f"    Invalid calls: {total_invalid}\n")
            f.write(f"    Invalid calls memory: {invalid_mem_bytes} bytes ({invalid_mem_kb} KB, {invalid_mem_mb} MB)\n")
            f.write(f"    Required calls: {total_required}\n")
            f.write(f"    Required calls memory: {required_mem_bytes} bytes ({required_mem_kb} KB, {required_mem_mb} MB)\n")

            # Calculate percentages for better insight
            if total_calls > 0:
                invalid_call_percentage = (total_invalid / total_calls) * 100
                required_call_percentage = (total_required / total_calls) * 100
                f.write(f"    Invalid calls percentage: {invalid_call_percentage:.2f}%\n")
                f.write(f"    Required calls percentage: {required_call_percentage:.2f}%\n")

            if total_memory > 0:
                invalid_memory_percentage = (total_invalid_memory / total_memory) * 100
                required_memory_percentage = (total_required_memory / total_memory) * 100
                f.write(f"    Invalid memory percentage: {invalid_memory_percentage:.2f}%\n")
                f.write(f"    Required memory percentage: {required_memory_percentage:.2f}%\n")

            # Number of processes (best-effort)
            if len(logs) > 1:
                f.write(f"    Number of processes: {len(logs)}\n")

            # Per-rank breakdown (if available)
            rank_map = results_config.get('allocation_logs_by_rank', {})
            if rank_map:
                f.write(f"\n    Per-process allocation data:\n")
                # Separate numeric ranks from unknown keys
                numeric = [(k, v) for k, v in rank_map.items() if isinstance(k, int)]
                unknown = [(k, v) for k, v in rank_map.items() if not isinstance(k, int)]
                numeric.sort(key=lambda x: x[0])  # sort by rank number

                # Write numeric ranks first
                for rk, log in numeric:
                    # If multiple logs stored as list for a rank, coalesce display
                    if isinstance(log, list):
                        # create an aggregate for that rank (rare)
                        r_total_mem = sum(l.get('total_memory', 0) for l in log)
                        r_malloc = sum(l.get('malloc_count', 0) for l in log)
                        r_calloc = sum(l.get('calloc_count', 0) for l in log)
                        r_realloc = sum(l.get('realloc_count', 0) for l in log)
                        r_aligned = sum(l.get('aligned_alloc_count', 0) for l in log)
                        r_calls = sum(l.get('total_allocation_calls', 0) for l in log)
                        r_invalid = sum(l.get('invalid_calls', 0) for l in log)
                        r_required = sum(l.get('required_calls', 0) for l in log)
                        r_invalid_mem = sum(l.get('invalid_memory', 0) for l in log)
                        r_required_mem = sum(l.get('required_memory', 0) for l in log)
                        r_exec_time = max((l.get('execution_time') or 0) for l in log) or None
                    else:
                        r_total_mem = log.get('total_memory', 0)
                        r_malloc = log.get('malloc_count', 0)
                        r_calloc = log.get('calloc_count', 0)
                        r_realloc = log.get('realloc_count', 0)
                        r_aligned = log.get('aligned_alloc_count', 0)
                        r_calls = log.get('total_allocation_calls', 0)
                        r_invalid = log.get('invalid_calls', 0)
                        r_required = log.get('required_calls', 0)
                        r_invalid_mem = log.get('invalid_memory', 0)
                        r_required_mem = log.get('required_memory', 0)
                        r_exec_time = log.get('execution_time')

                    mem_b, mem_k, mem_m = format_size(r_total_mem)
                    inv_b, inv_k, inv_m = format_size(r_invalid_mem)
                    req_b, req_k, req_m = format_size(r_required_mem)
                    time_s, time_ms = format_time(r_exec_time)

                    f.write(f"      Rank {rk}:\n")
                    f.write(f"        Total allocated memory: {mem_b} bytes ({mem_k} KB, {mem_m} MB)\n")
                    f.write(f"        Allocation calls: total={r_calls} (malloc={r_malloc}, calloc={r_calloc}, realloc={r_realloc}, aligned_alloc={r_aligned})\n")
                    f.write(f"        Invalid calls: {r_invalid} (memory={inv_b} bytes ({inv_k} KB, {inv_m} MB))\n")
                    f.write(f"        Required calls: {r_required} (memory={req_b} bytes ({req_k} KB, {req_m} MB))\n")
                    if r_exec_time is not None:
                        f.write(f"        Execution time (this rank): {time_s} seconds ({time_ms} ms)\n")

                # Then unknown keys
                for rk, log in unknown:
                    # rk is a string label
                    r_total_mem = log.get('total_memory', 0)
                    r_malloc = log.get('malloc_count', 0)
                    r_calloc = log.get('calloc_count', 0)
                    r_realloc = log.get('realloc_count', 0)
                    r_aligned = log.get('aligned_alloc_count', 0)
                    r_calls = log.get('total_allocation_calls', 0)
                    r_invalid = log.get('invalid_calls', 0)
                    r_required = log.get('required_calls', 0)
                    r_invalid_mem = log.get('invalid_memory', 0)
                    r_required_mem = log.get('required_memory', 0)
                    r_exec_time = log.get('execution_time')

                    mem_b, mem_k, mem_m = format_size(r_total_mem)
                    inv_b, inv_k, inv_m = format_size(r_invalid_mem)
                    req_b, req_k, req_m = format_size(r_required_mem)
                    time_s, time_ms = format_time(r_exec_time)

                    f.write(f"      {rk}:\n")
                    f.write(f"        Total allocated memory: {mem_b} bytes ({mem_k} KB, {mem_m} MB)\n")
                    f.write(f"        Allocation calls: total={r_calls} (malloc={r_malloc}, calloc={r_calloc}, realloc={r_realloc}, aligned_alloc={r_aligned})\n")
                    f.write(f"        Invalid calls: {r_invalid} (memory={inv_b} bytes ({inv_k} KB, {inv_m} MB))\n")
                    f.write(f"        Required calls: {r_required} (memory={req_b} bytes ({req_k} KB, {req_m} MB))\n")
                    if r_exec_time is not None:
                        f.write(f"        Execution time (this log): {time_s} seconds ({time_ms} ms)\n")

                # If rank 0 exists, highlight it
                if 0 in rank_map:
                    f.write("\n    Rank 0 details:\n")
                    r0 = rank_map[0]
                    if isinstance(r0, list):
                        # aggregate
                        r_total_mem = sum(l.get('total_memory', 0) for l in r0)
                        r_calls = sum(l.get('total_allocation_calls', 0) for l in r0)
                        r_invalid = sum(l.get('invalid_calls', 0) for l in r0)
                        r_required = sum(l.get('required_calls', 0) for l in r0)
                        r_invalid_mem = sum(l.get('invalid_memory', 0) for l in r0)
                        r_required_mem = sum(l.get('required_memory', 0) for l in r0)
                        r_exec_time = max((l.get('execution_time') or 0) for l in r0) or None
                    else:
                        r_total_mem = r0.get('total_memory', 0)
                        r_calls = r0.get('total_allocation_calls', 0)
                        r_invalid = r0.get('invalid_calls', 0)
                        r_required = r0.get('required_calls', 0)
                        r_invalid_mem = r0.get('invalid_memory', 0)
                        r_required_mem = r0.get('required_memory', 0)
                        r_exec_time = r0.get('execution_time')

                    mem_b, mem_k, mem_m = format_size(r_total_mem)
                    inv_b, inv_k, inv_m = format_size(r_invalid_mem)
                    req_b, req_k, req_m = format_size(r_required_mem)
                    time_s, time_ms = format_time(r_exec_time)

                    f.write(f"      Total allocated memory (rank 0): {mem_b} bytes ({mem_k} KB, {mem_m} MB)\n")
                    f.write(f"      Allocation calls (rank 0): {r_calls}\n")
                    f.write(f"      Invalid calls (rank 0): {r_invalid} (memory={inv_b} bytes ({inv_k} KB, {inv_m} MB))\n")
                    f.write(f"      Required calls (rank 0): {r_required} (memory={req_b} bytes ({req_k} KB, {req_m} MB))\n")
                    if r_exec_time is not None:
                        f.write(f"      Execution time (rank 0): {time_s} seconds ({time_ms} ms)\n")

def generate_summary_report(all_results, output_file):
    """Generate comprehensive summary report"""
    with open(output_file, 'w') as f:
        f.write("HPC Test Suite Analysis Summary\n")
        f.write("=" * 50 + "\n\n")

        for test_case in TEST_CASES:
            if test_case not in all_results:
                continue

            f.write(f"Test Case: {test_case}\n")
            f.write("-" * 30 + "\n")

            results = all_results[test_case]

            # Binary sizes
            f.write("Binary Sizes:\n")
            for config in [BUILD_SLICING_OFF, BUILD_SLICING_ON]:
                if config in results and 'binary_size' in results[config]:
                    size_bytes, size_kb, size_mb = format_size(results[config]['binary_size'])
                    config_name = "Slicing OFF" if "off" in config else "Slicing ON"
                    f.write(f"  {config_name}: {size_bytes} bytes ({size_kb} KB, {size_mb} MB)\n")

            # LLVM Module lines
            f.write("\nLLVM Module Lines:\n")
            if BUILD_SLICING_OFF in results:
                orig_lines = results[BUILD_SLICING_OFF].get('original_module_lines', 'N/A')
                f.write(f"  Original module: {orig_lines}\n")

            if BUILD_SLICING_ON in results and 'slice_module_lines' in results[BUILD_SLICING_ON]:
                slice_lines = results[BUILD_SLICING_ON]['slice_module_lines']
                f.write(f"  Slice module: {slice_lines}\n")

            # Slicing time
            if BUILD_SLICING_ON in results and 'slicing_duration' in results[BUILD_SLICING_ON]:
                slicing_time = results[BUILD_SLICING_ON]['slicing_duration']
                time_sec, time_ms = format_time(slicing_time)
                f.write(f"\nSlicing Time: {time_sec} seconds ({time_ms} ms)\n")

            # Execution times and allocation data
            f.write("\nExecution Data:\n")
            for config in [BUILD_SLICING_OFF, BUILD_SLICING_ON]:
                if config not in results:
                    continue

                config_name = "Slicing OFF" if "off" in config else "Slicing ON"
                f.write(f"  {config_name}:\n")

                # Use helper to write aggregated and per-rank data
                _write_agg_and_per_rank(f, config_name, results[config])

            f.write("\n" + "=" * 50 + "\n\n")

def main():
    """Main function"""
    results_dir = "../../cluster_results/NEW_alloc_tracking_results"

    if not os.path.exists(results_dir):
        print(f"Error: Results directory '{results_dir}' does not exist!")
        return

    all_results = {}

    # Analyze each test case
    for test_case in TEST_CASES:
        print(f"Analyzing test case: {test_case}")
        results = analyze_test_case(test_case, results_dir)
        all_results[test_case] = results

    # Generate summary report
    output_file = "analysis_summary.txt"
    generate_summary_report(all_results, output_file)
    print(f"\nAnalysis complete! Summary written to: {output_file}")

if __name__ == "__main__":
    main()