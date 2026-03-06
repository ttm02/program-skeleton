import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns

# Set up the plotting style for professional appearance
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")

# Parse the log data
# Original program data
original_start = 1757074914.073536
original_end = 1757075279.502218
original_execution_time = original_end - original_start

# Original program allocations (timestamps in nanoseconds)
original_allocations = [
    (1757074914097631352, 5228704),    # 50276 * 104
    (1757074914097800930, 3217600),    # 50275 * 64  
    (1757074914097899389, 1749024768)  # 27328512 * 64
]

# Sliced program data
sliced_start = 1757075907.845841
sliced_end = 1757075911.559075
sliced_execution_time = sliced_end - sliced_start

# Sliced program allocations (timestamps in nanoseconds)
sliced_allocations = [
    (1757075907849011545, 5228704),    # 50276 * 104
    (1757075907849142900, 3217600),    # 50275 * 64
    (1757075907849219181, 1749024768)  # 27328512 * 64
]

# Slicing time
slicing_time = 2.360128

# Calculate total time including slicing
total_time_with_slicing = slicing_time + sliced_execution_time

print("Execution Time Analysis:")
print("=" * 50)
print(f"Original program execution time: {original_execution_time:.3f} seconds")
print(f"Slicing time: {slicing_time:.3f} seconds")
print(f"Sliced program execution time: {sliced_execution_time:.3f} seconds")
print(f"Total time (slicing + execution): {total_time_with_slicing:.3f} seconds")
print(f"Speedup factor: {original_execution_time / sliced_execution_time:.2f}×")
print(f"Total speedup factor: {original_execution_time / total_time_with_slicing:.2f}×")

# ---------------------------
# Plot 1: Execution Time Comparison (Reordered)
# ---------------------------
fig, ax = plt.subplots(1, 1, figsize=(12, 8))

# Reordered categories and corresponding times/colors
categories = ['Original\nExecution Time', 'Slice Total Time\n(Slicing + Execution Time)', 'Slicing Time', 'Slice\nExecution Time']
times = [original_execution_time, total_time_with_slicing, slicing_time, sliced_execution_time]
colors = ['#4169E1', '#2E8B57', '#DC143C', '#FFA500']

bars = ax.bar(categories, times, color=colors, alpha=0.7, edgecolor='black', linewidth=1)

# Add value labels on bars
for bar, time_val in zip(bars, times):
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width() / 2., height + max(times) * 0.01,
            f'{time_val:.3f}s', ha='center', va='bottom', fontweight='bold', fontsize=12)

# Calculate separator position: halfway between the "Original Program" and "Total Time" bars
sep_x = (bars[0].get_x() + bars[0].get_width() / 2. +
         bars[1].get_x() + bars[1].get_width() / 2.) / 2.0

# Add a black dotted separator line
ax.axvline(x=sep_x, color='black', linestyle='--', linewidth=1.5, alpha=0.8, zorder=2)

ax.set_ylabel('Execution Time (seconds)', fontsize=14, fontweight='bold')
ax.set_title('SPEC CPU Benchmark 429.mcf: Execution Time and Slicing Overhead Comparison',
             fontsize=16, fontweight='bold', pad=20)
ax.grid(True, alpha=0.3, axis='y')

# Ensure y-axis starts at 0 and has headroom
ax.set_ylim(0, max(times) * 1.15)

plt.tight_layout()
#plt.savefig('execution_time_comparison.svg', format='svg', dpi=300, bbox_inches='tight')
#plt.savefig('execution_time_comparison.pdf')
plt.savefig('execution_time_comparison.png')
plt.close()



# ---------------------------
# Helper: convert allocation timestamps to relative seconds (same as original script)
# ---------------------------
def convert_allocations_to_relative_time(allocations, program_start_seconds):
    relative_times = []
    sizes = []
    for timestamp_ns, size in allocations:
        timestamp_s = timestamp_ns / 1e9
        relative_time = timestamp_s - program_start_seconds
        relative_times.append(relative_time)
        sizes.append(size / (1e+6))  # Convert bytes to MB
    return relative_times, sizes

# Original program timeline
orig_times, orig_sizes = convert_allocations_to_relative_time(original_allocations, original_start)
# Add program end point for plotting a end marker
orig_times.append(original_execution_time)
orig_sizes.append(0)

# Sliced program timeline
sliced_times, sliced_sizes = convert_allocations_to_relative_time(sliced_allocations, sliced_start)
# Add program end point
sliced_times.append(sliced_execution_time)
sliced_sizes.append(0)

# ---------------------------
# Plot 2: Combined Memory Allocation Timeline
# - single plot combining both original and sliced allocations
# - legend moved outside plot and boxed
# - use the longer program's total time as baseline, mark sliced end with dotted vertical line
# - if the longer timeline is much larger than the shorter, apply a soft "cutoff" to avoid huge empty space
# ---------------------------
fig, ax = plt.subplots(figsize=(16, 6))

# Plot allocations (exclude the appended program-end points for the scatter)
ax.scatter(orig_times[:-1], orig_sizes[:-1], color='#DC143C', s=120, alpha=0.85,
           label='Original: memory allocations', zorder=3, marker='o')
ax.scatter(sliced_times[:-1], sliced_sizes[:-1], color='#2E8B57', s=120, alpha=0.95,
           label='Sliced: memory allocations', zorder=4, marker='X')

# Baseline and program end markers
ax.axhline(y=0, color='black', linestyle='-', alpha=0.3)

# Mark original program end (dashed) and sliced program end (dotted)
ax.axvline(x=original_execution_time, color='#8B0000', linestyle='--', alpha=0.7,
           linewidth=1.5, label=f'Original End ({original_execution_time:.1f}s)')
ax.axvline(x=sliced_execution_time, color='#2E8B57', linestyle=':', alpha=0.9,
           linewidth=2, label=f'Sliced End ({sliced_execution_time:.3f}s)')

# Add allocation labels (annotate points)
for time_pt, size_mb in zip(orig_times[:-1], orig_sizes[:-1]):
    ax.annotate(f'{size_mb:.1f} MB\n@{time_pt:.4f}s', (time_pt, size_mb),
                textcoords="offset points", xytext=(8, 8),
                ha='left', fontsize=9, fontweight='bold',
                bbox=dict(boxstyle="round,pad=0.3", facecolor='white', alpha=0.8))

for time_pt, size_mb in zip(sliced_times[:-1], sliced_sizes[:-1]):
    ax.annotate(f'{size_mb:.1f} MB\n@{time_pt:.4f}s', (time_pt, size_mb),
                textcoords="offset points", xytext=(8, -28),
                ha='left', fontsize=9, fontweight='bold',
                bbox=dict(boxstyle="round,pad=0.3", facecolor='white', alpha=0.8))

ax.set_xlabel('Time from Program Start (seconds)', fontsize=12, fontweight='bold')
ax.set_ylabel('Allocation Size (MB)', fontsize=12, fontweight='bold')
ax.set_title('Combined Memory Allocation Timeline (Original vs Sliced)', fontsize=14, fontweight='bold')

# Move legend outside the plot on the right and put a clear boxed frame around it
legend = ax.legend(loc='upper left', bbox_to_anchor=(1.02, 1.0), borderaxespad=0, fontsize=10, frameon=True)
leg_frame = legend.get_frame()
leg_frame.set_facecolor('white')
leg_frame.set_edgecolor('black')
leg_frame.set_alpha(0.95)

ax.grid(True, alpha=0.3)

# Determine display limits to avoid huge empty whitespace:
longer_time = max(original_execution_time, sliced_execution_time)
shorter_time = min(original_execution_time, sliced_execution_time)

# If the longer timeline is much larger than the shorter one, apply a "soft cutoff" so the left-side (where allocations are)
# remains readable and the long empty tail is truncated. This keeps the timeline informative while noting the truncation.
cutoff_applied = False
if longer_time > max(5.0, 6.0 * shorter_time):  # threshold: 6x longer (and >5s)
    cutoff_applied = True
    # Keep about 3x of the shorter program visible (or at least some sensible minimum)
    display_limit = max(shorter_time * 3.0, shorter_time + 1.0)
else:
    display_limit = longer_time

# Add small left margin and set x limits
x_min = min(0.0, min(orig_times[0], sliced_times[0])) - 0.2
ax.set_xlim(x_min, display_limit * 1.02)

# If a cutoff was applied, visually mark the truncation and annotate the true full original end time
if cutoff_applied:
    # draw a small diagonal break marker in axis coordinates (looks like //) to indicate truncation
    ax.plot([0.92, 0.96], [0.02, 0.08], transform=ax.transAxes, color='black', clip_on=False, linewidth=1.2)
    ax.plot([0.92, 0.96], [0.08, 0.02], transform=ax.transAxes, color='black', clip_on=False, linewidth=1.2)
    # place a note about full runtime near the top-right of the axes (in axes coords)
    ax.text(0.99, 0.92, f'Full original end: {original_execution_time:.1f}s', transform=ax.transAxes,
            ha='right', va='top', fontsize=9, bbox=dict(boxstyle="round,pad=0.3", facecolor='white', alpha=0.8))
    # draw a dashed indicator at the truncation edge to show "cut here"
    ax.axvline(x=display_limit, color='gray', linestyle='--', linewidth=1.0, alpha=0.6)
    ax.text(display_limit, max(orig_sizes + sliced_sizes) * 0.95, '... truncated', ha='right', va='top',
            fontsize=9, color='gray', bbox=dict(boxstyle="round,pad=0.2", facecolor='white', alpha=0.8))
else:
    # If no cutoff, make sure the full original line is visible (already drawn above)
    pass

# Ensure readability: set y limits with some headroom
max_alloc_size = max(max(orig_sizes[:-1]) if orig_sizes[:-1] else 0,
                     max(sliced_sizes[:-1]) if sliced_sizes[:-1] else 0, 1.0)
ax.set_ylim(0, max_alloc_size * 1.25)

plt.tight_layout(rect=(0, 0, 0.88, 1.0))  # leave space on the right for the legend
#plt.savefig('combined_memory_allocation_timeline.svg', format='svg', dpi=300, bbox_inches='tight')
#plt.savefig('combined_memory_allocation_timeline.pdf')
plt.close()

# ---------------------------
# Plot 3: Side-by-side allocation timing comparison with different scales (unchanged)
# ---------------------------
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 8))

# Left plot: Original program (longer timeline)
bars1 = ax1.bar(range(len(orig_sizes[:-1])), orig_sizes[:-1],
               color='#DC143C', alpha=0.7, edgecolor='black', linewidth=1)
ax1.set_xlabel('Allocation Order', fontsize=12, fontweight='bold')
ax1.set_ylabel('Allocation Size (MB)', fontsize=12, fontweight='bold')
ax1.set_title(f'Original Program\nTotal Runtime: {original_execution_time:.1f}s',
             fontsize=14, fontweight='bold')
ax1.set_xticks(range(len(orig_sizes[:-1])))
ax1.set_xticklabels([f'Alloc {i+1}\n@{t:.4f}s' for i, t in enumerate(orig_times[:-1])])

# Add value labels on bars
for bar, size in zip(bars1, orig_sizes[:-1]):
    height = bar.get_height()
    ax1.text(bar.get_x() + bar.get_width()/2., height + max(orig_sizes[:-1]) * 0.02,
            f'{size:.1f} MB', ha='center', va='bottom', fontweight='bold', fontsize=10)

# Right plot: Sliced program (shorter timeline)
bars2 = ax2.bar(range(len(sliced_sizes[:-1])), sliced_sizes[:-1],
               color='#2E8B57', alpha=0.7, edgecolor='black', linewidth=1)
ax2.set_xlabel('Allocation Order', fontsize=12, fontweight='bold')
ax2.set_ylabel('Allocation Size (MB)', fontsize=12, fontweight='bold')
ax2.set_title(f'Sliced Program\nTotal Runtime: {sliced_execution_time:.1f}s',
             fontsize=14, fontweight='bold')
ax2.set_xticks(range(len(sliced_sizes[:-1])))
ax2.set_xticklabels([f'Alloc {i+1}\n@{t:.4f}s' for i, t in enumerate(sliced_times[:-1])])

# Add value labels on bars
for bar, size in zip(bars2, sliced_sizes[:-1]):
    height = bar.get_height()
    ax2.text(bar.get_x() + bar.get_width()/2., height + max(sliced_sizes[:-1]) * 0.02,
            f'{size:.1f} MB', ha='center', va='bottom', fontweight='bold', fontsize=10)

# Keep same y-scale for comparison
max_size = max(max(orig_sizes[:-1]), max(sliced_sizes[:-1]))
ax1.set_ylim(0, max_size * 1.15)
ax2.set_ylim(0, max_size * 1.15)

ax1.grid(True, alpha=0.3, axis='y')
ax2.grid(True, alpha=0.3, axis='y')

plt.tight_layout()
#plt.savefig('allocation_comparison.svg', format='svg', dpi=300, bbox_inches='tight')
#plt.savefig('allocation_comparison.pdf')
plt.close()

# ---------------------------
# Memory allocation summary (same as original script)
# ---------------------------
print("\nMemory Allocation Analysis:")
print("=" * 50)
total_orig_memory = sum(size for _, size in original_allocations) / (1e+6)
total_sliced_memory = sum(size for _, size in sliced_allocations) / (1e+6)

print(f"Original program total allocations: {total_orig_memory:.1f} MB")
print(f"Sliced program total allocations: {total_sliced_memory:.1f} MB")
print(f"Memory allocation difference: {abs(total_orig_memory - total_sliced_memory):.1f} MB")

# Allocation timing analysis
orig_allocation_duration = (original_allocations[-1][0] - original_allocations[0][0]) / 1e9
sliced_allocation_duration = (sliced_allocations[-1][0] - sliced_allocations[0][0]) / 1e9

print(f"Original program allocation phase duration: {orig_allocation_duration:.6f} seconds")
print(f"Sliced program allocation phase duration: {sliced_allocation_duration:.6f} seconds")
print(f"Allocation phase speedup: {orig_allocation_duration / sliced_allocation_duration:.2f}×")

# Calculate percentage of time spent in allocation phase
orig_alloc_percentage = (orig_allocation_duration / original_execution_time) * 100
sliced_alloc_percentage = (sliced_allocation_duration / sliced_execution_time) * 100

print(f"Original program: {orig_alloc_percentage:.6f}% of time spent allocating")
print(f"Sliced program: {sliced_alloc_percentage:.6f}% of time spent allocating")
print("=" * 50)
