// kernel/sensoragg.h - GreenXV6 Feature 4
// Shared-memory environmental sensor aggregator.
//
// Multiple user-space sensor processes write readings into the kernel
// via the writesensor() syscall. The kernel aggregates them using a
// spinlock-protected circular buffer. Any process can read the latest
// aggregate via the getsensordata() syscall.

#ifndef SENSORAGG_H
#define SENSORAGG_H

#define SENSOR_BUF_SIZE  16
#define SENSOR_TYPE_TEMP    0
#define SENSOR_TYPE_ENERGY  1
#define SENSOR_TYPE_AIR     2

struct sensor_reading {
  int  type;
    int  value;
      int  pid;
        uint timestamp;
        };

        struct sensor_agg {
          struct spinlock lock;
            struct sensor_reading buf[SENSOR_BUF_SIZE];
              int head;
                int count;
                  long total_temp;
                    long total_energy;
                      long total_air;
                        int  n_temp;
                          int  n_energy;
                            int  n_air;
                            };

                            struct sensor_snapshot {
                              int avg_temp;
                                int avg_energy;
                                  int avg_air;
                                    int total_readings;
                                    };

                                    #endif // SENSORAGG_H
