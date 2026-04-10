// user/sensor_read.c â GreenXV6 Feature 4 demo
// Reads the aggregated environmental sensor data from the kernel.
// Run concurrently with sensor_sim to see data being updated.
// Run as: sensor_read [--watch]
//   --watch: continuously refresh (Ctrl-C to stop)
//   (no args): print once and exit

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// struct sensor_snapshot is defined in user/user.h (added by apply.sh)

static void
print_snapshot(struct sensor_snapshot *s)
{
  printf("\n=== GreenXV6 Sensor Dashboard ===\n");
  printf("Feature 4: kernel sensor aggregator with spinlock sync\n");
  printf("Total readings submitted: %d\n\n", s->total_readings);

  if(s->avg_temp >= 0) {
    printf("  Temperature: %d.%d Â°C", s->avg_temp / 10, s->avg_temp % 10);
    if(s->avg_temp > 280)      printf("  [WARM â consider cooling]\n");
    else if(s->avg_temp > 250) printf("  [Normal]\n");
    else                       printf("  [Cool]\n");
  } else {
    printf("  Temperature: no data yet\n");
  }

  if(s->avg_energy >= 0) {
    printf("  Energy usage: %d mW", s->avg_energy);
    if(s->avg_energy > 2000)      printf("  [HIGH â throttle recommended]\n");
    else if(s->avg_energy > 1000) printf("  [Moderate]\n");
    else                          printf("  [Low â eco]\n");
  } else {
    printf("  Energy usage: no data yet\n");
  }

  if(s->avg_air >= 0) {
    printf("  Air quality: AQI %d", s->avg_air);
    if(s->avg_air >= 100)     printf("  [POOR â sensitive groups at risk]\n");
    else if(s->avg_air >= 50) printf("  [Moderate]\n");
    else                      printf("  [Good]\n");
  } else {
    printf("  Air quality: no data yet\n");
  }

  printf("\n(Data averaged over all readings since boot)\n");
}

int
main(int argc, char *argv[])
{
  int watch_mode = 0;
  if(argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'w')
    watch_mode = 1;

  struct sensor_snapshot snap;

  if(!watch_mode) {
    if(getsensordata(&snap) < 0) {
      printf("sensor_read: getsensordata() failed\n");
      exit(1);
    }
    print_snapshot(&snap);
  } else {
    printf("Watching sensor data. Ctrl-C to stop.\n");
    for(;;) {
      if(getsensordata(&snap) < 0) {
        printf("sensor_read: getsensordata() failed\n");
        exit(1);
      }
      printf("\033[2J\033[H"); // ANSI clear screen
      print_snapshot(&snap);
      sleep(10); // refresh every ~1 second
    }
  }
  exit(0);
}
