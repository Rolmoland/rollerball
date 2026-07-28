#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "drv_ipc.h"
#include "app_room.h"

#define IPC_SEND_INTERVAL_MS   50

static void ipc_tx_thread_entry(void *param)
{
    rt_device_t ipc_dev;
    edge_rc_frame_t tx_frame;
    rt_uint32_t seq = 0;

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

    rt_kprintf("[IPC-TX] sending ball position to M55\n");

    while (1)
    {
        rt_int16_t x, y;
        room_get_pos(&x, &y);

        memset(&tx_frame, 0, sizeof(tx_frame));
        tx_frame.client_id  = CM55_IPC_PIPE_CLIENT_ID;
        tx_frame.role       = RC_ROLE_M33;
        tx_frame.magic      = RC_MAGIC_WORD;
        tx_frame.seq        = ++seq;
        tx_frame.channel[0] = (rt_uint16_t)x;
        tx_frame.channel[1] = (rt_uint16_t)y;
        tx_frame.checksum   = edge_rc_checksum(&tx_frame);

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
