# GreenXV6 - Energy-Aware xv6 Operating System

An extension of [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) that makes CPU energy consumption an explicit OS-level concern.

## Four new features

| # | Feature | Kernel change | User demo |
|---|---------|--------------|-----------|
| 1 | CPU idle sleep (WFI) | `kernel/proc.c` scheduler | boot and observe |
| 2 | Per-process energy accounting | `kernel/proc.h`, `kernel/trap.c`, 2 new syscalls | `energy_monitor` |
| 3 | Eco scheduling mode | `kernel/proc.c` scheduler, 2 new syscalls | `eco_demo` |
| 4 | Sensor aggregator + locks | `kernel/sensoragg.c/h`, 2 new syscalls | `sensor_sim` + `sensor_read` |

## Quick start

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt-get install gcc-riscv64-unknown-elf qemu-system-riscv

# 2. Clone xv6
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv

# 3. Apply GreenXV6 patches
git clone https://github.com/mowda2/greenxv6.git
bash greenxv6/apply.sh .

# 4. Build and run
make clean && make qemu
```

## Demo commands (run inside xv6 shell)

```
energy_monitor       # live per-process energy table (Feature 2)
eco_demo             # eco scheduling before/after comparison (Feature 3)
sensor_sim &         # start 3 concurrent sensor processes (Feature 4)
sensor_read --watch  # live aggregated sensor dashboard (Feature 4)
```

Feature 1 (WFI idle sleep) is always active - no command needed.

## New syscalls

```c
uint64 getenergystat(int pid);              // get energy_ticks for a PID
int    resetenergystat(void);               // reset caller's energy counter
int    setecomode(int mode);                // 1=eco on, 0=eco off
int    getecomode(void);                    // query current eco_mode
int    writesensor(int type, int v);        // submit a sensor reading
int    getsensordata(struct sensor_snapshot *s); // read aggregated data
```

## Documentation

See `PROJECT_DOCUMENTATION.md` for full feature descriptions, design rationale, unit test descriptions, and the statement of contribution.
