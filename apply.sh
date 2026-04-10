#!/bin/bash
# apply.sh - GreenXV6 patch application script
# Usage: ./apply.sh /path/to/xv6-riscv

set -e

XDK="${1:-.}"

if [ ! -f "$XDK/kernel/main.c" ]; then
  echo "ERROR: $XDK does not look like an xv6-riscv checkout."
  echo "Usage: $0 /path/to/xv6-riscv"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== GreenXV6 patch script ==="
echo "Target: $XDK"
echo ""

append_once() {
  local file="$1"
  local marker="$2"
  local text="$3"
  if ! grep -q "$marker" "$file"; then
    echo "$text" >> "$file"
    echo "  [+] $file"
  else
    echo "  [=] $file (already patched)"
  fi
}

echo "--- Step 1: Copy new kernel files ---"
cp "$SCRIPT_DIR/kernel/sensoragg.h" "$XDK/kernel/sensoragg.h"
cp "$SCRIPT_DIR/kernel/sensoragg.c" "$XDK/kernel/sensoragg.c"
echo "  [+] kernel/sensoragg.h"
echo "  [+] kernel/sensoragg.c"

echo ""
echo "--- Step 2: Copy user programs ---"
cp "$SCRIPT_DIR/user/energy_monitor.c" "$XDK/user/energy_monitor.c"
cp "$SCRIPT_DIR/user/eco_demo.c"       "$XDK/user/eco_demo.c"
cp "$SCRIPT_DIR/user/sensor_sim.c"     "$XDK/user/sensor_sim.c"
cp "$SCRIPT_DIR/user/sensor_read.c"    "$XDK/user/sensor_read.c"
echo "  [+] user/energy_monitor.c"
echo "  [+] user/eco_demo.c"
echo "  [+] user/sensor_sim.c"
echo "  [+] user/sensor_read.c"

echo ""
echo "--- Step 3: Patch kernel/proc.h ---"
PROC_H="$XDK/kernel/proc.h"
append_once "$PROC_H" "energy_ticks" '
// GreenXV6 Feature 2: per-process energy accounting
  uint64 energy_ticks;
// GreenXV6 Feature 3: eco scheduling skip counter
  int eco_skip;'

echo ""
echo "--- Step 4: Patch kernel/syscall.h ---"
SCALL_H="$XDK/kernel/syscall.h"
append_once "$SCALL_H" "SYS_getenergystat" '
// GreenXV6 new syscalls
#define SYS_getenergystat  22
#define SYS_resetenergystat 23
#define SYS_setecomode     24
#define SYS_getecomode     25
#define SYS_writesensor    26
#define SYS_getsensordata  27'

echo ""
echo "--- Step 5: Patch kernel/syscall.c ---"
SCALL_C="$XDK/kernel/syscall.c"

if ! grep -q "sensoragg.h" "$SCALL_C"; then
  sed -i 's|#include "proc.h"|#include "proc.h"\n#include "sensoragg.h"|' "$SCALL_C"
  echo '  [+] Added #include "sensoragg.h"'
fi

append_once "$SCALL_C" "sys_getenergystat" '
extern uint64 sys_getenergystat(void);
extern uint64 sys_resetenergystat(void);
extern uint64 sys_setecomode(void);
extern uint64 sys_getecomode(void);
extern uint64 sys_writesensor(void);
extern uint64 sys_getsensordata(void);'

if ! grep -q "SYS_getenergystat.*sys_getenergystat" "$SCALL_C"; then
  sed -i 's|\[SYS_close\].*sys_close,|[SYS_close]   sys_close,\n[SYS_getenergystat] sys_getenergystat,\n[SYS_resetenergystat] sys_resetenergystat,\n[SYS_setecomode]  sys_setecomode,\n[SYS_getecomode]  sys_getecomode,\n[SYS_writesensor] sys_writesensor,\n[SYS_getsensordata] sys_getsensordata,|' "$SCALL_C"
  echo "  [+] Added dispatch entries to syscalls[]"
else
  echo "  [=] syscall.c dispatch table (already patched)"
fi

echo ""
echo "--- Step 6: Patch kernel/sysproc.c ---"
SYSPROC="$XDK/kernel/sysproc.c"

if ! grep -q "sensoragg.h" "$SYSPROC"; then
  sed -i '1s|^|#include "sensoragg.h"\n|' "$SYSPROC"
  echo '  [+] Added #include "sensoragg.h"'
fi

append_once "$SYSPROC" "sys_getenergystat" '
// ---- GreenXV6 Feature 2: energy accounting syscalls ----

extern int eco_mode;

uint64
sys_getenergystat(void)
{
  int pid;
  argint(0, &pid);
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED) {
      uint64 t = p->energy_ticks;
      release(&p->lock);
      return t;
    }
    release(&p->lock);
  }
  return (uint64)-1;
}

uint64
sys_resetenergystat(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->energy_ticks = 0;
  release(&p->lock);
  return 0;
}

// ---- GreenXV6 Feature 3: eco mode syscalls ----

uint64
sys_setecomode(void)
{
  int mode;
  argint(0, &mode);
  eco_mode = mode ? 1 : 0;
  printf("GreenXV6: eco_mode = %d\n", eco_mode);
  return 0;
}

uint64
sys_getecomode(void)
{
  return eco_mode;
}

// ---- GreenXV6 Feature 4: sensor aggregator syscalls ----

uint64
sys_writesensor(void)
{
  int type, value;
  argint(0, &type);
  argint(1, &value);
  return sensoragg_write(type, value);
}

uint64
sys_getsensordata(void)
{
  uint64 addr;
  argaddr(0, &addr);
  struct sensor_snapshot snap;
  if(sensoragg_read(&snap) < 0)
    return -1;
  struct proc *p = myproc();
  if(copyout(p->pagetable, addr, (char*)&snap, sizeof(snap)) < 0)
    return -1;
  return 0;
}'

echo ""
echo "--- Step 7: Patch kernel/defs.h ---"
DEFS_H="$XDK/kernel/defs.h"
append_once "$DEFS_H" "sensoragg_init" '
// sensoragg.c (GreenXV6 Feature 4)
struct sensor_snapshot;
void            sensoragg_init(void);
int             sensoragg_write(int, int);
int             sensoragg_read(struct sensor_snapshot*);

// eco_mode (GreenXV6 Feature 3, defined in proc.c)
extern int      eco_mode;'

echo ""
echo "--- Step 8: Patch kernel/proc.c ---"
PROC_C="$XDK/kernel/proc.c"

append_once "$PROC_C" "eco_mode" '
// GreenXV6 Feature 3: eco scheduling global flag
int eco_mode = 0;
#define ECO_THRESHOLD  500
#define ECO_SKIP_EVERY 3'

if ! grep -q "p->energy_ticks = 0" "$PROC_C"; then
  sed -i 's|p->pid = allocpid();|p->pid = allocpid();\n  p->energy_ticks = 0; \/\/ GreenXV6 Feature 2\n  p->eco_skip = 0;     \/\/ GreenXV6 Feature 3|' "$PROC_C"
  echo "  [+] Zero-init energy_ticks and eco_skip in allocproc()"
else
  echo "  [=] allocproc() (already patched)"
fi

if ! grep -q "wfi" "$PROC_C"; then
  sed -i '/intr_on();/{
n
a\    int found = 0; // GreenXV6 Feature 1
}' "$PROC_C"

  sed -i 's|p->state = RUNNING;|// GreenXV6 Feature 3: eco skip\n        if(eco_mode \&\& p->energy_ticks > ECO_THRESHOLD) {\n          p->eco_skip++;\n          if(p->eco_skip % ECO_SKIP_EVERY != 0) { release(\&p->lock); found=1; continue; }\n        }\n        p->state = RUNNING;|' "$PROC_C"

  sed -i 's|c->proc = 0;$|c->proc = 0;\n        found = 1; // GreenXV6 Feature 1|' "$PROC_C"

  python3 - "$PROC_C" <<'PYEOF'
import re, sys
path = sys.argv[1]
text = open(path).read()
wfi_block = """
      // GreenXV6 Feature 1: CPU idle sleep via WFI
      if(!found) {
        asm volatile("wfi");
      }
"""
sched_start = text.find('\nscheduler(')
if sched_start == -1:
    sched_start = text.find('\nvoid\nscheduler(')
if sched_start == -1:
    print("  [!] Could not locate scheduler()")
    sys.exit(0)
inner = text.find("release(&p->lock);\n    }\n", sched_start)
if inner == -1:
    print("  [!] Could not locate proc loop end")
    sys.exit(0)
if 'wfi' in text[sched_start:sched_start+3000]:
    print("  [=] scheduler() WFI (already patched)")
    sys.exit(0)
insert_pos = inner + len("release(&p->lock);\n    }\n")
new_text = text[:insert_pos] + wfi_block + text[insert_pos:]
open(path, 'w').write(new_text)
print("  [+] WFI idle sleep injected into scheduler()")
PYEOF
else
  echo "  [=] scheduler() WFI (already patched)"
fi

echo ""
echo "--- Step 9: Patch kernel/trap.c ---"
TRAP_C="$XDK/kernel/trap.c"

if ! grep -q "energy_ticks++" "$TRAP_C"; then
  python3 - "$TRAP_C" <<'PYEOF'
import sys
path = sys.argv[1]
text = open(path).read()
if 'energy_ticks++' in text:
    print("  [=] trap.c energy accounting (already patched)")
    sys.exit(0)
old = 'if(which_dev == 2)\n    yield();'
new = ('if(which_dev == 2) {\n'
       '    // GreenXV6 Feature 2: energy accounting\n'
       '    struct proc *ep = myproc();\n'
       '    if(ep && ep->state == RUNNING)\n'
       '      ep->energy_ticks++;\n'
       '    yield();\n'
       '  }')
if old in text:
    new_text = text.replace(old, new, 1)
    open(path, 'w').write(new_text)
    print("  [+] energy_ticks increment added to usertrap()")
else:
    print("  [!] Could not find timer yield block in trap.c")
PYEOF
else
  echo "  [=] trap.c energy accounting (already patched)"
fi

echo ""
echo "--- Step 10: Patch kernel/main.c ---"
MAIN_C="$XDK/kernel/main.c"
if ! grep -q "sensoragg_init" "$MAIN_C"; then
  sed -i 's|userinit();|userinit();\n  sensoragg_init(); \/\/ GreenXV6 Feature 4|' "$MAIN_C"
  echo "  [+] sensoragg_init() added to main()"
else
  echo "  [=] main.c (already patched)"
fi

echo ""
echo "--- Step 11: Patch user/user.h ---"
USER_H="$XDK/user/user.h"
append_once "$USER_H" "getenergystat" '
// GreenXV6 new syscalls
struct sensor_snapshot {
  int avg_temp;
  int avg_energy;
  int avg_air;
  int total_readings;
};
uint64  getenergystat(int);
int     resetenergystat(void);
int     setecomode(int);
int     getecomode(void);
int     writesensor(int, int);
int     getsensordata(struct sensor_snapshot*);'

echo ""
echo "--- Step 12: Patch user/usys.pl ---"
USYS="$XDK/user/usys.pl"
append_once "$USYS" "getenergystat" '
entry("getenergystat");
entry("resetenergystat");
entry("setecomode");
entry("getecomode");
entry("writesensor");
entry("getsensordata");'

echo ""
echo "--- Step 13: Patch Makefile ---"
MK="$XDK/Makefile"

if ! grep -q "sensoragg.o" "$MK"; then
  sed -i 's|\$K/pipe.o \\|\$K/pipe.o \\\n\t\$K/sensoragg.o \\|' "$MK"
  echo "  [+] sensoragg.o added to OBJS"
else
  echo "  [=] Makefile OBJS (already patched)"
fi

if ! grep -q "energy_monitor" "$MK"; then
  sed -i 's|\$U/_wc\\|\$U/_wc\\\n\t\$U/_energy_monitor\\\n\t\$U/_eco_demo\\\n\t\$U/_sensor_sim\\\n\t\$U/_sensor_read\\|' "$MK"
  echo "  [+] User programs added to UPROGS"
else
  echo "  [=] Makefile UPROGS (already patched)"
fi

echo ""
echo "=== All patches applied ==="
echo ""
echo "To build and run:"
echo "  cd $XDK"
echo "  make clean && make qemu"
echo ""
echo "Demo commands (inside xv6 shell):"
echo "  energy_monitor       # Feature 2: live energy dashboard"
echo "  eco_demo             # Feature 3: eco scheduling demo"
echo "  sensor_sim &         # Feature 4: start sensor processes"
echo "  sensor_read --watch  # Feature 4: watch live sensor data"
