#ifndef __MODULE_MAZE_ENV_H__
#define __MODULE_MAZE_ENV_H__

#include <rtthread.h>

#define MAZE_ENV_SIZE                 10U
#define MAZE_ENV_MAP_COUNT            1U
#define MAZE_ENV_STATE_COUNT          (MAZE_ENV_SIZE * MAZE_ENV_SIZE)
#define MAZE_ENV_ACTION_COUNT         4U
#define MAZE_ENV_INVALID_STATE        0xFFFFU

typedef enum
{
    MAZE_ENV_ACTION_UP = 0,
    MAZE_ENV_ACTION_DOWN,
    MAZE_ENV_ACTION_LEFT,
    MAZE_ENV_ACTION_RIGHT,
    MAZE_ENV_ACTION_NONE,
} maze_env_action_t;

typedef enum
{
    MAZE_ENV_CELL_EMPTY = 0,
    MAZE_ENV_CELL_WALL,
    MAZE_ENV_CELL_START,
    MAZE_ENV_CELL_GOAL,
    MAZE_ENV_CELL_INVALID = 0xFF,
} maze_env_cell_t;

typedef enum
{
    MAZE_ENV_STEP_MOVED = 0,
    MAZE_ENV_STEP_COLLISION,
    MAZE_ENV_STEP_GOAL,
    MAZE_ENV_STEP_NONE,
} maze_env_step_result_t;

typedef struct
{
    rt_uint8_t agent_x;
    rt_uint8_t agent_y;
    rt_uint8_t start_x;
    rt_uint8_t start_y;
    rt_uint8_t goal_x;
    rt_uint8_t goal_y;
    rt_uint8_t map_id;
    rt_uint8_t visited[MAZE_ENV_SIZE][MAZE_ENV_SIZE];
    rt_uint16_t step_count;
    rt_uint16_t total_steps;
    rt_uint16_t collision_count;
    float cumulative_reward;
} maze_env_t;

rt_err_t maze_env_init(maze_env_t *env, rt_uint8_t map_id);
rt_err_t maze_env_reset(maze_env_t *env);
maze_env_step_result_t maze_env_step(maze_env_t *env,
                                     maze_env_action_t action,
                                     float *reward);
maze_env_cell_t maze_env_cell_at(rt_uint8_t map_id,
                                 rt_uint8_t x,
                                 rt_uint8_t y);
rt_uint16_t maze_env_state_index(rt_uint8_t x, rt_uint8_t y);
void maze_env_get_normalized_state(const maze_env_t *env, float state[2]);
rt_bool_t maze_env_is_done(const maze_env_t *env);

#endif /* __MODULE_MAZE_ENV_H__ */
