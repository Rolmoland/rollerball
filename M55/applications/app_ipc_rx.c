#include <rtthread.h>
#include <rtdevice.h>
#include "drv_ipc.h"
#include "app_demo_collector.h"
#include "app_maze_protocol.h"
#include "app_maze_ui.h"
#include "app_mode_manager.h"
#include "module_maze_env.h"

#define IPC_POLL_INTERVAL_MS   20

static rt_int16_t reward_to_tenths(float reward)
{
    rt_int32_t scaled = (rt_int32_t)(reward * 10.0f);

    if (scaled > 32767)
    {
        scaled = 32767;
    }
    if (scaled < -32768)
    {
        scaled = -32768;
    }
    return (rt_int16_t)scaled;
}

static maze_env_action_t direction_to_action(rt_uint16_t direction)
{
    switch (direction)
    {
    case IMU_DIRECTION_UP:    return MAZE_ENV_ACTION_UP;
    case IMU_DIRECTION_DOWN:  return MAZE_ENV_ACTION_DOWN;
    case IMU_DIRECTION_LEFT:  return MAZE_ENV_ACTION_LEFT;
    case IMU_DIRECTION_RIGHT: return MAZE_ENV_ACTION_RIGHT;
    default:                  return MAZE_ENV_ACTION_NONE;
    }
}

static void print_demo_data(const demo_transition_t *transition)
{
    rt_kprintf("DEMO_DATA,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u\n",
               transition->transition_id,
               transition->episode_id,
               transition->step_index,
               transition->map_id,
               transition->state_x,
               transition->state_y,
               transition->action,
               transition->reward_tenths,
               transition->next_x,
               transition->next_y,
               transition->result,
               transition->done);
}

static void publish_ui_state(const maze_env_t *env,
                             maze_env_action_t action,
                             maze_env_step_result_t result,
                             float last_reward,
                             rt_uint32_t revision)
{
    maze_ui_state_t state;

    state.agent_x = env->agent_x;
    state.agent_y = env->agent_y;
    state.map_id = env->map_id;
    state.action = (rt_uint8_t)action;
    state.result = (rt_uint8_t)result;
    state.done = (rt_uint8_t)maze_env_is_done(env);
    state.step_count = env->step_count;
    state.total_steps = env->total_steps;
    state.collision_count = env->collision_count;
    state.last_reward_tenths = reward_to_tenths(last_reward);
    state.total_reward_tenths = reward_to_tenths(env->cumulative_reward);
    state.revision = revision;
    maze_ui_update(&state);
}

static void collect_transition(const maze_env_t *env,
                               rt_uint8_t state_x,
                               rt_uint8_t state_y,
                               maze_env_action_t action,
                               maze_env_step_result_t result,
                               float reward,
                               rt_uint16_t episode_id,
                               rt_uint32_t transition_id)
{
    demo_transition_t transition;

    transition.state_x = state_x;
    transition.state_y = state_y;
    transition.next_x = env->agent_x;
    transition.next_y = env->agent_y;
    transition.map_id = env->map_id;
    transition.action = (rt_uint8_t)action;
    transition.result = (rt_uint8_t)result;
    transition.done = (rt_uint8_t)maze_env_is_done(env);
    transition.episode_id = episode_id;
    transition.step_index = env->total_steps;
    transition.reward_tenths = reward_to_tenths(reward);
    transition.transition_id = transition_id;

    if (demo_collector_append(&transition) == RT_EOK)
    {
        maze_ui_set_demo_count(demo_collector_count());
        print_demo_data(&transition);
    }
}

static void ipc_rx_thread_entry(void *param)
{
    rt_device_t ipc_dev;
    edge_rc_frame_t rx_frame;
    maze_env_t env;
    maze_env_action_t last_action = MAZE_ENV_ACTION_NONE;
    maze_env_step_result_t last_result = MAZE_ENV_STEP_NONE;
    float last_reward = 0.0f;
    rt_uint32_t active_map_revision;
    rt_uint32_t state_revision = 1U;
    rt_uint32_t last_event_sequence = 0U;
    rt_uint32_t transition_id = 0U;
    rt_uint16_t last_reset_generation = 0U;
    rt_uint16_t episode_id = 1U;
    rt_bool_t sender_seen = RT_FALSE;

    (void)param;

    if (maze_env_init(&env, 0U) != RT_EOK)
    {
        rt_kprintf("[IPC-RX] maze environment init failed\n");
        return;
    }
    active_map_revision = maze_env_map_revision();

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
    if (ipc_dev == RT_NULL ||
        rt_device_open(ipc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("[IPC-RX] open failed\n");
        return;
    }

    rt_kprintf("[IPC-RX] listening for IMU gesture events from M33\n");
    rt_kprintf("DEMO_HEADER,transition_id,episode_id,step_index,map_id,"
               "state_x,state_y,action,reward_x10,next_x,next_y,result,done\n");

    while (1)
    {
        rt_uint32_t map_revision = maze_env_map_revision();

        if (map_revision != active_map_revision)
        {
            if (maze_env_init(&env, 0U) == RT_EOK)
            {
                active_map_revision = map_revision;
                episode_id++;
                last_action = MAZE_ENV_ACTION_NONE;
                last_result = MAZE_ENV_STEP_NONE;
                last_reward = 0.0f;
                state_revision++;
                publish_ui_state(&env, last_action, last_result,
                                 last_reward, state_revision);
            }
        }

        if (rt_device_read(ipc_dev, 0, &rx_frame, 1) == 1)
        {
            if (rx_frame.magic != RC_MAGIC_WORD ||
                rx_frame.role != RC_ROLE_M33 ||
                edge_rc_checksum(&rx_frame) != rx_frame.checksum ||
                rx_frame.channel[IMU_CH_PAYLOAD] !=
                    IMU_GESTURE_PAYLOAD_V1)
            {
                rt_thread_mdelay(IPC_POLL_INTERVAL_MS);
                continue;
            }

            if (!sender_seen)
            {
                last_reset_generation =
                    rx_frame.channel[IMU_CH_RESET_GENERATION];
                sender_seen = RT_TRUE;
            }
            else if (rx_frame.channel[IMU_CH_RESET_GENERATION] !=
                     last_reset_generation)
            {
                last_reset_generation =
                    rx_frame.channel[IMU_CH_RESET_GENERATION];
                if (maze_env_reset(&env) == RT_EOK)
                {
                    episode_id++;
                    last_action = MAZE_ENV_ACTION_NONE;
                    last_result = MAZE_ENV_STEP_NONE;
                    last_reward = 0.0f;
                    state_revision++;
                }
            }

            if (rx_frame.seq != last_event_sequence)
            {
                app_mode_state_t mode_state;
                maze_env_action_t action = direction_to_action(
                    rx_frame.channel[IMU_CH_DIRECTION]);

                last_event_sequence = rx_frame.seq;
                if (action < MAZE_ENV_ACTION_COUNT &&
                    app_mode_get_state(&mode_state) == RT_EOK &&
                    mode_state.mode == APP_MODE_DEMO &&
                    !maze_env_is_done(&env))
                {
                    rt_uint8_t state_x = env.agent_x;
                    rt_uint8_t state_y = env.agent_y;

                    last_action = action;
                    last_result = maze_env_step(&env, action, &last_reward);
                    state_revision++;
                    transition_id++;
                    collect_transition(&env, state_x, state_y,
                                       last_action, last_result, last_reward,
                                       episode_id, transition_id);
                }
            }

            publish_ui_state(&env, last_action, last_result,
                             last_reward, state_revision);
        }
        rt_thread_mdelay(IPC_POLL_INTERVAL_MS);
    }
}

static int app_ipc_rx_init(void)
{
    rt_thread_t thread = rt_thread_create("ipc_rx",
                                          ipc_rx_thread_entry,
                                          RT_NULL,
                                          1536,
                                          17,
                                          10);
    if (thread == RT_NULL)
    {
        rt_kprintf("[IPC-RX] thread create failed\n");
        return -RT_ERROR;
    }
    rt_thread_startup(thread);
    return RT_EOK;
}
INIT_APP_EXPORT(app_ipc_rx_init);
