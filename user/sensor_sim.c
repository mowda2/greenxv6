// user/sensor_sim.c - GreenXV6 Feature 4 demo
// Simulates three environmental sensor processes

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SENSOR_TYPE_TEMP    0
#define SENSOR_TYPE_ENERGY  1
#define SENSOR_TYPE_AIR     2

static unsigned long seed = 12345;
static int myrand(void) {
    seed = seed * 1103515245 + 12345;
  return (int)((seed >> 16) & 0x7fff);
}

int
main(void)
{
    printf("\n=== GreenXV6 Sensor Simulator ===\n");
  printf("Feature 4: multi-process environmental sensor aggregation\n");
  printf("Spawning 3 sensor processes. Run 'sensor_read' to see data.\n\n");

  int temp_pid = fork();
  if(temp_pid == 0) {
    seed = 111;
    printf("[TempSensor PID %d] Started.\n", getpid());
    for(int i = 0; i < 30; i++) {
      int temp = 180 + (myrand() % 170);
      if(writesensor(SENSOR_TYPE_TEMP, temp) == 0)
                printf("[TempSensor] %d.%d C\n", temp/10, temp%10);
      sleep(10);
    }
    exit(0);
  }

  int energy_pid = fork();
  if(energy_pid == 0) {
    seed = 222;
    printf("[EnergyMeter PID %d] Started.\n", getpid());
    for(int i = 0; i < 30; i++) {
      int mw = 500 + (myrand() % 2500);
      if(writesensor(SENSOR_TYPE_ENERGY, mw) == 0)
                printf("[EnergyMeter] %d mW\n", mw);
      sleep(10);
    }
    exit(0);
  }

  int air_pid = fork();
  if(air_pid == 0) {
    seed = 333;
    printf("[AirSensor PID %d] Started.\n", getpid());
    for(int i = 0; i < 30; i++) {
      int aqi = myrand() % 150;
      const char *level = aqi < 50 ? "Good" : aqi < 100 ? "Moderate" : "Sensitive";
      if(writesensor(SENSOR_TYPE_AIR, aqi) == 0)
                printf("[AirSensor] AQI %d (%s)\n", aqi, level);
      sleep(10);
    }
    exit(0);
  }

  printf("[sensor_sim] All sensors running.\n");
  wait(0);
  wait(0);
  wait(0);
  printf("\n[sensor_sim] All sensor processes finished.\n");
  exit(0);
}
