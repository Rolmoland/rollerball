#ifndef __APP_MAZE_H__
#define __APP_MAZE_H__

#include <rtthread.h>
#include "module_maze_engine.h"

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
    float last_reward;
    float cumulative_reward;
    rt_uint32_t revision;
} maze_app_snapshot_t;

rt_err_t maze_app_get_snapshot(maze_app_snapshot_t *snapshot);
void maze_app_reset(void);

#endif /* __APP_MAZE_H__ */
