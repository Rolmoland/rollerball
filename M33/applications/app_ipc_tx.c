#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "drv_ipc.h"
#include "app_maze.h"
#include "app_maze_protocol.h"

#define IPC_SEND_INTERVAL_MS   50

static rt_int16_t reward_to_wire(float reward)
{
    rt_int32_t scaled = (rt_int32_t)(reward * 10.0f);

    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    return (rt_int16_t)scaled;
}

static void ipc_tx_thread_entry(void *param)
{
    rt_device_t ipc_dev;
    edge_rc_frame_t tx_frame;
    maze_transition_t pending_transition;
    rt_bool_t has_pending_transition = RT_FALSE;

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

    rt_kprintf("[IPC-TX] sending maze state to M55\n");

    while (1)
    {
        if (!has_pending_transition &&
            maze_app_take_transition(&pending_transition) == RT_EOK)
        {
            has_pending_transition = RT_TRUE;
        }

        if (has_pending_transition)
        {
            memset(&tx_frame, 0, sizeof(tx_frame));
            tx_frame.client_id = CM55_IPC_PIPE_CLIENT_ID;
            tx_frame.role = RC_ROLE_M33;
            tx_frame.magic = RC_MAGIC_WORD;
            tx_frame.seq = pending_transition.transition_id;
            tx_frame.channel[DEMO_CH_STATE] =
                MAZE_PACK_POSITION(pending_transition.state_x,
                                   pending_transition.state_y);
            tx_frame.channel[DEMO_CH_NEXT_STATE] =
                MAZE_PACK_POSITION(pending_transition.next_x,
                                   pending_transition.next_y);
            tx_frame.channel[DEMO_CH_STATUS] =
                MAZE_PACK_STATUS(pending_transition.map_id,
                                 pending_transition.action,
                                 pending_transition.result,
                                 pending_transition.done);
            tx_frame.channel[DEMO_CH_EPISODE] = pending_transition.episode_id;
            tx_frame.channel[DEMO_CH_STEP_INDEX] = pending_transition.step_index;
            tx_frame.channel[DEMO_CH_REWARD] =
                (rt_uint16_t)reward_to_wire(pending_transition.reward);
            tx_frame.channel[DEMO_CH_PAYLOAD] = DEMO_PAYLOAD_V1;
            tx_frame.checksum = edge_rc_checksum(&tx_frame);

            if (rt_device_write(ipc_dev, 0, &tx_frame, 1) == 1)
            {
                has_pending_transition = RT_FALSE;
            }
        }
        else
        {
            maze_app_snapshot_t snapshot;

            if (maze_app_get_snapshot(&snapshot) == RT_EOK)
            {
                memset(&tx_frame, 0, sizeof(tx_frame));
                tx_frame.client_id = CM55_IPC_PIPE_CLIENT_ID;
                tx_frame.role = RC_ROLE_M33;
                tx_frame.magic = RC_MAGIC_WORD;
                tx_frame.seq = snapshot.revision;
                tx_frame.channel[MAZE_CH_POSITION] =
                    MAZE_PACK_POSITION(snapshot.agent_x, snapshot.agent_y);
                tx_frame.channel[MAZE_CH_STATUS] =
                    MAZE_PACK_STATUS(snapshot.map_id, snapshot.action,
                                     snapshot.result, snapshot.done);
                tx_frame.channel[MAZE_CH_STEP_COUNT] = snapshot.step_count;
                tx_frame.channel[MAZE_CH_TOTAL_STEPS] = snapshot.total_steps;
                tx_frame.channel[MAZE_CH_COLLISIONS] = snapshot.collision_count;
                tx_frame.channel[MAZE_CH_LAST_REWARD] =
                    (rt_uint16_t)reward_to_wire(snapshot.last_reward);
                tx_frame.channel[MAZE_CH_TOTAL_REWARD] =
                    (rt_uint16_t)reward_to_wire(snapshot.cumulative_reward);
                tx_frame.channel[MAZE_CH_PAYLOAD] = MAZE_PAYLOAD_V1;
                tx_frame.checksum = edge_rc_checksum(&tx_frame);

                rt_device_write(ipc_dev, 0, &tx_frame, 1);
            }
        }

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
