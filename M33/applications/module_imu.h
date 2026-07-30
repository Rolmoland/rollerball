#ifndef __MODULE_IMU_H__
#define __MODULE_IMU_H__

#include <rtthread.h>

typedef struct
{
    float accel[3];  /* 加速度，单位 mg，索引 0/1/2 对应 X/Y/Z 轴。 */
    float gyro[3];   /* 角速度，单位 mdps，索引 0/1/2 对应 X/Y/Z 轴。 */
} imu_data_t;

/**
 * @brief 获取最近一次 IMU 采样的线程安全快照。
 * @param[out] out 接收三轴加速度和三轴角速度，不得为 RT_NULL。
 */
void imu_get_data(imu_data_t *out);

#endif /* __MODULE_IMU_H__ */
