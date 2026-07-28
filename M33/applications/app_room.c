#include <rtthread.h>
#include "app_gesture.h"
#include "app_room.h"

/* Room bounds in pixels, relative to center - keep in sync with
 * M55's app_room_ui.c (two independent firmware images, no shared header) */
#define ROOM_HALF_W_PX  200
#define ROOM_HALF_H_PX  350
#define ROOM_STEP_PX    6
#define ROOM_SAMPLE_MS  50

static rt_int16_t     g_room_x = 0;
static rt_int16_t     g_room_y = 0;
static struct rt_mutex g_room_lock;

static rt_int16_t clamp16(rt_int16_t v, rt_int16_t lo, rt_int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void room_thread_entry(void *param)
{
    while (1)
    {
        gesture_dir_t g = gesture_get();

        rt_mutex_take(&g_room_lock, RT_WAITING_FOREVER);

        switch (g)
        {
        case GESTURE_UP:    g_room_y -= ROOM_STEP_PX; break;
        case GESTURE_DOWN:  g_room_y += ROOM_STEP_PX; break;
        case GESTURE_LEFT:  g_room_x -= ROOM_STEP_PX; break;
        case GESTURE_RIGHT: g_room_x += ROOM_STEP_PX; break;
        default: break;
        }

        g_room_x = clamp16(g_room_x, -ROOM_HALF_W_PX, ROOM_HALF_W_PX);
        g_room_y = clamp16(g_room_y, -ROOM_HALF_H_PX, ROOM_HALF_H_PX);

        rt_mutex_release(&g_room_lock);

        rt_thread_mdelay(ROOM_SAMPLE_MS);
    }
}

void room_get_pos(rt_int16_t *x, rt_int16_t *y)
{
    RT_ASSERT(x != RT_NULL && y != RT_NULL);
    rt_mutex_take(&g_room_lock, RT_WAITING_FOREVER);
    *x = g_room_x;
    *y = g_room_y;
    rt_mutex_release(&g_room_lock);
}

static void room_print(void)
{
    rt_int16_t x, y;
    room_get_pos(&x, &y);
    rt_kprintf("[ROOM] x=%d y=%d (bounds: x=[%d,%d] y=[%d,%d])\n",
               x, y, -ROOM_HALF_W_PX, ROOM_HALF_W_PX, -ROOM_HALF_H_PX, ROOM_HALF_H_PX);
}
MSH_CMD_EXPORT(room_print, Print current ball position and room bounds);

static int app_room_init(void)
{
    rt_mutex_init(&g_room_lock, "room_lock", RT_IPC_FLAG_PRIO);

    rt_thread_t t = rt_thread_create("room",
                                     room_thread_entry,
                                     RT_NULL,
                                     1024,
                                     17,
                                     10);
    if (t == RT_NULL)
    {
        rt_kprintf("[ROOM] ERROR: thread create failed\n");
        return -RT_ERROR;
    }
    rt_thread_startup(t);
    return RT_EOK;
}
INIT_APP_EXPORT(app_room_init);
