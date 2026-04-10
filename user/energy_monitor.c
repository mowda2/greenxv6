// user/energy_monitor.c - GreenXV6 Feature 2 demo
// Displays energy_ticks for all live processes in a table.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROC       64
#define POLL_SECS   2
#define TICKS_PER_SEC 10

struct proc_energy {
  int    pid;
  uint64 ticks;
};

static void
sort_by_energy(struct proc_energy *arr, int n)
{
    for(int i = 0; i < n - 1; i++) {
    for(int j = 0; j < n - 1 - i; j++) {
      if(arr[j].ticks < arr[j+1].ticks) {
        struct proc_energy tmp = arr[j];
        arr[j] = arr[j+1];
        arr[j+1] = tmp;
      }
    }
    }
}

int
main(void)
{
    printf("\n=== GreenXV6 Energy Monitor ===\n");
  printf("Feature 2: per-process CPU energy accounting\n");
  printf("Refreshes every %d seconds. Press Ctrl-C to exit.\n\n", POLL_SECS);

  for(;;) {
    struct proc_energy entries[NPROC];
    int count = 0;

    for(int pid = 1; pid <= NPROC; pid++) {
      uint64 ticks = getenergystat(pid);
      if(ticks != (uint64)-1) {
        entries[count].pid = pid;
        entries[count].ticks = ticks;
        count++;
      }
    }

    sort_by_energy(entries, count);

    printf("\033[2J\033[H");
    printf("=== GreenXV6 Energy Monitor ===\n");
    printf("%-6s %-14s %-12s %s\n", "PID", "Energy Ticks", "Approx CPU-s", "Rating");
    printf("------ -------------- ------------ ----------\n");

    for(int i = 0; i < count; i++) {
      uint64 t = entries[i].ticks;
      uint64 cpu_s  = t / TICKS_PER_SEC;
      uint64 cpu_ds = (t % TICKS_PER_SEC) * 10 / TICKS_PER_SEC;

      const char *rating;
      if (t == 0)       rating = "idle";
      else if (t < 50)  rating = "low";
      else if (t < 200) rating = "moderate";
      else if (t < 500) rating = "high";
      else              rating = "!! heavy !!";

      printf("%-6d %-14llu %llu.%llu s       %s\n",
                     entries[i].pid, t, cpu_s, cpu_ds, rating);
    }

    printf("\nTotal active processes: %d\n", count);
    sleep(POLL_SECS * TICKS_PER_SEC);
  }
  exit(0);
}
