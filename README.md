# Linux Kernel Observability Lab

A collection of Linux systems experiments exploring scheduler instrumentation,
custom system calls, virtual-to-physical address translation, and memory access
behavior. Maintained by **Priyatam Annambhotla**.

The project uses kernel modules, kprobes, procfs interfaces, and small C programs
to study how user-space workloads interact with the kernel. It is an educational
lab with experimental implementations; kernel runtime validation and fresh
measurements are planned on another Linux system.

## Experiments

| Area | What it explores |
| --- | --- |
| Scheduler event counting | Instruments `pick_next_task_fair` and counts entries by the previous task's ID. These are function-entry observations, not a count of completed context switches. |
| Scheduler stack traces | Aggregates kernel stacks observed at the scheduler probe and exposes counts through procfs. |
| Switch-interval profiling | Groups per-CPU intervals between `__switch_to` observations by sampled kernel stack. Values are in nanoseconds and are approximate intervals, not exact per-process CPU accounting. |
| Custom syscall | Adds an educational `s2_encrypt` syscall that shifts characters by a small key and logs the result. This is a syscall integration exercise, not cryptographic encryption. |
| Pagemap diagnostics | Reads `/proc/self/pagemap` to inspect mappings and measure lookup latency across different allocation sizes. |
| Page walking | Explores VMA inspection and virtual-to-physical translation through a kernel procfs interface. |
| Access tracking | Contains a prototype for sampling page accessed bits. Address-space handling, page-table synchronization, and measurement validity still need review. |
| Optional tiered-memory simulation | Demonstrates allocation, injected delays, and file-backed I/O. It does not measure actual CXL or persistent-memory hardware. |

## Repository layout

```text
linux-kernel-observability-lab/
├── README.md
├── docs/
│   └── results/legacy/          # Preserved historical CSV files and log
├── scheduler-profiling/
│   ├── event-counting/
│   ├── stack-traces/
│   └── cpu-time/
├── syscalls/
│   ├── patches/
│   └── tests/
├── memory-diagnostics/
│   ├── pagemap/
│   ├── page-walking/
│   └── access-tracking/
├── extensions/
│   └── tiered-memory/
└── .gitignore
```

One implementation is retained for each core experiment. Generated kernel
objects, compiled executables, duplicate archives, and OS metadata are excluded.

## Environment and building

The syscall patch targets the upstream **Linux v6.12 (6.12.0)** source tree and
uses the lab-specific **x86-64 syscall number 548**. Scheduler instrumentation
also targets Linux 6.12; the event-counting probe directly decodes the x86-64
register ABI. Porting to another kernel or architecture requires reviewing probe
symbols, function signatures, and kernel APIs.

Use a disposable Linux VM for kernel experiments, with matching kernel build
files, a C compiler, GNU Make, and the relevant kernel options (`CONFIG_MODULES`,
`CONFIG_KPROBES`, `CONFIG_KALLSYMS`, `CONFIG_PROC_FS`, and stack-trace support).
The current source has not yet been built and exercised as a complete suite on
the target kernel.

Build a scheduler module against the running kernel's build tree:

```sh
make -C scheduler-profiling/event-counting
# An explicitly selected, prepared kernel build tree can also be used:
make -C scheduler-profiling/stack-traces KDIR=/path/to/linux-build
make -C scheduler-profiling/cpu-time KDIR=/path/to/linux-build
```

All scheduler variants build `perftop.ko`; use one variant at a time.
Event counting and stack traces expose `/proc/perftop`; switch-interval sampling
exposes `/proc/stack_sched`. The memory kernel modules remain prototypes and
need review before loading, especially the access-tracking implementation.

Build the user-space pagemap experiment and syscall client:

```sh
make -C memory-diagnostics/pagemap
make -C syscalls/tests
```

Pagemap PFN visibility requires appropriate privileges in the Linux lab
environment. Allocation sizes describe user-space allocation strategies; a
2 MB allocation does not itself request a huge page. The default pagemap workload
allocates 2 GiB; a smaller total can be supplied as the second argument, for
example `./memalloc 4KB 16` from its directory.

Check the syscall patch from an unmodified Linux v6.12 source checkout before
applying it:

```sh
git apply --check /path/to/linux-kernel-observability-lab/syscalls/patches/0001-add-s2-encrypt.patch
git apply /path/to/linux-kernel-observability-lab/syscalls/patches/0001-add-s2-encrypt.patch
```

Build and boot that custom kernel before running the syscall client. Number 548
is local to this lab and must not be assumed to identify this syscall on other
kernels. The patch includes the new implementation file and its build integration.

## Results and next steps

Existing result files are preserved unchanged in
[`docs/results/legacy/`](docs/results/legacy/). They are historical artifacts,
not validation of the reorganized and revised source. This update does not add
new measurements or revise the recorded results.

Experiments will be rerun on another Linux system. The next results update will
record the exact kernel build, configuration, hardware or VM setup, commands,
workload parameters, and raw output alongside interpretation and limitations.

The optional tiered-memory programs use simulated delays and ordinary file I/O;
their historical timing uses `clock()`, which measures process CPU time rather
than elapsed wall time. These outputs should not be interpreted as CXL/PMEM
latency, persistence guarantees, or measured energy consumption.

## Project work

This repository brings together scheduler probes, procfs reporting, syscall
integration, and memory diagnostics in a common lab layout. The preparation work
includes selecting canonical core implementations, removing per-event scheduler
logging, bounding profiler record storage, improving profiler cleanup and stack
matching, packaging a self-contained syscall patch, and correcting the pagemap
workload to inspect each allocation separately. These source changes still need
validation on the target Linux system.
