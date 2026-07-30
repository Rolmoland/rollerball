#ifndef __APP_MAZE_UI_H__
#define __APP_MAZE_UI_H__

#include <rtthread.h>

typedef struct
{
    rt_uint8_t agent_x;
    rt_uint8_t agent_y;
    rt_uint8_t map_id;
    rt_uint8_t action;
    rt_uint8_t result;
    rt_uint8_t done;
    rt_uint16_t step_count;
    rt_uint16_t total_steps;
    rt_uint16_t collision_count;
    rt_int16_t last_reward_tenths;
    rt_int16_t total_reward_tenths;
    rt_uint32_t revision;
} maze_ui_state_t;

void maze_ui_init(void);
void maze_ui_update(const maze_ui_state_t *state);
void maze_ui_set_demo_count(rt_uint16_t count);
rt_err_t maze_ui_reload_map(void);

#endif /* __APP_MAZE_UI_H__ */
