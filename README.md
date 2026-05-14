# QNX IPC Benchmark Suite

A professional RTOS IPC benchmarking project built on **QNX Neutrino 8.0** running inside **QEMU** on Ubuntu.

<img width="1784" height="593" alt="plot_dist_64B" src="https://github.com/user-attachments/assets/a22b5119-786f-44ac-98d3-85b74462ee38" />
<img width="1784" height="593" alt="plot_dist_16384B" src="https://github.com/user-attachments/assets/2ead498e-cdf3-4d1d-9762-fd62a050e5c6" />
<img width="1934" height="763" alt="plot_sweep" src="https://github.com/user-attachments/assets/f339a657-4593-41de-b2b2-d4d18eb16c17" />
<img width="1934" height="763" alt="plot_sweep" src="https://github.com/user-attachments/assets/4f0eaf86-3005-43d6-80eb-eb10524dabe1" />


This project benchmarks and compares two fundamental inter-process communication mechanisms used in real-time embedded systems:

* QNX Native Message Passing (`MsgSend / MsgReceive / MsgReply`)
* POSIX Shared Memory + Semaphores

The benchmark measures:

* Average latency
* p50 / p95 / p99 / p99.9 latency
* Worst-case latency
* Payload scaling behavior
* IPC determinism under virtualization

---

# Project Overview

Modern embedded systems — especially automotive ADAS, robotics, industrial controllers, and avionics — rely heavily on deterministic inter-process communication.

QNX is widely used in:

* Automotive ECUs
* Infotainment systems
* Medical devices
* Industrial automation
* Safety-critical systems

This project simulates and benchmarks IPC behavior on a real RTOS environment.

---

# Technologies Used

| Component      | Technology                               |
| -------------- | ---------------------------------------- |
| RTOS           | QNX Neutrino 8.0                         |
| Virtualization | QEMU                                     |
| Host OS        | Ubuntu Linux                             |
| Language       | C                                        |
| Visualization  | Python + Matplotlib                      |
| IPC Mechanisms | QNX Message Passing, POSIX Shared Memory |

---

# IPC Mechanisms Benchmarked

## 1. QNX Native Message Passing

QNX uses synchronous message passing as its core IPC mechanism.

Functions used:

```c
MsgSend()
MsgReceive()
MsgReply()
```

Characteristics:

* Kernel-mediated
* Deterministic
* Safe synchronization
* Widely used in QNX microkernel architecture

---

## 2. Shared Memory + Semaphores

A high-performance IPC mechanism using:

```c
shm_open()
mmap()
sem_open()
sem_wait()
sem_post()
```

Characteristics:

* Shared address space
* Lower copy overhead
* Manual synchronization required
* Common in high-throughput embedded pipelines

---

# Benchmark Metrics

For each payload size:

* 8 B
* 64 B
* 512 B
* 4 KB
* 16 KB

The benchmark computes:

* Average latency
* Minimum latency
* p50 latency
* p95 latency
* p99 latency
* p99.9 latency
* Maximum latency

Each test executes:

* 200 warmup iterations
* 10,000 measured iterations

---

# Repository Structure

```text
.
├── msg_client.c
├── msg_server.c
├── shm_writer.c
├── shm_reader.c
├── parse_results.py
├── run_bench.sh
├── Makefile
├── results/
│   ├── plot_sweep.png
│   ├── plot_dist_64B.png
│   └── plot_dist_16384B.png
└── README.md
```

---

# System Architecture

```text
Ubuntu Linux Host
│
├── QNX SDP Toolchain
├── QEMU Virtual Machine
│
└── QNX Neutrino RTOS Guest
    │
    ├── Message Passing Benchmark
    └── Shared Memory Benchmark
```

---

# Environment Setup

## 1. Source QNX Environment

```bash
source ~/qnx800/qnxsdp-env.sh
```

---

## 2. Build QNX Executables

```bash
make clean
make
```

Verify binaries:

```bash
file msg_server
```

Expected:

```text
ELF 64-bit ... QNX Neutrino executable
```

---

# Shared Disk Workflow

A FAT disk image was used to transfer binaries between Ubuntu and QNX.

## Create Shared Image

```bash
dd if=/dev/zero of=~/qnx_shared.img bs=1M count=64
mkfs.vfat ~/qnx_shared.img
```

---

## Copy Binaries Into Shared Image

```bash
mcopy -i ~/qnx_shared.img msg_server ::
mcopy -i ~/qnx_shared.img msg_client ::
mcopy -i ~/qnx_shared.img shm_writer ::
mcopy -i ~/qnx_shared.img shm_reader ::
```

---

# QEMU Boot Command

```bash
qemu-system-x86_64 \
-M pc-i440fx-7.2 \
-cpu Haswell \
-m 512 \
-hda disk-qemu \
-drive file=~/qnx_shared.img,format=raw,index=1,media=disk \
-no-acpi \
-no-hpet \
-nographic \
-monitor none
```

---

# Mount Shared Image Inside QNX

```bash
mount -t dos /dev/hd1 /fs
```

---

# Copy Binaries To Writable Filesystem

```bash
cp /fs/msg_server /tmp/
cp /fs/msg_client /tmp/
cp /fs/shm_writer /tmp/
cp /fs/shm_reader /tmp/
```

---

# Make Executables Runnable

```bash
chmod +x /tmp/msg_server
chmod +x /tmp/msg_client
chmod +x /tmp/shm_writer
chmod +x /tmp/shm_reader
```

---

# Running The Benchmarks

## Message Passing Benchmark

Start server:

```bash
/tmp/msg_server &
```

Run client:

```bash
/tmp/msg_client
```

---

## Shared Memory Benchmark

Start writer:

```bash
/tmp/shm_writer &
```

Run reader:

```bash
/tmp/shm_reader
```

---

# Extracting Results

Inside QNX:

```bash
cp /tmp/*summary*.csv /fs/
cp /tmp/*results_*B.csv /fs/
```

Back on Ubuntu:

```bash
mkdir -p results
```

```bash
mcopy -i ~/qnx_shared.img ::msg_summary.csv results/
mcopy -i ~/qnx_shared.img ::shm_summary.csv results/
```

---

# Visualization

Run:

```bash
python3 parse_results.py
```

Generated plots:

* `plot_sweep.png`
* `plot_dist_64B.png`
* `plot_dist_16384B.png`

---

# Benchmark Results

## Message Passing Results

| Payload | Avg Latency | p99 Latency |
| ------- | ----------- | ----------- |
| 8 B     | 1063.59 us  | 7047.77 us  |
| 64 B    | 1034.80 us  | 6363.86 us  |
| 512 B   | 1030.56 us  | 6905.62 us  |
| 4 KB    | 1263.64 us  | 7930.52 us  |
| 16 KB   | 1336.37 us  | 8393.30 us  |

---

## Shared Memory Results

| Payload | Avg Latency | p99 Latency |
| ------- | ----------- | ----------- |
| 8 B     | 1020.72 us  | 5615.23 us  |
| 64 B    | 1082.72 us  | 7695.70 us  |
| 512 B   | 1074.92 us  | 7336.80 us  |
| 4 KB    | 979.41 us   | 6595.05 us  |
| 16 KB   | 1359.62 us  | 9008.37 us  |

---

# Observations

* Shared memory was not dramatically faster under QEMU virtualization.
* Host scheduling and virtualization overhead dominate latency.
* p99 latency remained below 10 ms for all payloads.
* Message passing showed stable deterministic behavior.
* Shared memory required explicit synchronization.

On real hardware:

* jitter would decrease significantly
* average latency would reduce
* determinism would improve further

---

# What This Project Demonstrates

This project demonstrates:

* RTOS workflow setup
* Cross-compilation using QNX SDP
* QEMU-based RTOS virtualization
* Real-time IPC benchmarking
* Payload scaling analysis
* Statistical latency characterization
* Shared memory synchronization
* QNX microkernel communication architecture

---

# Skills Demonstrated

* Embedded Systems
* RTOS Development
* QNX Neutrino
* POSIX IPC
* QEMU Virtualization
* Performance Benchmarking
* Linux Toolchains
* Real-Time Systems
* C Programming
* Systems Programming
* Data Visualization

---

# Future Improvements

Potential extensions:

* CPU stress testing
* Thread priority benchmarking
* Real-time scheduling experiments
* Multi-process sensor fusion simulation
* Pulse-based IPC benchmarking
* Real hardware deployment
* ARM/QNX target support
* Latency heatmaps and tracing

---

# Author

Apratim Phadke

Electronics & Telecommunication Engineering
Embedded Systems + RTOS + VLSI Enthusiast
