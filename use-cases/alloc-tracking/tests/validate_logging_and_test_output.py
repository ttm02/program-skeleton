import os
import re
import argparse


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
            mismatches.append(f"    Allocation {i + 1}: {size1} vs {size2}")

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


def compare_test_case(folder_1, folder_2):
    folder_1_files = []
    folder_2_files = []

    for folder in [folder_1, folder_2]:
        files = []
        for file in sorted(os.listdir(folder)):
            if "_allocation_log_" in file:
                files.append(os.path.join(folder, file))
        if folder == folder_1:
            folder_1_files = files
        else:
            folder_2_files = files

    if len(folder_1_files) == 0:
        print("FAIL: no allocation logs to compare")
        exit(-1)

    # Check if both runs have the same number of files
    if len(folder_1_files) != len(folder_2_files):
        print("FAIL: different number of logging files")
        exit(-1)

    # compare the file pairs individually
    all_match = True
    file_comparisons = []
    total_stats = {"total": 0, "invalid": 0, "compared": 0, "files": len(folder_1_files)}

    for f1, f2 in zip(folder_1_files, folder_2_files):
        print(f1)
        print(f2)
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

    if all_match:
        print("PASS")
        exit(0)
    else:
        print("FAIL:")
        print("\n".join(file_comparisons))
        exit(-1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("folder_1")
    parser.add_argument("folder_2")
    args = parser.parse_args()
    # use colors in the console to print test results
    compare_test_case(args.folder_1, args.folder_2)


if __name__ == "__main__":
    main()
