#include <rtthread.h>
#include <string.h>
#include "app_demo_collector.h"
#include "app_maze_ui.h"

static demo_transition_t s_transitions[DEMO_MAX_TRANSITIONS];
static struct rt_mutex s_collector_lock;
static rt_uint16_t s_transition_count;
static rt_uint32_t s_dropped_count;
static rt_uint32_t s_duplicate_count;

static rt_bool_t transition_exists(rt_uint32_t transition_id)
{
    rt_uint16_t index;

    for (index = 0; index < s_transition_count; index++)
    {
        if (s_transitions[index].transition_id == transition_id)
        {
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

rt_err_t demo_collector_append(const demo_transition_t *transition)
{
    RT_ASSERT(transition != RT_NULL);

    rt_mutex_take(&s_collector_lock, RT_WAITING_FOREVER);
    if (transition_exists(transition->transition_id))
    {
        s_duplicate_count++;
        rt_mutex_release(&s_collector_lock);
        return -RT_ERROR;
    }
    if (s_transition_count >= DEMO_MAX_TRANSITIONS)
    {
        s_dropped_count++;
        rt_mutex_release(&s_collector_lock);
        return -RT_EFULL;
    }

    s_transitions[s_transition_count] = *transition;
    s_transition_count++;
    rt_mutex_release(&s_collector_lock);
    return RT_EOK;
}

rt_err_t demo_collector_get(rt_uint16_t index, demo_transition_t *transition)
{
    RT_ASSERT(transition != RT_NULL);

    rt_mutex_take(&s_collector_lock, RT_WAITING_FOREVER);
    if (index >= s_transition_count)
    {
        rt_mutex_release(&s_collector_lock);
        return -RT_EINVAL;
    }

    *transition = s_transitions[index];
    rt_mutex_release(&s_collector_lock);
    return RT_EOK;
}

rt_uint16_t demo_collector_count(void)
{
    rt_uint16_t count;

    rt_mutex_take(&s_collector_lock, RT_WAITING_FOREVER);
    count = s_transition_count;
    rt_mutex_release(&s_collector_lock);
    return count;
}

void demo_collector_clear(void)
{
    rt_mutex_take(&s_collector_lock, RT_WAITING_FOREVER);
    memset(s_transitions, 0, sizeof(s_transitions));
    s_transition_count = 0;
    s_dropped_count = 0;
    s_duplicate_count = 0;
    rt_mutex_release(&s_collector_lock);
}

static void demo_stats(void)
{
    rt_uint16_t count;
    rt_uint32_t dropped;
    rt_uint32_t duplicates;
    demo_transition_t last;

    rt_mutex_take(&s_collector_lock, RT_WAITING_FOREVER);
    count = s_transition_count;
    dropped = s_dropped_count;
    duplicates = s_duplicate_count;
    if (count > 0)
    {
        last = s_transitions[count - 1U];
    }
    rt_mutex_release(&s_collector_lock);

    rt_kprintf("[DEMO] count=%u/%u dropped=%u duplicate=%u\n",
               count, DEMO_MAX_TRANSITIONS, dropped, duplicates);
    if (count > 0)
    {
        rt_kprintf("[DEMO] last id=%u episode=%u step=%u (%u,%u)->(%u,%u) "
                   "action=%u result=%u reward_x10=%d done=%u\n",
                   last.transition_id, last.episode_id, last.step_index,
                   last.state_x, last.state_y, last.next_x, last.next_y,
                   last.action, last.result, last.reward_tenths, last.done);
    }
}
MSH_CMD_EXPORT(demo_stats, Print demonstration collection statistics);

static void demo_clear(void)
{
    demo_collector_clear();
    maze_ui_set_demo_count(demo_collector_count());
    rt_kprintf("[DEMO] cleared\n");
}
MSH_CMD_EXPORT(demo_clear, Clear demonstration data in RAM);

static int app_demo_collector_init(void)
{
    rt_mutex_init(&s_collector_lock, "demo_lock", RT_IPC_FLAG_PRIO);
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(app_demo_collector_init);
