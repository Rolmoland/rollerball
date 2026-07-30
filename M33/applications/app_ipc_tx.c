#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "drv_ipc.h"
#include "app_gesture.h"
#include "app_maze.h"
#include "app_maze_protocol.h"

#define IPC_SEND_INTERVAL_MS   50

static rt_uint16_t gesture_to_wire(gesture_dir_t gesture)
{
    switch (gesture)
    {
    case GESTURE_UP:    return IMU_DIRECTION_UP;
    case GESTURE_DOWN:  return IMU_DIRECTION_DOWN;
    case GESTURE_LEFT:  return IMU_DIRECTION_LEFT;
    case GESTURE_RIGHT: return IMU_DIRECTION_RIGHT;
    default:            return IMU_DIRECTION_NONE;
    }
}

static void ipc_tx_thread_entry(void *param)
{
    rt_device_t ipc_dev;
    edge_rc_frame_t tx_frame;
    rt_bool_t gesture_armed = RT_TRUE;
    rt_uint32_t event_sequence = 0U;

    ipc_dev = edge_ipc_device_find();
    if (ipc_dev == RT_NULL)
    {
        if (edge_ipc_device_register() != RT_EOK)
        {
            rt_kprintf("[IPC-TX] register failed\n");
            return;
        }
        ipc_dev = edge_ipc_device_find();
    }
    if (ipc_dev == RT_NULL || rt_device_open(ipc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("[IPC-TX] open failed\n");
        return;
    }

    rt_kprintf("[IPC-TX] sending IMU gesture events to M55\n");

    while (1)
    {
        gesture_dir_t gesture = gesture_get();

        if (gesture == GESTURE_NONE)
        {
            gesture_armed = RT_TRUE;
        }
        else if (gesture_armed)
        {
            event_sequence++;
            gesture_armed = RT_FALSE;
        }

        memset(&tx_frame, 0, sizeof(tx_frame));
        tx_frame.client_id = CM55_IPC_PIPE_CLIENT_ID;
        tx_frame.role = RC_ROLE_M33;
        tx_frame.magic = RC_MAGIC_WORD;
        tx_frame.seq = event_sequence;
        tx_frame.channel[IMU_CH_DIRECTION] = gesture_to_wire(gesture);
        tx_frame.channel[IMU_CH_RESET_GENERATION] =
            maze_app_reset_generation();
        tx_frame.channel[IMU_CH_PAYLOAD] = IMU_GESTURE_PAYLOAD_V1;
        tx_frame.checksum = edge_rc_checksum(&tx_frame);
        rt_device_write(ipc_dev, 0, &tx_frame, 1);

        rt_thread_mdelay(IPC_SEND_INTERVAL_MS);
    }
}

static int app_ipc_tx_init(void)
{
    rt_thread_t t = rt_thread_create("ipc_tx",
                                     ipc_tx_thread_entry,
                                     RT_NULL,
                                     1024,
                                     18,
                                     10);
    if (t == RT_NULL)
    {
        rt_kprintf("[IPC-TX] thread create failed\n");
        return -RT_ERROR;
    }
    rt_thread_startup(t);
    return RT_EOK;
}
INIT_APP_EXPORT(app_ipc_tx_init);
