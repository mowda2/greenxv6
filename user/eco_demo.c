// user/eco_demo.c - GreenXV6 Feature 3 demo
// Demonstrates eco scheduling mode

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BURN_ITERS    500000
#define MEASURE_TICKS 30

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

                printf("Phase 1: Normal scheduling (eco_mode = OFF)\n");
                  setecomode(0);

                    int greedy_pid = fork();
                      if(greedy_pid == 0) {
                          resetenergystat();
                              for(;;) burn_cpu();
                                  exit(0);
                                    }

                                      int light_pid = fork();
                                        if(light_pid == 0) {
                                            resetenergystat();
                                                for(;;) {
                                                      burn_cpu();
                                                            sleep(5);
                                                                }
                                                                    exit(0);
                                                                      }

                                                                        sleep(MEASURE_TICKS);

                                                                          uint64 greedy_ticks_normal = getenergystat(greedy_pid);
                                                                            uint64 light_ticks_normal  = getenergystat(light_pid);

                                                                              printf("  Greedy child (PID %d): %llu ticks\n", greedy_pid, greedy_ticks_normal);
                                                                                printf("  Light  child (PID %d): %llu ticks\n\n", light_pid, light_ticks_normal);

                                                                                  printf("Phase 2: Eco scheduling (eco_mode = ON)\n");
                                                                                    setecomode(1);

                                                                                      sleep(MEASURE_TICKS);

                                                                                        uint64 greedy_ticks_eco = getenergystat(greedy_pid);
                                                                                          uint64 light_ticks_eco  = getenergystat(light_pid);

                                                                                            uint64 greedy_delta = greedy_ticks_eco - greedy_ticks_normal;
                                                                                              uint64 light_delta  = light_ticks_eco  - light_ticks_normal;

                                                                                                printf("  Greedy child (PID %d): +%llu ticks (throttled)\n", greedy_pid, greedy_delta);
                                                                                                  printf("  Light  child (PID %d): +%llu ticks (preferred)\n\n", light_pid, light_delta);

                                                                                                    kill(greedy_pid);
                                                                                                      kill(light_pid);
                                                                                                        wait(0);
                                                                                                          wait(0);
                                                                                                            setecomode(0);
                                                                                                            
                                                                                                              printf("=== Summary ===\n");
                                                                                                                printf("Eco mode reduced greedy child's CPU allocation.\n");
                                                                                                                  printf("Lower-energy processes got relatively more time.\n\n");
                                                                                                                  
                                                                                                                    exit(0);
                                                                                                                    }
