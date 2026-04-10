// user/energy_monitor.c â GreenXV6 Feature 2 demo
// Displays energy_ticks for all live processes in a table.
// Run as: energy_monitor
//
// It polls the PIDs 1..NPROC, calls getenergystat() for each,
// and prints a sorted table. Re-runs every 2 seconds so you can
// watch values change in real time.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROC       64   // must match kernel param.h
#define POLL_SECS   2    // seconds between refreshes
#define TICKS_PER_SEC 10 // approximate (xv6 timer rate ~10 Hz on QEMU)

// A snapshot of one process's energy data
struct proc_energy {
  int pid;
  int ticks;
};

// Sort by energy_ticks descending (bubble sort â simple enough for â¤64 procs)
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

    // Poll every possible PID
    for(int pid = 1; pid <= NPROC; pid++) {
      int ticks = (int)getenergystat(pid);
      if(ticks >= 0) {
        entries[count].pid = pid;
        entries[count].ticks = ticks;
        count++;
      }
    }

    sort_by_energy(entries, count);

    // Print table header
    printf("\033[2J\033[H"); // clear screen (ANSI â works in QEMU serial)
    printf("=== GreenXV6 Energy Monitor ===\n");
    printf("PID\tEnergy Ticks\tCPU-s\tRating\n");
    printf("----\t------------\t-----\t------\n");

    for(int i = 0; i < count; i++) {
      int t = entries[i].ticks;
      int cpu_s  = t / TICKS_PER_SEC;
      int cpu_ds = (t % TICKS_PER_SEC) * 10 / TICKS_PER_SEC;

      const char *rating;
      if (t == 0)       rating = "idle";
      else if (t < 50)  rating = "low";
      else if (t < 200) rating = "moderate";
      else if (t < 500) rating = "high";
      else              rating = "!! heavy !!";

      printf("%d\t%d\t\t%d.%d s\t%s\n",
             entries[i].pid, t, cpu_s, cpu_ds, rating);
    }

    printf("\nTotal active processes: %d\n", count);
    printf("(Use eco_demo to toggle eco scheduling mode)\n");
    sleep(POLL_SECS * TICKS_PER_SEC);
  }
  exit(0);
}
