#include <rtthread.h>
#include <rtdevice.h>
#include "drv_ipc.h"
#include "app_maze_protocol.h"
#include "app_maze_ui.h"

#define IPC_POLL_INTERVAL_MS   20

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

    rt_kprintf("[IPC-RX] listening for maze state from M33\n");

    while (1)
    {
        if (rt_device_read(ipc_dev, 0, &rx_frame, 1) == 1)
        {
            if (rx_frame.magic == RC_MAGIC_WORD &&
                rx_frame.role  == RC_ROLE_M33 &&
                edge_rc_checksum(&rx_frame) == rx_frame.checksum &&
                rx_frame.channel[MAZE_CH_PAYLOAD] == MAZE_PAYLOAD_V1)
            {
                maze_ui_state_t state;
                rt_uint16_t position = rx_frame.channel[MAZE_CH_POSITION];
                rt_uint16_t status = rx_frame.channel[MAZE_CH_STATUS];

                state.agent_x = MAZE_POSITION_X(position);
                state.agent_y = MAZE_POSITION_Y(position);
                state.map_id = MAZE_STATUS_MAP(status);
                state.action = MAZE_STATUS_ACTION(status);
                state.result = MAZE_STATUS_RESULT(status);
                state.done = MAZE_STATUS_DONE(status);
                state.step_count = rx_frame.channel[MAZE_CH_STEP_COUNT];
                state.total_steps = rx_frame.channel[MAZE_CH_TOTAL_STEPS];
                state.collision_count = rx_frame.channel[MAZE_CH_COLLISIONS];
                state.last_reward_tenths =
                    (rt_int16_t)rx_frame.channel[MAZE_CH_LAST_REWARD];
                state.total_reward_tenths =
                    (rt_int16_t)rx_frame.channel[MAZE_CH_TOTAL_REWARD];
                state.revision = rx_frame.seq;

                maze_ui_update(&state);
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
