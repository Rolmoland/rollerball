#include <rtthread.h>
#include <rtdevice.h>
#include "imu_app.h"

/* LSM6DS3 I2C address: SA0=0 -> 0x6A, SA0=1 -> 0x6B */
#define LSM6DS3_ADDR        0x6A

/* Register map */
#define REG_WHO_AM_I        0x0F
#define REG_CTRL1_XL        0x10  /* Accelerometer config */
#define REG_CTRL2_G         0x11  /* Gyroscope config     */
#define REG_CTRL3_C         0x12  /* General config       */
#define REG_OUTX_L_G        0x22  /* Gyro  output base (6 bytes) */
#define REG_OUTX_L_XL       0x28  /* Accel output base (6 bytes) */

/* CTRL1_XL: ODR=104Hz, FS=±2g   -> 0x40 */
#define CTRL1_XL_VAL        0x40
/* CTRL2_G:  ODR=104Hz, FS=250dps -> 0x40 */
#define CTRL2_G_VAL         0x40
/* CTRL3_C:  IF_INC=1 (auto-increment register address on burst read) */
#define CTRL3_C_VAL         0x04

/* Sensitivity: ±2g -> 0.061 mg/LSB; 250dps -> 8.75 mdps/LSB */
#define ACCEL_SENS          0.061f
#define GYRO_SENS           8.75f

#define IMU_I2C_BUS         "i2c0"
#define IMU_SAMPLE_MS       10

static imu_data_t   g_imu;
static struct rt_mutex g_imu_lock;

/* ------------------------------------------------------------------ */
/* Low-level I2C helpers                                               */
/* ------------------------------------------------------------------ */

static rt_err_t lsm_write(struct rt_i2c_bus_device *bus, rt_uint8_t reg, rt_uint8_t val)
{
    rt_uint8_t buf[2] = {reg, val};
    struct rt_i2c_msg msg = {LSM6DS3_ADDR, RT_I2C_WR, 2, buf};
    return (rt_i2c_transfer(bus, &msg, 1) == 1) ? RT_EOK : -RT_ERROR;
}

static rt_err_t lsm_read(struct rt_i2c_bus_device *bus,
                          rt_uint8_t reg, rt_uint8_t *buf, rt_size_t len)
{
    struct rt_i2c_msg msgs[2] =
    {
        {LSM6DS3_ADDR, RT_I2C_WR, 1, &reg},
        {LSM6DS3_ADDR, RT_I2C_RD, len, buf},
    };
    return (rt_i2c_transfer(bus, msgs, 2) == 2) ? RT_EOK : -RT_ERROR;
}

/* ------------------------------------------------------------------ */
/* IMU thread                                                          */
/* ------------------------------------------------------------------ */

static void imu_thread_entry(void *param)
{
    struct rt_i2c_bus_device *bus;
    rt_uint8_t who_am_i = 0;
    rt_uint8_t raw[12];
    rt_int16_t rg[3], ra[3];

    bus = rt_i2c_bus_device_find(IMU_I2C_BUS);
    if (bus == RT_NULL)
    {
        rt_kprintf("[IMU] ERROR: %s not found\n", IMU_I2C_BUS);
        return;
    }

    if (lsm_read(bus, REG_WHO_AM_I, &who_am_i, 1) != RT_EOK)
    {
        rt_kprintf("[IMU] ERROR: I2C read failed (addr=0x%02X, check wiring)\n", LSM6DS3_ADDR);
        return;
    }
    if (who_am_i != 0x69 && who_am_i != 0x6A)
    {
        rt_kprintf("[IMU] ERROR: WHO_AM_I=0x%02X, expected 0x69/0x6A\n", who_am_i);
        return;
    }
    rt_kprintf("[IMU] LSM6DS3 ready, WHO_AM_I=0x%02X\n", who_am_i);

    lsm_write(bus, REG_CTRL1_XL, CTRL1_XL_VAL);
    lsm_write(bus, REG_CTRL2_G,  CTRL2_G_VAL);
    lsm_write(bus, REG_CTRL3_C,  CTRL3_C_VAL);

    rt_uint32_t print_cnt = 0;

    while (1)
    {
        /* Burst-read 12 bytes: gyro XYZ (6B) + accel XYZ (6B) */
        if (lsm_read(bus, REG_OUTX_L_G, raw, 12) == RT_EOK)
        {
            rg[0] = (rt_int16_t)(raw[1]  << 8 | raw[0]);
            rg[1] = (rt_int16_t)(raw[3]  << 8 | raw[2]);
            rg[2] = (rt_int16_t)(raw[5]  << 8 | raw[4]);
            ra[0] = (rt_int16_t)(raw[7]  << 8 | raw[6]);
            ra[1] = (rt_int16_t)(raw[9]  << 8 | raw[8]);
            ra[2] = (rt_int16_t)(raw[11] << 8 | raw[10]);

            rt_mutex_take(&g_imu_lock, RT_WAITING_FOREVER);
            g_imu.gyro[0]  = rg[0] * GYRO_SENS;
            g_imu.gyro[1]  = rg[1] * GYRO_SENS;
            g_imu.gyro[2]  = rg[2] * GYRO_SENS;
            g_imu.accel[0] = ra[0] * ACCEL_SENS;
            g_imu.accel[1] = ra[1] * ACCEL_SENS;
            g_imu.accel[2] = ra[2] * ACCEL_SENS;
            rt_mutex_release(&g_imu_lock);
        }
        /* Print every 100 ticks = 1 second */
        if (++print_cnt >= 100)
        {
            print_cnt = 0;
            rt_mutex_take(&g_imu_lock, RT_WAITING_FOREVER);
            rt_kprintf("[IMU] Accel(mg) X=%d Y=%d Z=%d | Gyro(mdps) X=%d Y=%d Z=%d\n",
                       (int)g_imu.accel[0], (int)g_imu.accel[1], (int)g_imu.accel[2],
                       (int)g_imu.gyro[0],  (int)g_imu.gyro[1],  (int)g_imu.gyro[2]);
            rt_mutex_release(&g_imu_lock);
        }

        rt_thread_mdelay(IMU_SAMPLE_MS);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void imu_get_data(imu_data_t *out)
{
    RT_ASSERT(out != RT_NULL);
    rt_mutex_take(&g_imu_lock, RT_WAITING_FOREVER);
    *out = g_imu;
    rt_mutex_release(&g_imu_lock);
}

/* ------------------------------------------------------------------ */
/* MSH test command                                                    */
/* ------------------------------------------------------------------ */

static void imu_print(void)
{
    imu_data_t d;
    imu_get_data(&d);
    rt_kprintf("[IMU] Accel(mg)  X=%d Y=%d Z=%d\n",
               (int)d.accel[0], (int)d.accel[1], (int)d.accel[2]);
    rt_kprintf("[IMU] Gyro(mdps) X=%d Y=%d Z=%d\n",
               (int)d.gyro[0],  (int)d.gyro[1],  (int)d.gyro[2]);
}
MSH_CMD_EXPORT(imu_print, Print LSM6DS3 accel and gyro data);

/* ------------------------------------------------------------------ */
/* Auto-start via RT-Thread init framework                             */
/* ------------------------------------------------------------------ */

static int imu_app_init(void)
{
    rt_mutex_init(&g_imu_lock, "imu_lock", RT_IPC_FLAG_PRIO);

    rt_thread_t t = rt_thread_create("imu",
                                     imu_thread_entry,
                                     RT_NULL,
                                     1024,
                                     15,
                                     10);
    if (t == RT_NULL)
    {
        rt_kprintf("[IMU] ERROR: thread create failed\n");
        return -RT_ERROR;
    }
    rt_thread_startup(t);
    return RT_EOK;
}
INIT_APP_EXPORT(imu_app_init);
