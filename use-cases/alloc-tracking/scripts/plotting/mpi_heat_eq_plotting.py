import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns

# Set up the plotting style for professional appearance
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")

# Data with slicing enabled
slicing_enabled = [
    (18, 200, 1757247957.816800, 1757247958.355435),
    (102, 2000, 1757247958.646860, 1757247959.228195),
    (130, 4000, 1757247959.541783, 1757247960.098914),
    (194, 6000, 1757247960.408815, 1757247960.964277),
    (224, 6000, 1757247961.258226, 1757247961.846696),
    (258, 8000, 1757247962.164079, 1757247962.780986)
]

# Data with slicing disabled
slicing_disabled = [
    (18, 200, 1757247902.047027, 1757247902.619792),
    (102, 2000, 1757247902.922811, 1757247904.132579),
    (130, 4000, 1757247904.471946, 1757247906.696820),
    (194, 6000, 1757247907.316011, 1757247915.119807),
    (224, 6000, 1757247915.512062, 1757247928.742823),
    (258, 8000, 1757247929.071272, 1757247957.306674)
]

# RSS memory data (in KB)
rss_original_kb = [132600, 146264, 158200, 216144, 258640, 324364]
rss_slice_kb = [129608, 129536, 129728, 129528, 129688, 129420]

# Calculate execution times and extract parameters
def process_data(data):
    n_values = []
    nt_values = []
    exec_times = []
    labels = []
    for n, nt, start, end in data:
        n_values.append(n)
        nt_values.append(nt)
        exec_times.append(end - start)
        labels.append(f"n={n}, nt={nt}")
    return n_values, nt_values, exec_times, labels

n_enabled, nt_enabled, times_enabled, labels_enabled = process_data(slicing_enabled)
n_disabled, nt_disabled, times_disabled, labels_disabled = process_data(slicing_disabled)

# Plot 1: Line Plot for Execution Time Comparison
fig, ax = plt.subplots(1, 1, figsize=(14, 8))
x_pos = range(len(times_enabled))
# Create line plots with markers
line2 = ax.plot(x_pos, times_disabled, marker='s', linewidth=3, markersize=8, label='Original', color='#4169E1', markerfacecolor='white', markeredgewidth=2, markeredgecolor='#4169E1')
line1 = ax.plot(x_pos, times_enabled, marker='o', linewidth=3, markersize=8, label='Slice', color='#2E8B57', markerfacecolor='white', markeredgewidth=2, markeredgecolor='#2E8B57')

# Add value labels on points with improved positioning
for i, (x, y) in enumerate(zip(x_pos, times_enabled)):
    # Move first two configurations higher to avoid overlap
    if i == 0: offset_y = 10
    elif i == 1: offset_y = 20
    elif i == 2: offset_y = 7
    else: offset_y = 10
    ax.annotate(f'{y:.2f}s', (x, y), textcoords="offset points", xytext=(0, offset_y), ha='center', fontweight='bold', fontsize=9, color='#2E8B57')

for i, (x, y) in enumerate(zip(x_pos, times_disabled)):
    offset_x = 0
    # Move first two configurations higher to avoid overlap
    if i == 0: offset_y = 20
    elif i == 1: offset_y = 25
    elif i == 4: offset_x = -10
    else: offset_y = 15
    ax.annotate(f'{y:.2f}s', (x, y), textcoords="offset points", xytext=(offset_x, offset_y), ha='center', fontweight='bold', fontsize=9, color='#4169E1')

ax.set_xlabel('Input Configuration', fontsize=14, fontweight='bold')
ax.set_ylabel('Execution Time (seconds)', fontsize=14, fontweight='bold')
ax.set_title('MPI Heat Equation Simulation: Execution Time Comparison', fontsize=16, fontweight='bold', pad=20)
ax.set_xticks(x_pos)
ax.set_xticklabels(labels_enabled, rotation=45, ha='right', fontsize=11)
ax.legend(fontsize=12, framealpha=0.9)
ax.grid(True, alpha=0.3)
# Set y-axis to start from 0 for better comparison
ax.set_ylim(0, max(max(times_enabled), max(times_disabled)) * 1.1)
plt.tight_layout()
#plt.savefig('execution_time_comparison_line.svg', format='svg', dpi=300, bbox_inches='tight')
plt.savefig('execution_time_comparison_line.png')
plt.close()

# Plot 2: Performance Improvement (Speedup Factor) - keeping as bar chart
fig, ax = plt.subplots(1, 1, figsize=(14, 8))
speedup_factors = [t_disabled / t_enabled for t_disabled, t_enabled in zip(times_disabled, times_enabled)]
bars = ax.bar(range(len(speedup_factors)), speedup_factors, color=['#404040' if x > 1 else '#4169E1' for x in speedup_factors], alpha=0.7, edgecolor='black', linewidth=1) #'#4169E1'

# Add value labels on bars
for i, (bar, value) in enumerate(zip(bars, speedup_factors)):
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2., height + 0.2, f'{value:.1f}×', ha='center', va='bottom', fontweight='bold', fontsize=12)

ax.axhline(y=1, color='red', linestyle='--', alpha=0.7, linewidth=2, label='No Improvement (1×)')
ax.set_xlabel('Input Configuration', fontsize=14, fontweight='bold')
ax.set_ylabel('Speedup Factor (Original/Sliced)', fontsize=14, fontweight='bold')
ax.set_title('MPI Heat Equation Simulation: Execution Time Speedup', fontsize=16, fontweight='bold', pad=20)
ax.set_xticks(range(len(speedup_factors)))
ax.set_xticklabels(labels_enabled, rotation=45, ha='right', fontsize=11)
ax.grid(True, alpha=0.3, axis='y')
ax.legend(fontsize=12, framealpha=0.9)
plt.tight_layout()
#plt.savefig('performance_improvement.svg', format='svg', dpi=300, bbox_inches='tight')
plt.savefig('performance_improvement.png')
plt.close()

# Memory allocation data for each configuration (original total allocations)
allocation_data = {
    (18, 200): [46656, 8000, 8000, 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 8000],
    (102, 2000): [8489664, 1124864, 1124864, 21632, 21632, 21632, 21632, 21632, 21632, 21632, 21632, 21632, 21632, 21632, 21632, 1124864],
    (130, 4000): [17576000, 2299968, 2299968, 34848, 34848, 34848, 34848, 34848, 34848, 34848, 34848, 34848, 34848, 34848, 34848, 2299968],
    (194, 6000): [58411072, 7529536, 7529536, 76832, 76832, 76832, 76832, 76832, 76832, 76832, 76832, 76832, 76832, 76832, 76832, 7529536],
    (224, 6000): [89915392, 11543176, 11543176, 102152, 102152, 102152, 102152, 102152, 102152, 102152, 102152, 102152, 102152, 102152, 102152, 11543176],
    (258, 8000): [137388096, 17576000, 17576000, 135200, 135200, 135200, 135200, 135200, 135200, 135200, 135200, 135200, 135200, 135200, 135200, 17576000]
}

# Required allocations data (only those with "required to allocate true")
# Based on the logged data analysis
required_allocation_data = {
    (18, 200): [46656], # Only the main function allocation is required
    (102, 2000): [8489664], # Only the main function allocation is required
    (130, 4000): [17576000], # Only the main function allocation is required
    (194, 6000): [58411072], # Only the main function allocation is required
    (224, 6000): [89915392], # Only the main function allocation is required
    (258, 8000): [137388096] # Only the main function allocation is required
}

# Calculate total allocations and convert to appropriate units
total_allocations_bytes = []
required_allocations_bytes = []
config_labels = []
for (n, nt) in allocation_data.keys():
    # Original total allocations
    total_bytes = sum(allocation_data[(n, nt)])
    total_allocations_bytes.append(total_bytes)
    # Required allocations (slice)
    required_bytes = sum(required_allocation_data[(n, nt)])
    required_allocations_bytes.append(required_bytes)
    config_labels.append(f"n={n}, nt={nt}")

# Convert bytes to MB for better readability
total_allocations_mb = [bytes_val / (1e+6) for bytes_val in total_allocations_bytes]
required_allocations_mb = [bytes_val / (1e+6) for bytes_val in required_allocations_bytes]

# Plot 3: Memory Allocation Analysis (Updated with both lines)
fig, ax = plt.subplots(1, 1, figsize=(14, 8))
x_pos = range(len(total_allocations_mb))
# Create line plots with markers for both memory allocations
line1 = ax.plot(x_pos, total_allocations_mb, marker='D', linewidth=3, markersize=10, label='Original', color='#4169E1', markerfacecolor='white', markeredgewidth=2, markeredgecolor='#4169E1')
line2 = ax.plot(x_pos, required_allocations_mb, marker='o', linewidth=3, markersize=10, label='Slice', color='#2E8B57', markerfacecolor='white', markeredgewidth=2, markeredgecolor='#2E8B57')

# Add value labels on points for original allocations
for i, (x, y) in enumerate(zip(x_pos, total_allocations_mb)):
    offset_x = 0
    if i == 3: offset_x = -10
    elif i == 2: offset_x = -15
    elif i == 4: offset_x = -15
    else: offset_y = -15
    ax.annotate(f'{y:.1f} MB', (x, y), textcoords="offset points", xytext=(offset_x,12), ha='center', fontweight='bold', fontsize=10, color='#4169E1')

# Add value labels on points for required allocations with improved positioning
for i, (x, y) in enumerate(zip(x_pos, required_allocations_mb)):
    offset_x = 0
    # Move last three configurations above the points
    if i == 3: offset_y = 12
    elif i == 5: offset_y = 15
    elif i == 4: offset_y = 15
    else: offset_y = -15
    ax.annotate(f'{y:.1f} MB', (x, y), textcoords="offset points", xytext=(offset_x, offset_y), ha='center', fontweight='bold', fontsize=10, color='#2E8B57')

ax.set_xlabel('Input Configuration', fontsize=14, fontweight='bold')
ax.set_ylabel('Total Dynamic Memory (MB)', fontsize=14, fontweight='bold')
ax.set_title('3D Heat Equation Simulation: Total Dynamic Memory with increasing Inputs', fontsize=16, fontweight='bold', pad=20)
ax.set_xticks(x_pos)
ax.set_xticklabels(config_labels, rotation=45, ha='right', fontsize=11)
ax.legend(fontsize=12, framealpha=0.9)
ax.grid(True, alpha=0.3)
# Set y-axis to start from 0 with extra space at bottom for labels
ax.set_ylim(-max(total_allocations_mb) * 0.05, max(total_allocations_mb) * 1.1)
plt.tight_layout()
#plt.savefig('memory_allocation_analysis.svg', format='svg', dpi=300, bbox_inches='tight')
plt.savefig('memory_allocation_analysis.png')
plt.close()

# Plot 4: RSS Memory Usage Comparison
fig, ax = plt.subplots(1, 1, figsize=(14, 8))
# Convert KB to MB (divide by 1000 as requested)
rss_original_mb = [kb / 1000 for kb in rss_original_kb]
rss_slice_mb = [kb / 1000 for kb in rss_slice_kb]
x_pos = range(len(rss_original_mb))
# Create line plots with markers for RSS memory usage
line1 = ax.plot(x_pos, rss_original_mb, marker='D', linewidth=3, markersize=10, label='Original', color='#4169E1', markerfacecolor='white', markeredgewidth=2, markeredgecolor='#4169E1')
line2 = ax.plot(x_pos, rss_slice_mb, marker='o', linewidth=3, markersize=10, label='Slice', color='#2E8B57', markerfacecolor='white', markeredgewidth=2, markeredgecolor='#2E8B57')

# Add value labels on points for original RSS
for i, (x, y) in enumerate(zip(x_pos, rss_original_mb)):
    offset_x = 0
    offset_y = 12
    if i == 4 or i == 5: # Last two points might need adjustment
        offset_x = -10
    ax.annotate(f'{y:.1f} MB', (x, y), textcoords="offset points", xytext=(offset_x, offset_y), ha='center', fontweight='bold', fontsize=10, color='#4169E1')

# Add value labels on points for slice RSS
for i, (x, y) in enumerate(zip(x_pos, rss_slice_mb)):
    offset_x = 0
    offset_y = -15
    ax.annotate(f'{y:.1f} MB', (x, y), textcoords="offset points", xytext=(offset_x, offset_y), ha='center', fontweight='bold', fontsize=10, color='#2E8B57')

ax.set_xlabel('Input Configuration', fontsize=14, fontweight='bold')
ax.set_ylabel('Maximum RSS (MB)', fontsize=14, fontweight='bold')
ax.set_title('3D Heat Equation Simulation: Maximum RSS with increasing Inputs', fontsize=16, fontweight='bold', pad=20)
ax.set_xticks(x_pos)
ax.set_xticklabels(config_labels, rotation=45, ha='right', fontsize=11)
ax.legend(fontsize=12, framealpha=0.9)
ax.grid(True, alpha=0.3)
# Set y-axis to start from 0 with extra space at bottom for labels
ax.set_ylim(-max(rss_original_mb) * 0.05, max(rss_original_mb) * 1.1)
plt.tight_layout()
#plt.savefig('rss_memory_comparison.svg', format='svg', dpi=300, bbox_inches='tight')
plt.savefig('rss_memory_comparison.png')
plt.close()

# --- START: NEW PLOT ---
# Plot 5: Bar chart for single large application RSS comparison
# Data for the large single application RSS comparison
large_app_rss_original_kb = 1721328
large_app_rss_slice_kb = 28376

# Convert KB to MB for consistency
large_app_rss_original_mb = large_app_rss_original_kb / 1000
large_app_rss_slice_mb = large_app_rss_slice_kb / 1000

# Create the plot - keep it reasonably wide
fig, ax = plt.subplots(figsize=(10, 7))

labels = ['Original', 'Slice']
rss_values_mb = [large_app_rss_original_mb, large_app_rss_slice_mb]
colors = ['#4169E1', '#2E8B57']

# Use numeric positions with tighter spacing
x = np.linspace(0, 1, len(labels))  # packs bars closer than default
bars = ax.bar(x, rss_values_mb, color=colors, edgecolor='black',
              linewidth=1, alpha=0.7, width=0.5)

# Add value labels on top of the bars
for bar in bars:
    height = bar.get_height()
    ax.annotate(f'{height:,.1f} MB',
                xy=(bar.get_x() + bar.get_width() / 2, height),
                xytext=(0, 3),
                textcoords="offset points",
                ha='center', va='bottom', fontweight='bold', fontsize=12)

# Set plot labels and title
ax.set_ylabel('Maximum RSS (MB)', fontsize=14, fontweight='bold')
ax.set_title('SPEC CPU Benchmark 429.mcf: Maximum RSS Comparison',
             fontsize=16, fontweight='bold', pad=20)

# Fix x-axis ticks/labels to match numeric positions
ax.set_xticks(x)
ax.set_xticklabels(labels, fontsize=14)
ax.tick_params(axis='y', labelsize=11)

# Add grid and layout adjustments
ax.grid(True, alpha=0.3, axis='y')
ax.set_ylim(0, max(rss_values_mb) * 1.15)
ax.margins(x=0.15)  # optional: add some left/right margin
plt.tight_layout()

# Save the plot
plt.savefig('large_app_rss_comparison.png')
plt.close()
# --- END: NEW PLOT ---

# Calculate memory savings
memory_savings_mb = [orig - req for orig, req in zip(total_allocations_mb, required_allocations_mb)]
memory_savings_percent = [(orig - req) / orig * 100 for orig, req in zip(total_allocations_mb, required_allocations_mb)]

# Calculate RSS savings
rss_savings_mb = [orig - slice_val for orig, slice_val in zip(rss_original_mb, rss_slice_mb)]
rss_savings_percent = [(orig - slice_val) / orig * 100 for orig, slice_val in zip(rss_original_mb, rss_slice_mb)]


# Generate summary statistics
print("Performance Analysis Summary:")
print("=" * 50)
print(f"Average execution time (slicing enabled): {np.mean(times_enabled):.3f} seconds")
print(f"Average execution time (slicing disabled): {np.mean(times_disabled):.3f} seconds")
print(f"Average speedup factor: {np.mean(speedup_factors):.2f}×")
print(f"Maximum speedup achieved: {max(speedup_factors):.2f}×")
print(f"Slicing overhead negligible: {max(times_enabled) - min(times_enabled):.3f} seconds variation")
print(f"Original version time range: {max(times_disabled) - min(times_disabled):.3f} seconds")

print("\nMemory Allocation Summary:")
print("=" * 50)
print("Original Total Memory vs Slice Required Memory:")
for i, ((n, nt), total_mb, req_mb, saving_mb, saving_pct) in enumerate(
    zip(allocation_data.keys(), total_allocations_mb, required_allocations_mb, memory_savings_mb, memory_savings_percent)):
    print(f"n={n}, nt={nt}:")
    print(f" Original Total: {total_mb:.1f} MB ({total_allocations_bytes[i]:,} bytes)")
    print(f" Slice Required: {req_mb:.1f} MB ({required_allocations_bytes[i]:,} bytes)")
    print(f" Memory Saved: {saving_mb:.1f} MB ({saving_pct:.1f}%)")
    print()

print(f"Average memory savings: {np.mean(memory_savings_percent):.1f}%")
print(f"Total memory growth factor (original): {max(total_allocations_mb)/min(total_allocations_mb):.1f}×")
print(f"Total memory growth factor (slice): {max(required_allocations_mb)/min(required_allocations_mb):.1f}×")


print("\nRSS Memory Usage Summary:")
print("=" * 50)
print("Original Max RSS vs Slice Max RSS:")
for i, ((n, nt), orig_rss, slice_rss, saving_mb, saving_pct) in enumerate(
    zip(allocation_data.keys(), rss_original_mb, rss_slice_mb, rss_savings_mb, rss_savings_percent)):
    print(f"n={n}, nt={nt}:")
    print(f" Original RSS: {orig_rss:.1f} MB ({rss_original_kb[i]:,} KB)")
    print(f" Slice RSS: {slice_rss:.1f} MB ({rss_slice_kb[i]:,} KB)")
    print(f" RSS Saved: {saving_mb:.1f} MB ({saving_pct:.1f}%)")
    print()

print(f"Average RSS savings: {np.mean(rss_savings_percent):.1f}%")
print(f"RSS growth factor (original): {max(rss_original_mb)/min(rss_original_mb):.1f}×")
print(f"RSS growth factor (slice): {max(rss_slice_mb)/min(rss_slice_mb):.1f}×")
print("=" * 50)