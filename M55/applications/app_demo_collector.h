#ifndef __APP_DEMO_COLLECTOR_H__
#define __APP_DEMO_COLLECTOR_H__

#include <rtthread.h>

#define DEMO_MAX_TRANSITIONS 256U

typedef struct
{
    rt_uint8_t state_x;
    rt_uint8_t state_y;
    rt_uint8_t next_x;
    rt_uint8_t next_y;
    rt_uint8_t map_id;
    rt_uint8_t action;
    rt_uint8_t result;
    rt_uint8_t done;
    rt_uint16_t episode_id;
    rt_uint16_t step_index;
    rt_int16_t reward_tenths;
    rt_uint32_t transition_id;
} demo_transition_t;

rt_err_t demo_collector_append(const demo_transition_t *transition);
rt_err_t demo_collector_get(rt_uint16_t index, demo_transition_t *transition);
rt_uint16_t demo_collector_count(void);
void demo_collector_clear(void);

#endif /* __APP_DEMO_COLLECTOR_H__ */
