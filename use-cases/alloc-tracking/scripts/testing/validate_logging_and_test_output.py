import os
import re


# This script will compare the logging files in folder pairs for individual allocation sizes.
# The test will pass if each allocation in the sliced version matches the corresponding
# allocation in the non-sliced version. Invalid allocations are skipped but counted.


# coloring from: https://stackoverflow.com/questions/287871/how-do-i-print-colored-text-to-the-terminal
class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

FOLDER_PAIRS = [
    ("build_debug_slicing_off_logging_on", "build_debug_slicing_on_logging_on"),
    ("build_release_slicing_off_logging_on", "build_release_slicing_on_logging_on")
]

ALLOC_PATTERN = re.compile(r"allocation size (\d+)")
CALLOC_PATTERN = re.compile(r"allocation size (\d+)\([^)]*\)")
VALID_PATTERN = re.compile(r"valid (true|false)")

def process_file_lines(file_path):
    """Returns list of (size, is_valid) tuples for each allocation line"""
    lines_data = []
    with open(file_path, 'r') as f:
        for line in f:
            size = None
            is_valid = True  # Default to true if not specified
            
            # Check for validity
            valid_match = VALID_PATTERN.search(line)
            if valid_match:
                is_valid = valid_match.group(1) == "true"
            
            # Check for calloc pattern first (more specific)
            size_match = CALLOC_PATTERN.search(line)
            if not size_match:
                size_match = ALLOC_PATTERN.search(line)
            
            if size_match:
                size = int(size_match.group(1))
                lines_data.append((size, is_valid))
    
    return lines_data

def compare_allocations(f1_lines, f2_lines, filename):
    """Compare individual allocations between two files"""
    if len(f1_lines) != len(f2_lines):
        return False, f"Different number of allocations ({len(f1_lines)} vs {len(f2_lines)}) in {filename}", 0, 0, 0
    
    total_allocations = len(f1_lines)
    invalid_count = 0
    valid_compared = 0
    mismatches = []
    
    for i, ((size1, valid1), (size2, valid2)) in enumerate(zip(f1_lines, f2_lines)):
        # Count invalid allocations (invalid in either file)
        if not valid1 or not valid2:
            invalid_count += 1
            continue
        
        # Compare valid allocations
        valid_compared += 1
        if size1 != size2:
            mismatches.append(f"    Allocation {i+1}: {size1} vs {size2}")
    
    success = len(mismatches) == 0
    
    if success:
        result_msg = f"All {valid_compared} valid allocations match in {filename}"
        if invalid_count > 0:
            result_msg += f" ({invalid_count} invalid allocations skipped)"
    else:
        result_msg = f"Found {len(mismatches)} mismatches in {filename}:\n" + "\n".join(mismatches)
        if invalid_count > 0:
            result_msg += f"\n    ({invalid_count} invalid allocations skipped)"
    
    return success, result_msg, total_allocations, invalid_count, valid_compared

def compare_test_cases(folder_1, folder_2):
    base_path = "tests"
    test_cases = set()
    
    # collect test cases
    for root, dirs, files in os.walk(os.path.join(folder_1, base_path)):
        if "_allocation_log_" in root:
            continue
        for file in files:
            if "_allocation_log_" in file:
                test_case = os.path.relpath(root, os.path.join(folder_1, base_path))
                test_cases.add(test_case)
    
    # compare test cases
    results = {}
    results_stats = {}
    
    for test_case in sorted(test_cases):
        folder_1_files = []
        folder_2_files = []
        
        test_path = os.path.join(base_path, test_case)
        for folder in [folder_1, folder_2]:
            files = []
            for file in sorted(os.listdir(os.path.join(folder, test_path))):
                if "_allocation_log_" in file:
                    files.append(os.path.join(folder, test_path, file))
            if folder == folder_1:
                folder_1_files = files
            else:
                folder_2_files = files
        
        # Check if both runs have the same number of files
        if len(folder_1_files) != len(folder_2_files):
            results[test_case] = f"FAIL: different number of logging files ({len(folder_1_files)} vs {len(folder_2_files)})"
            results_stats[test_case] = {"total": 0, "invalid": 0, "compared": 0, "files": 0}
            continue
        
        # compare the file pairs individually
        all_match = True
        file_comparisons = []
        total_stats = {"total": 0, "invalid": 0, "compared": 0, "files": len(folder_1_files)}
        
        for f1, f2 in zip(folder_1_files, folder_2_files):
            f1_lines = process_file_lines(f1)
            f2_lines = process_file_lines(f2)
            
            success, msg, total_allocs, invalid_count, compared_count = compare_allocations(
                f1_lines, f2_lines, os.path.basename(f1)
            )
            
            total_stats["total"] += total_allocs
            total_stats["invalid"] += invalid_count
            total_stats["compared"] += compared_count
            
            if success:
                file_comparisons.append(f"{bcolors.OKGREEN}PASS: {msg}{bcolors.ENDC}")
            else:
                file_comparisons.append(f"{bcolors.FAIL}FAIL: {msg}{bcolors.ENDC}")
                all_match = False
        
        results_stats[test_case] = total_stats
        
        if all_match:
            results[test_case] = "PASS"
        else:
            results[test_case] = "\n".join(file_comparisons)
    
    return results, results_stats

def main():
    # use colors in the console to print test results
    for folder_1, folder_2 in FOLDER_PAIRS:
        print(f"\n\tComparing {bcolors.HEADER}{folder_1}{bcolors.ENDC} vs {bcolors.HEADER}{folder_2}{bcolors.ENDC}:")
        results, results_stats = compare_test_cases(folder_1, folder_2)

        # Calculate the maximum test case name length for alignment
        max_name_length = max(len(test_case) for test_case in results.keys()) + 4 if results else 0

        for test_case, result in results.items():
            stats = results_stats.get(test_case, {"total": 0, "invalid": 0, "compared": 0, "files": 0})
            padded_name = test_case.ljust(max_name_length)
            
            # Create stats string
            stats_str = f"Files: {stats['files']}, Total allocs: {stats['total']}, Invalid: {stats['invalid']}, Compared: {stats['compared']}"
            if stats['total'] > 0:
                invalid_percent = (stats['invalid'] / stats['total']) * 100
                stats_str += f" ({invalid_percent:.1f}% invalid)"
            
            if result == "PASS":
                print(f"\t\t{bcolors.OKGREEN}PASS{bcolors.ENDC} Test Case {bcolors.OKBLUE}{padded_name}{bcolors.ENDC} {stats_str}")
            else:
                print(f"\t\t{bcolors.FAIL}FAIL{bcolors.ENDC} Test Case {bcolors.OKBLUE}{padded_name}{bcolors.ENDC} {stats_str}")
                if "\n" in result:
                    # Split the result and add extra indentation to show it belongs to the failed test case
                    lines = result.split("\n")
                    for line in lines:
                        if line.strip():  # Only print non-empty lines
                            print(f"\t\t\t{line}")
                else:
                    print(f"\t\t\t{result}")


if __name__ == "__main__":
    main()