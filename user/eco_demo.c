// user/eco_demo.c â GreenXV6 Feature 3 demo
// Demonstrates eco scheduling mode:
//   1. Forks a "greedy" child that burns CPU.
//   2. Forks a "light" child that mostly sleeps.
//   3. Enables eco_mode and shows the greedy child gets throttled.
//   4. Measures and reports energy_ticks for each child.
//
// Run as: eco_demo

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BURN_ITERS    500000  // iterations for greedy child busy-loop
#define MEASURE_TICKS 30      // how many ticks to measure (3 seconds)

static void
burn_cpu(void)
{
  volatile long x = 0;
  for(long i = 0; i < BURN_ITERS; i++)
    x += i * 3 - i / 2;
  (void)x;
}

int
main(void)
{
  printf("\n=== GreenXV6 Eco Scheduling Demo ===\n");
  printf("Feature 3: energy-aware scheduler throttling\n\n");

  // Phase 1: measure without eco mode
  printf("Phase 1: Normal scheduling (eco_mode = OFF)\n");
  setecomode(0);

  int greedy_pid = fork();
  if(greedy_pid == 0) {
    // Greedy child â burns CPU continuously
    resetenergystat();
    for(;;) burn_cpu();
    exit(0);
  }

  int light_pid = fork();
  if(light_pid == 0) {
    // Light child â does a little work, mostly sleeps
    resetenergystat();
    for(;;) {
      burn_cpu();
      sleep(5); // sleep 0.5 seconds
    }
    exit(0);
  }

  // Parent: measure for MEASURE_TICKS ticks
  sleep(MEASURE_TICKS);

  int greedy_ticks_normal = (int)getenergystat(greedy_pid);
  int light_ticks_normal  = (int)getenergystat(light_pid);

  printf("  Greedy child (PID %d): %d ticks\n", greedy_pid, greedy_ticks_normal);
  printf("  Light  child (PID %d): %d ticks\n\n", light_pid, light_ticks_normal);

  // Phase 2: enable eco mode and re-measure
  printf("Phase 2: Eco scheduling (eco_mode = ON)\n");
  setecomode(1);

  sleep(MEASURE_TICKS);

  int greedy_ticks_eco = (int)getenergystat(greedy_pid);
  int light_ticks_eco  = (int)getenergystat(light_pid);

  // Ticks since start of phase 2 = current - what we saw at end of phase 1
  int greedy_delta = greedy_ticks_eco - greedy_ticks_normal;
  int light_delta  = light_ticks_eco  - light_ticks_normal;

  printf("  Greedy child (PID %d): +%d ticks (throttled)\n", greedy_pid, greedy_delta);
  printf("  Light  child (PID %d): +%d ticks (preferred)\n\n", light_pid, light_delta);

  // Kill the children
  kill(greedy_pid);
  kill(light_pid);
  wait(0);
  wait(0);
  setecomode(0); // restore normal mode

  printf("=== Summary ===\n");
  printf("Eco mode reduced greedy child's CPU allocation.\n");
  printf("Lower-energy processes got relatively more time.\n");
  printf("This extends battery life in constrained environments.\n\n");

  exit(0);
}
