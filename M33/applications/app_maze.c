#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "app_gesture.h"
#include "app_maze.h"

#define MAZE_SAMPLE_MS 50
#define MAZE_RESET_PIN GET_PIN(CYBSP_USER_BTN_PORT_NUM, CYBSP_USER_BTN_PIN)
#define MAZE_TRANSITION_QUEUE_SIZE 16

static maze_t g_maze;
static struct rt_mutex g_maze_lock;
static rt_bool_t g_maze_ready = RT_FALSE;
static rt_uint8_t g_last_action = ACTION_NONE;
static rt_uint8_t g_last_result = MAZE_STEP_NONE;
static float g_last_reward = 0.0f;
static rt_uint32_t g_revision = 0;
static volatile rt_bool_t g_reset_requested = RT_FALSE;
static maze_transition_t g_transition_queue[MAZE_TRANSITION_QUEUE_SIZE];
static rt_uint8_t g_transition_head = 0;
static rt_uint8_t g_transition_tail = 0;
static rt_uint8_t g_transition_count = 0;
static rt_uint16_t g_episode_id = 1;
static rt_uint32_t g_transition_id = 0;

static void maze_reset_button_irq(void *args)
{
    (void)args;
    g_reset_requested = RT_TRUE;
}

static maze_action_t gesture_to_action(gesture_dir_t gesture)
{
    switch (gesture)
    {
    case GESTURE_UP:    return ACTION_UP;
    case GESTURE_DOWN:  return ACTION_DOWN;
    case GESTURE_LEFT:  return ACTION_LEFT;
    case GESTURE_RIGHT: return ACTION_RIGHT;
    default:            return ACTION_NONE;
    }
}

static void transition_push(const maze_transition_t *transition)
{
    if (g_transition_count == MAZE_TRANSITION_QUEUE_SIZE)
    {
        g_transition_tail = (g_transition_tail + 1U) % MAZE_TRANSITION_QUEUE_SIZE;
        g_transition_count--;
    }

    g_transition_queue[g_transition_head] = *transition;
    g_transition_head = (g_transition_head + 1U) % MAZE_TRANSITION_QUEUE_SIZE;
    g_transition_count++;
}

static void maze_thread_entry(void *parameter)
{
    rt_bool_t armed = RT_TRUE;
    rt_base_t previous_button = PIN_HIGH;
    (void)parameter;

    while (1)
    {
        rt_base_t button = rt_pin_read(MAZE_RESET_PIN);
        gesture_dir_t gesture = gesture_get();

        if (g_reset_requested || (previous_button == PIN_HIGH && button == PIN_LOW))
        {
            g_reset_requested = RT_FALSE;
            maze_app_reset();
            armed = RT_FALSE;
            rt_kprintf("[MAZE] reset by SW2\n");
        }
        previous_button = button;

        if (gesture == GESTURE_NONE)
        {
            armed = RT_TRUE;
        }
        else if (armed)
        {
            maze_action_t action = gesture_to_action(gesture);

            rt_mutex_take(&g_maze_lock, RT_WAITING_FOREVER);
            if (!maze_is_done(&g_maze))
            {
                maze_transition_t transition;

                transition.state_x = g_maze.agent_x;
                transition.state_y = g_maze.agent_y;
                g_last_action = action;
                g_last_result = (rt_uint8_t)maze_step(&g_maze, action, &g_last_reward);
                transition.next_x = g_maze.agent_x;
                transition.next_y = g_maze.agent_y;
                transition.map_id = g_maze.current_map_id;
                transition.action = g_last_action;
                transition.result = g_last_result;
                transition.done = (rt_uint8_t)maze_is_done(&g_maze);
                transition.episode_id = g_episode_id;
                transition.step_index = g_maze.total_steps;
                transition.reward = g_last_reward;
                transition.transition_id = ++g_transition_id;
                transition_push(&transition);
                g_revision++;
            }
            rt_mutex_release(&g_maze_lock);
            armed = RT_FALSE;
        }

        rt_thread_mdelay(MAZE_SAMPLE_MS);
    }
}

rt_err_t maze_app_get_snapshot(maze_app_snapshot_t *snapshot)
{
    RT_ASSERT(snapshot != RT_NULL);

    if (!g_maze_ready)
    {
        return -RT_EBUSY;
    }

    rt_mutex_take(&g_maze_lock, RT_WAITING_FOREVER);
    snapshot->agent_x = g_maze.agent_x;
    snapshot->agent_y = g_maze.agent_y;
    snapshot->map_id = g_maze.current_map_id;
    snapshot->action = g_last_action;
    snapshot->result = g_last_result;
    snapshot->done = (rt_uint8_t)maze_is_done(&g_maze);
    snapshot->step_count = g_maze.step_count;
    snapshot->total_steps = g_maze.total_steps;
    snapshot->collision_count = g_maze.collision_count;
    snapshot->last_reward = g_last_reward;
    snapshot->cumulative_reward = g_maze.cumulative_reward;
    snapshot->revision = g_revision;
    rt_mutex_release(&g_maze_lock);

    return RT_EOK;
}

rt_err_t maze_app_take_transition(maze_transition_t *transition)
{
    RT_ASSERT(transition != RT_NULL);

    if (!g_maze_ready)
    {
        return -RT_EBUSY;
    }

    rt_mutex_take(&g_maze_lock, RT_WAITING_FOREVER);
    if (g_transition_count == 0)
    {
        rt_mutex_release(&g_maze_lock);
        return -RT_ERROR;
    }

    *transition = g_transition_queue[g_transition_tail];
    g_transition_tail = (g_transition_tail + 1U) % MAZE_TRANSITION_QUEUE_SIZE;
    g_transition_count--;
    rt_mutex_release(&g_maze_lock);
    return RT_EOK;
}

void maze_app_reset(void)
{
    rt_mutex_take(&g_maze_lock, RT_WAITING_FOREVER);
    maze_init(&g_maze, 0);
    g_last_action = ACTION_NONE;
    g_last_result = MAZE_STEP_NONE;
    g_last_reward = 0.0f;
    g_episode_id++;
    g_revision++;
    rt_mutex_release(&g_maze_lock);
}

static void maze_reset_cmd(void)
{
    maze_app_reset();
    rt_kprintf("[MAZE] reset to map 0\n");
}
MSH_CMD_EXPORT_ALIAS(maze_reset_cmd, maze_reset, Reset maze to the start position);

static void maze_print(void)
{
    maze_app_snapshot_t snapshot;

    if (maze_app_get_snapshot(&snapshot) != RT_EOK)
    {
        rt_kprintf("[MAZE] not ready\n");
        return;
    }

    rt_kprintf("[MAZE] pos=(%u,%u) step=%u total=%u hit=%u reward_x10=%d done=%u\n",
               snapshot.agent_x, snapshot.agent_y,
               snapshot.step_count, snapshot.total_steps, snapshot.collision_count,
               (int)(snapshot.cumulative_reward * 10.0f),
               snapshot.done);
}
MSH_CMD_EXPORT(maze_print, Print current maze state);

static int app_maze_init(void)
{
    rt_thread_t thread;

    rt_mutex_init(&g_maze_lock, "maze_lock", RT_IPC_FLAG_PRIO);
    rt_pin_mode(MAZE_RESET_PIN, PIN_MODE_INPUT_PULLUP);
    if (rt_pin_attach_irq(MAZE_RESET_PIN, PIN_IRQ_MODE_FALLING,
                          maze_reset_button_irq, RT_NULL) == RT_EOK)
    {
        rt_pin_irq_enable(MAZE_RESET_PIN, PIN_IRQ_ENABLE);
    }
    maze_init(&g_maze, 0);
    g_revision = 1;
    g_maze_ready = RT_TRUE;

    thread = rt_thread_create("maze", maze_thread_entry, RT_NULL, 1024, 17, 10);
    if (thread == RT_NULL)
    {
        rt_kprintf("[MAZE] thread create failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
INIT_APP_EXPORT(app_maze_init);
