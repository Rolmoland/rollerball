#ifndef __MODULE_IMU_H__
#define __MODULE_IMU_H__

#include <rtthread.h>

typedef struct
{
    float accel[3];  /* mg,   0=X  1=Y  2=Z */
    float gyro[3];   /* mdps, 0=X  1=Y  2=Z */
} imu_data_t;

/* Thread-safe snapshot of the latest IMU reading */
void imu_get_data(imu_data_t *out);

#endif /* __MODULE_IMU_H__ */
