import csv
import os
import statistics
import matplotlib
matplotlib.use('Agg')  
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

RESULTS_DIR = os.path.join(os.path.dirname(__file__), '..', 'results')

def load_csv(filename):
    path = os.path.join(RESULTS_DIR, filename)
    latencies = []
    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            latencies.append(int(row['latency_ns']) / 1000.0)  # ns → µs
    return latencies

msg_lat = load_csv('msg_results.csv')
shm_lat = load_csv('shm_results.csv')

def stats(data):
    return {
        'mean':   statistics.mean(data),
        'median': statistics.median(data),
        'stdev':  statistics.stdev(data),
        'p99':    float(np.percentile(data, 99)),
        'min':    min(data),
        'max':    max(data),
    }

ms = stats(msg_lat)
ss = stats(shm_lat)

print(f"{'Metric':<12} {'MsgPass (µs)':>14} {'ShmMem (µs)':>14}")
print("-" * 42)
for k in ['mean','median','stdev','p99','min','max']:
    print(f"{k:<12} {ms[k]:>14.2f} {ss[k]:>14.2f}")

fig, axes = plt.subplots(1, 3, figsize=(14, 5))
fig.suptitle('QNX IPC Benchmark: Message Passing vs Shared Memory',
             fontsize=14, fontweight='bold')

# Plot 1: Latency over time
ax = axes[0]
ax.plot(msg_lat[:2000], alpha=0.5, linewidth=0.4, color='#185FA5', label='MsgPass')
ax.plot(shm_lat[:2000], alpha=0.5, linewidth=0.4, color='#0F6E56', label='ShmMem')
ax.set_title('Latency over time (first 2000 iterations)')
ax.set_xlabel('Iteration')
ax.set_ylabel('Latency (µs)')
ax.legend(); ax.grid(True, alpha=0.3)

# Plot 2: Histogram comparison
ax = axes[1]
upper = max(np.percentile(msg_lat,99.5), np.percentile(shm_lat,99.5))
bins = np.linspace(0, upper, 80)
ax.hist(msg_lat, bins=bins, alpha=0.6, color='#185FA5', label='MsgPass')
ax.hist(shm_lat, bins=bins, alpha=0.6, color='#0F6E56', label='ShmMem')
ax.axvline(ms['mean'], color='#042C53', linestyle='--', linewidth=1.2)
ax.axvline(ss['mean'], color='#04342C', linestyle='--', linewidth=1.2)
ax.set_title('Latency distribution')
ax.set_xlabel('Latency (µs)')
ax.set_ylabel('Count')
ax.legend(); ax.grid(True, alpha=0.3)

# Plot 3: Summary bar chart with error bars
ax = axes[2]
labels = ['Msg Passing', 'Shared Mem']
means  = [ms['mean'], ss['mean']]
stdevs = [ms['stdev'], ss['stdev']]
colors = ['#185FA5', '#0F6E56']
bars = ax.bar(labels, means, yerr=stdevs, capsize=6,
              color=colors, alpha=0.8, width=0.4)
for bar, mean in zip(bars, means):
    ax.text(bar.get_x() + bar.get_width()/2, mean + stdevs[means.index(mean)] + 0.1,
            f'{mean:.2f} µs', ha='center', va='bottom', fontsize=10, fontweight='bold')
ax.set_title('Mean latency ± σ')
ax.set_ylabel('Latency (µs)')
ax.grid(True, axis='y', alpha=0.3)

plt.tight_layout()
out = os.path.join(RESULTS_DIR, 'qnx_ipc_benchmark.png')
plt.savefig(out, dpi=150, bbox_inches='tight')
print(f"\nChart saved: {out}")