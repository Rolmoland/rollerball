#include <rtthread.h>
#include "module_imu.h"
#include "app_gesture.h"

/* Trigger threshold vs. hold threshold (hysteresis avoids chatter at the edge) */
#define TILT_THRESH_ON_MG    300.0f
#define TILT_THRESH_OFF_MG   150.0f
#define GESTURE_SAMPLE_MS    50

static gesture_dir_t   g_gesture = GESTURE_NONE;
static struct rt_mutex g_gesture_lock;

static gesture_dir_t compute_gesture(float ax, float ay, gesture_dir_t prev)
{
    /* Board's physical forward/back axis reads on ax, left/right reads on ay
     * (confirmed on hardware - swapped from the initial guess) */

    /* Hold the current gesture until tilt falls back below the lower threshold */
    if (prev == GESTURE_LEFT  && ay >  TILT_THRESH_OFF_MG) return GESTURE_LEFT;
    if (prev == GESTURE_RIGHT && ay < -TILT_THRESH_OFF_MG) return GESTURE_RIGHT;
    if (prev == GESTURE_UP    && ax < -TILT_THRESH_OFF_MG) return GESTURE_UP;
    if (prev == GESTURE_DOWN  && ax >  TILT_THRESH_OFF_MG) return GESTURE_DOWN;

    if (ay >  TILT_THRESH_ON_MG) return GESTURE_LEFT;
    if (ay < -TILT_THRESH_ON_MG) return GESTURE_RIGHT;
    if (ax < -TILT_THRESH_ON_MG) return GESTURE_UP;
    if (ax >  TILT_THRESH_ON_MG) return GESTURE_DOWN;

    return GESTURE_NONE;
}

static void gesture_thread_entry(void *param)
{
    imu_data_t d;
    gesture_dir_t cur = GESTURE_NONE;

    while (1)
    {
        imu_get_data(&d);
        cur = compute_gesture(d.accel[0], d.accel[1], cur);

        rt_mutex_take(&g_gesture_lock, RT_WAITING_FOREVER);
        g_gesture = cur;
        rt_mutex_release(&g_gesture_lock);

        rt_thread_mdelay(GESTURE_SAMPLE_MS);
    }
}

gesture_dir_t gesture_get(void)
{
    gesture_dir_t g;
    rt_mutex_take(&g_gesture_lock, RT_WAITING_FOREVER);
    g = g_gesture;
    rt_mutex_release(&g_gesture_lock);
    return g;
}

static const char *gesture_name(gesture_dir_t g)
{
    switch (g)
    {
    case GESTURE_UP:    return "UP";
    case GESTURE_DOWN:  return "DOWN";
    case GESTURE_LEFT:  return "LEFT";
    case GESTURE_RIGHT: return "RIGHT";
    default:            return "NONE";
    }
}

static void gesture_print(void)
{
    rt_kprintf("[GESTURE] %s\n", gesture_name(gesture_get()));
}
MSH_CMD_EXPORT(gesture_print, Print current tilt gesture direction);

static int app_gesture_init(void)
{
    rt_mutex_init(&g_gesture_lock, "gesture_lock", RT_IPC_FLAG_PRIO);

    rt_thread_t t = rt_thread_create("gesture",
                                     gesture_thread_entry,
                                     RT_NULL,
                                     1024,
                                     16,
                                     10);
    if (t == RT_NULL)
    {
        rt_kprintf("[GESTURE] ERROR: thread create failed\n");
        return -RT_ERROR;
    }
    rt_thread_startup(t);
    return RT_EOK;
}
INIT_APP_EXPORT(app_gesture_init);
