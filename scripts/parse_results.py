
import csv
import os
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# ── paths ────────────────────────────────────────────────
SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, '..', 'results')

# Automotive ASIL-D hard deadline we draw as a red line on charts.
# 10 ms is a conservative brake-actuation budget.
# Your p99 should be well under this — that is the whole point.
ASIL_D_BUDGET_US = 10_000.0

PAYLOAD_SIZES = [8, 64, 512, 4096, 16384]
SIZE_LABELS   = ['8 B', '64 B', '512 B', '4 KB', '16 KB']

# ── colour palette ───────────────────────────────────────
C_MSG_AVG  = '#185FA5'
C_MSG_P99  = '#0C447C'
C_SHM_AVG  = '#1D9E75'
C_SHM_P99  = '#085041'
C_ASIL     = '#C0392B'
C_GRID     = '#CCCCCC'

# ── helpers ──────────────────────────────────────────────
def load_summary(fname):
    """Return dict keyed by payload_bytes with all stat columns."""
    path = os.path.join(RESULTS_DIR, fname)
    if not os.path.exists(path):
        print(f"ERROR: {path} not found. Did you copy the CSVs from QNX?")
        sys.exit(1)
    rows = {}
    with open(path) as f:
        for row in csv.DictReader(f):
            key = int(row['payload_bytes'])
            rows[key] = {k: float(v) for k, v in row.items() if k != 'payload_bytes'}
    return rows

def load_raw(fname):
    """Return list of latencies in µs from a per-size CSV."""
    path = os.path.join(RESULTS_DIR, fname)
    if not os.path.exists(path):
        return None
    lats = []
    with open(path) as f:
        for row in csv.DictReader(f):
            lats.append(int(row['latency_ns']) / 1000.0)
    return lats

def extract_col(summary, sizes, col):
    return [summary[s][col] for s in sizes]

# ── load data ────────────────────────────────────────────
msg = load_summary('msg_summary.csv')
shm = load_summary('shm_summary.csv')

sizes = PAYLOAD_SIZES   # x-axis values (numeric, for log scale)
xlabels = SIZE_LABELS

msg_avg  = extract_col(msg, sizes, 'avg_us')
msg_p99  = extract_col(msg, sizes, 'p99_us')
msg_p999 = extract_col(msg, sizes, 'p999_us')

shm_avg  = extract_col(shm, sizes, 'avg_us')
shm_p99  = extract_col(shm, sizes, 'p99_us')
shm_p999 = extract_col(shm, sizes, 'p999_us')

x = np.arange(len(sizes))  # bar positions

# ── print table to terminal ──────────────────────────────
print(f"\n{'Payload':<8} {'MSG avg':>10} {'MSG p99':>10} {'MSG p99.9':>10} "
      f"{'SHM avg':>10} {'SHM p99':>10} {'SHM p99.9':>10}")
print("─" * 72)
for i, s in enumerate(sizes):
    print(f"{SIZE_LABELS[i]:<8} "
          f"{msg_avg[i]:>9.1f}µ "
          f"{msg_p99[i]:>9.1f}µ "
          f"{msg_p999[i]:>9.1f}µ "
          f"{shm_avg[i]:>9.1f}µ "
          f"{shm_p99[i]:>9.1f}µ "
          f"{shm_p999[i]:>9.1f}µ")


fig, axes = plt.subplots(1, 2, figsize=(13, 5))
fig.suptitle(
    'QNX IPC Benchmark — Payload Sweep  |  QNX 8.0 x86-64 QEMU',
    fontsize=13, fontweight='bold', y=1.01
)

BAR_W = 0.35

for ax, metric, msg_vals, shm_vals, ylabel, title in [
    (axes[0], 'Average Latency',
     msg_avg, shm_avg,
     'Latency (µs)', 'Average latency vs payload'),
    (axes[1], 'p99 Latency (worst-case)',
     msg_p99, shm_p99,
     'Latency (µs)', 'p99 latency vs payload  [ASIL-D budget line = 10 ms]'),
]:
    bars_msg = ax.bar(x - BAR_W/2, msg_vals, BAR_W,
                      label='Message Passing', color=C_MSG_AVG, alpha=0.85)
    bars_shm = ax.bar(x + BAR_W/2, shm_vals, BAR_W,
                      label='Shared Memory',   color=C_SHM_AVG, alpha=0.85)

    # value labels on top of bars
    for bar in list(bars_msg) + list(bars_shm):
        h = bar.get_height()
        ax.text(bar.get_x() + bar.get_width() / 2, h + h * 0.02,
                f'{h:.1f}', ha='center', va='bottom', fontsize=8)

    # ASIL-D budget line — only meaningful on p99 chart
    if 'p99' in metric.lower():
        ax.axhline(ASIL_D_BUDGET_US, color=C_ASIL, linewidth=1.4,
                   linestyle='--', label=f'ASIL-D budget ({ASIL_D_BUDGET_US/1000:.0f} ms)')
        ax.text(len(sizes) - 0.5, ASIL_D_BUDGET_US * 1.05,
                'ASIL-D budget', color=C_ASIL, fontsize=8, ha='right')

    ax.set_xticks(x)
    ax.set_xticklabels(xlabels)
    ax.set_xlabel('Payload size')
    ax.set_ylabel(ylabel)
    ax.set_title(title, fontsize=10)
    ax.legend(fontsize=9)
    ax.grid(axis='y', color=C_GRID, linewidth=0.6)
    ax.set_axisbelow(True)

plt.tight_layout()
out1 = os.path.join(RESULTS_DIR, 'plot_sweep.png')
plt.savefig(out1, dpi=150, bbox_inches='tight')
plt.close()
print(f"\nSaved: {out1}")


for psize in [64, 16384]:
    msg_raw = load_raw(f'msg_results_{psize}B.csv')
    shm_raw = load_raw(f'shm_results_{psize}B.csv')

    if msg_raw is None or shm_raw is None:
        print(f"Skipping distribution plot for {psize}B — raw CSV not found.")
        continue

    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    label = SIZE_LABELS[PAYLOAD_SIZES.index(psize)]
    fig.suptitle(
        f'Latency Distribution at {label} payload  |  QNX 8.0',
        fontsize=12, fontweight='bold'
    )

    for ax, raw, color, name in [
        (axes[0], msg_raw, C_MSG_AVG, 'Message Passing'),
        (axes[1], shm_raw, C_SHM_AVG, 'Shared Memory'),
    ]:
        arr    = np.array(raw)
        # clip to p99.9 so outliers don't squash the histogram
        upper  = float(np.percentile(arr, 99.9))
        clipped = arr[arr <= upper]

        p50  = float(np.percentile(arr, 50))
        p99  = float(np.percentile(arr, 99))
        avg  = float(np.mean(arr))

        ax.hist(clipped, bins=80, color=color, alpha=0.75, edgecolor='none')
        ax.axvline(avg, color='black',  linewidth=1.2, linestyle='-',
                   label=f'avg = {avg:.1f} µs')
        ax.axvline(p50, color='navy',   linewidth=1.2, linestyle='--',
                   label=f'p50 = {p50:.1f} µs')
        ax.axvline(p99, color=C_ASIL,   linewidth=1.4, linestyle=':',
                   label=f'p99 = {p99:.1f} µs')

        ax.set_title(f'{name}', fontsize=10)
        ax.set_xlabel('Latency (µs)')
        ax.set_ylabel('Count')
        ax.legend(fontsize=8)
        ax.grid(color=C_GRID, linewidth=0.5)
        ax.set_axisbelow(True)
        ax.text(0.97, 0.95, f'(clipped at p99.9 = {upper:.0f} µs)',
                transform=ax.transAxes, fontsize=7,
                ha='right', va='top', color='grey')

    plt.tight_layout()
    out2 = os.path.join(RESULTS_DIR, f'plot_dist_{psize}B.png')
    plt.savefig(out2, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {out2}")

print("\nAll plots done.")