#include <rtthread.h>
#include <rtdevice.h>
#include "drv_ipc.h"
#include "app_ball_ui.h"

#define IPC_POLL_INTERVAL_MS   20
#define BALL_OFFSET_PX         150

/* Must match the enum order in M33's app_gesture.h */
typedef enum
{
    GESTURE_NONE = 0,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_LEFT,
    GESTURE_RIGHT,
} gesture_dir_t;

static void apply_gesture(gesture_dir_t g)
{
    switch (g)
    {
    case GESTURE_UP:    ball_ui_set_pos(0, -BALL_OFFSET_PX); break;
    case GESTURE_DOWN:  ball_ui_set_pos(0,  BALL_OFFSET_PX); break;
    case GESTURE_LEFT:  ball_ui_set_pos(-BALL_OFFSET_PX, 0); break;
    case GESTURE_RIGHT: ball_ui_set_pos(BALL_OFFSET_PX,  0); break;
    default:            ball_ui_set_pos(0, 0); break;
    }
}

static void ipc_rx_thread_entry(void *param)
{
    rt_device_t ipc_dev;
    edge_rc_frame_t rx_frame;

    ipc_dev = edge_ipc_device_find();
    if (ipc_dev == RT_NULL)
    {
        if (edge_ipc_device_register() != RT_EOK)
        {
            rt_kprintf("[IPC-RX] register failed\n");
            return;
        }
        ipc_dev = edge_ipc_device_find();
    }
    if (ipc_dev == RT_NULL || rt_device_open(ipc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("[IPC-RX] open failed\n");
        return;
    }

    rt_kprintf("[IPC-RX] listening for gesture frames from M33\n");

    while (1)
    {
        if (rt_device_read(ipc_dev, 0, &rx_frame, 1) == 1)
        {
            if (rx_frame.magic == RC_MAGIC_WORD &&
                rx_frame.role  == RC_ROLE_M33 &&
                edge_rc_checksum(&rx_frame) == rx_frame.checksum)
            {
                apply_gesture((gesture_dir_t)rx_frame.channel[0]);
            }
        }
        rt_thread_mdelay(IPC_POLL_INTERVAL_MS);
    }
}

static int app_ipc_rx_init(void)
{
    rt_thread_t t = rt_thread_create("ipc_rx",
                                     ipc_rx_thread_entry,
                                     RT_NULL,
                                     1024,
                                     17,
                                     10);
    if (t == RT_NULL)
    {
        rt_kprintf("[IPC-RX] thread create failed\n");
        return -RT_ERROR;
    }
    rt_thread_startup(t);
    return RT_EOK;
}
INIT_APP_EXPORT(app_ipc_rx_init);
