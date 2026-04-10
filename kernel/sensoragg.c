// kernel/sensoragg.c - GreenXV6 Feature 4
// Environmental sensor aggregator with spinlock synchronization.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sensoragg.h"

static struct sensor_agg agg;

void
sensoragg_init(void)
{
    initlock(&agg.lock, "sensoragg");
  agg.head = 0;
  agg.count = 0;
  agg.n_temp = 0;
  agg.n_energy = 0;
  agg.n_air = 0;
  agg.total_temp = 0;
  agg.total_energy = 0;
  agg.total_air = 0;
}

int
sensoragg_write(int type, int value)
{
    if(type < SENSOR_TYPE_TEMP || type > SENSOR_TYPE_AIR)
          return -1;

  acquire(&agg.lock);

  struct sensor_reading *r = &agg.buf[agg.head % SENSOR_BUF_SIZE];
  r->type = type;
  r->value = value;
  r->pid = myproc() ? myproc()->pid : -1;
  r->timestamp = ticks;

  agg.head = (agg.head + 1) % SENSOR_BUF_SIZE;
  agg.count++;

  switch(type) {
case SENSOR_TYPE_TEMP:
    agg.total_temp += value;
    agg.n_temp++;
    break;
case SENSOR_TYPE_ENERGY:
    agg.total_energy += value;
    agg.n_energy++;
    break;
case SENSOR_TYPE_AIR:
    agg.total_air += value;
    agg.n_air++;
    break;
  }

  release(&agg.lock);
  return 0;
}

int
sensoragg_read(struct sensor_snapshot *snap)
{
    acquire(&agg.lock);

  snap->avg_temp   = (agg.n_temp > 0)   ? (int)(agg.total_temp / agg.n_temp)     : -1;
  snap->avg_energy = (agg.n_energy > 0)  ? (int)(agg.total_energy / agg.n_energy) : -1;
  snap->avg_air    = (agg.n_air > 0)     ? (int)(agg.total_air / agg.n_air)       : -1;
  snap->total_readings = agg.count;

  release(&agg.lock);
  return 0;
}
