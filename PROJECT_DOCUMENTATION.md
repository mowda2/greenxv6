# GreenXV6: An Energy-Aware Operating System
## Project Documentation - CS Operating Systems Mini Project

---

## 1. Overview

GreenXV6 extends the xv6-RISC-V teaching operating system with four new features that make CPU energy consumption an explicit OS-level concern. The project interprets the sustainability requirement in both directions given in the brief:

- **Sustainability in OS design** (Features 1 and 3): structural changes to the scheduler that reduce energy waste during idle time and throttle high-consumption processes.
- **OS design for environmental sustainability** (Feature 4): a kernel-level environmental sensor aggregation service that simulates the kind of sensor-fusion workload that real green computing infrastructure relies on.
- **Observability** (Feature 2): per-process energy accounting exposed to user space, making energy consumption visible and measurable for the first time in xv6.

The four features work together as a coherent system. Feature 2 produces the data that Feature 3 acts on, Feature 1 saves energy during the gaps between scheduled work, and Feature 4 provides a realistic sustainability use-case that exercises the synchronization primitives covered in the course.

---

## 2. Feature Descriptions

### Feature 1 - CPU Idle Sleep via WFI

**Files modified:** `kernel/proc.c` (scheduler function)

**Description:**
The baseline xv6 scheduler is an infinite loop that continuously scans the process table for a RUNNABLE process. When no process is runnable, this loop spins at full CPU speed - consuming the same power as when running useful work.

GreenXV6 adds a `found` flag to the scheduler loop. After scanning all NPROC slots, if no runnable process was found, the scheduler executes the RISC-V `wfi` (Wait For Interrupt) assembly instruction:

```c
if(!found) {
    asm volatile("wfi");
}
```

The `wfi` instruction halts the hart (hardware thread) until the next interrupt arrives (typically the timer interrupt at the next tick). The CPU enters a low-power state during this interval, rather than spinning uselessly. On real RISC-V hardware this can reduce idle power draw by 30-80% depending on the implementation. In QEMU the instruction is supported and correctly wakes on timer interrupts.

The change is safe because `intr_on()` is always called at the top of the scheduler loop, guaranteeing the CPU will be woken by the next timer interrupt and re-enter the process-selection logic.

**Why it qualifies as a new feature:** The original xv6 scheduler has no WFI or any idle-sleep mechanism. This is a genuine change to the scheduler's energy behavior with a measurable real-world analogue (modern Linux uses a similar technique via cpuidle).

**Unit test:** Run stress_test (or simply boot xv6 and let it sit idle). With WFI enabled, QEMU host CPU usage for the emulation process drops noticeably when xv6 has no running user processes.

---

### Feature 2 - Per-Process Energy Accounting

**Files modified:** `kernel/proc.h`, `kernel/proc.c` (allocproc), `kernel/trap.c` (usertrap), `kernel/sysproc.c`, `kernel/syscall.h`, `kernel/syscall.c`, `user/user.h`, `user/usys.pl`

**New syscalls:** `getenergystat(pid)`, `resetenergystat()`

**Description:**
A new field `uint64 energy_ticks` is added to `struct proc`. Every time the timer interrupt fires while a process is running (i.e., in `usertrap()` with `which_dev == 2`), the running process's counter is incremented by one:

```c
if(which_dev == 2) {
    struct proc *ep = myproc();
    if(ep && ep->state == RUNNING)
        ep->energy_ticks++;
    yield();
}
```

This gives a tick-resolution approximation of CPU time consumed, which is a direct proxy for energy consumption (since CPU power draw is approximately proportional to utilization over time).

Two syscalls expose this to user space:

- `getenergystat(int pid)` - returns the `energy_ticks` value for any live process. Returns `(uint64)-1` if the PID is not found.
- `resetenergystat()` - resets the calling process's counter to zero, useful for benchmarking a specific code section in isolation.

The included `energy_monitor` program polls all PIDs and displays a live sorted table classifying each process as idle, low, moderate, high, or heavy based on their tick count.

**Unit test:** Run `energy_monitor` in one shell. In another shell, run a CPU-intensive program (`eco_demo` works). Observe that the intensive process accumulates ticks at a much higher rate than idle shell processes.

---

### Feature 3 - Eco Scheduling Mode

**Files modified:** `kernel/proc.c` (scheduler, global `eco_mode`), `kernel/sysproc.c`, `kernel/syscall.h`, `kernel/syscall.c`, `user/user.h`, `user/usys.pl`

**New syscalls:** `setecomode(int mode)`, `getecomode()`

**Description:**
A kernel-global integer `eco_mode` (default 0) controls whether the scheduler applies energy-aware throttling. When `eco_mode = 1`, any process whose `energy_ticks` exceeds a configurable threshold (`ECO_THRESHOLD = 500`) accumulates a skip counter. The scheduler skips that process every `ECO_SKIP_EVERY` (default: 3) visits, giving lower-energy processes proportionally more CPU time:

```c
if(eco_mode && p->energy_ticks > ECO_THRESHOLD) {
    p->eco_skip++;
    if(p->eco_skip % ECO_SKIP_EVERY != 0) {
        release(&p->lock);
        found = 1;
        continue;
    }
}
```

This is a simplified model of the "Green Scheduling" policies studied in energy-aware OS research, where process priority is dynamically adjusted based on historical resource consumption. It biases CPU time toward recently-light processes, which in a real system corresponds to preferring I/O-bound or intermittent tasks over sustained compute-heavy ones.

Two syscalls control the mode:

- `setecomode(1)` - enables eco scheduling kernel-wide, prints a confirmation to console.
- `setecomode(0)` - restores normal round-robin.
- `getecomode()` - queries current state.

The included `eco_demo` program demonstrates the effect: it forks a greedy child and a light child, measures their tick acquisition with and without eco mode enabled, and prints the difference.

**Unit test:** Run `eco_demo`. The greedy child's per-phase tick delta should be measurably lower in Phase 2 (eco mode on) than Phase 1 (eco mode off).

---

### Feature 4 - Environmental Sensor Aggregator with Spinlock Synchronization

**New files:** `kernel/sensoragg.h`, `kernel/sensoragg.c`

**Files modified:** `kernel/main.c`, `kernel/defs.h`, `kernel/sysproc.c`, `kernel/syscall.h`, `kernel/syscall.c`, `user/user.h`, `user/usys.pl`

**New syscalls:** `writesensor(int type, int value)`, `getsensordata(struct sensor_snapshot *snap)`

**Description:**
This feature implements a kernel service that aggregates environmental sensor readings from multiple concurrent processes. It models the kind of sensor-fusion layer that an OS supporting smart building energy management or environmental monitoring would need.

The kernel maintains a single global `struct sensor_agg` containing:

- A circular buffer of the last 16 `struct sensor_reading` entries.
- Running totals and counts for each sensor type (temperature, energy usage, air quality index).
- A `struct spinlock` protecting all shared state.

Any user-space process can submit a reading via `writesensor(type, value)`. The kernel acquires the spinlock, writes the entry into the circular buffer, and updates the running totals. Any other process can call `getsensordata(&snap)` to receive a `sensor_snapshot` with the running averages for all three sensor types.

Three sensor types are supported:

- `SENSOR_TYPE_TEMP (0)` - temperature in tenths of degrees C (e.g. 215 = 21.5C)
- `SENSOR_TYPE_ENERGY (1)` - power draw in milliwatts
- `SENSOR_TYPE_AIR (2)` - air quality index (0 = excellent, 500 = hazardous)

The synchronization model directly applies course material: the spinlock ensures that concurrent sensor-writing processes do not corrupt the shared buffer or running totals. This is safe even if multiple sensor processes write simultaneously, which the `sensor_sim` demo exercises by forking three concurrent sensor processes.

The `sensor_read --watch` program continuously polls `getsensordata()` and displays the current averages with health classifications, demonstrating real-time aggregation across processes.

**Unit test:** In one shell, run `sensor_sim`. In a second shell, run `sensor_read --watch`. The total readings count should increment and averages should update every second. The test passes if averages remain consistent (no torn reads or partial updates) across all readings.

---

## 3. How to Run the Demo

### Prerequisites

```bash
# Install RISC-V toolchain and QEMU (Ubuntu/Debian)
sudo apt-get install gcc-riscv64-unknown-elf qemu-system-riscv

# Clone xv6
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
```

### Applying GreenXV6 patches

```bash
# From the xv6-riscv directory:
git clone https://github.com/mowda2/greenxv6.git
bash greenxv6/apply.sh .
```

### Building

```bash
make clean && make qemu
```

### Demo sequence (inside xv6 shell)

**Feature 1 - WFI idle sleep:**
The feature is always active after boot. No user command needed. To observe it, boot xv6 and do nothing - QEMU host CPU usage stays low because xv6 is sleeping between timer ticks.

**Feature 2 - Energy monitor:**
```
$ energy_monitor
```
Opens a live table of all processes sorted by energy_ticks. Leave it running and open background workloads to see values climb.

**Feature 3 - Eco scheduling demo:**
```
$ eco_demo
```
Runs a two-phase experiment (eco off then eco on), prints tick deltas for a greedy and a light child, showing the throttling effect.

**Feature 4 - Sensor aggregator:**
Open two shell sessions (xv6 supports this via `&`):
```
$ sensor_sim &
$ sensor_read --watch
```
`sensor_sim` forks three sensor processes writing readings every second. `sensor_read --watch` shows live averages updating in real time.

---

## 4. Repository Structure

```
greenxv6/
|-- apply.sh                  # Automated patch script
|-- kernel/
|   |-- sensoragg.h           # Feature 4: sensor aggregator header
|   |-- sensoragg.c           # Feature 4: aggregator implementation
|-- user/
|   |-- energy_monitor.c      # Feature 2: live energy dashboard
|   |-- eco_demo.c            # Feature 3: eco scheduling experiment
|   |-- sensor_sim.c          # Feature 4: sensor process simulator
|   |-- sensor_read.c         # Feature 4: sensor data reader
```

---

## 5. Design Decisions and Tradeoffs

**WFI safety:** `wfi` is placed after `intr_on()` so the hart is guaranteed to be interruptible. On multi-core QEMU configurations, only the hart that finds an empty proc table sleeps; others continue scheduling. This is safe because the proc table and each proc's spinlock are shared across harts.

**Energy accounting granularity:** Counting timer ticks (approximately 10 Hz in QEMU's xv6 configuration) gives 100ms granularity. This is coarse but sufficient to distinguish idle, moderate, and heavy processes. A higher-resolution approach would use the RISC-V `cycle` CSR, but that requires M-mode access not available in xv6 user mode.

**Eco skip ratio:** The 1-in-3 skip ratio (`ECO_SKIP_EVERY = 3`) was chosen to be noticeable in a demo without being so aggressive that high-energy processes starve. In a production system this would be a tunable parameter.

**Sensor buffer size:** The 16-entry circular buffer is small but sufficient for the demo. The running-total approach (rather than recomputing the average on each read) ensures O(1) read performance regardless of total readings received.

**Spinlock vs sleeplock for sensors:** A spinlock is appropriate here because critical sections (the write and read paths) are short and non-blocking. A sleeplock would add unnecessary overhead for such brief critical sections.

---

## 6. Statement of Contribution

| Member | Contributions |
|--------|--------------|
| Mohammed Owda | Feature 1 (WFI scheduler), kernel/proc.c patches, apply.sh |
| Harveen Grewal | Feature 2 (energy accounting), trap.c, energy_monitor.c |
| Sameer Shah | Feature 3 (eco scheduling), eco_demo.c, syscall wiring |
| Aly Soliman | Feature 4 (sensor aggregator), sensoragg.c/h, sensor_sim.c, sensor_read.c |

Each member confirms that the contribution statement above accurately reflects their work on this project.

Signatures:

1. Mohammed Owda           Date: April 10, 2026
2. Harveen Grewal          Date: April 10, 2026
3. Sameer Shah             Date: April 10, 2026
4. Aly Soliman             Date: April 10, 2026
