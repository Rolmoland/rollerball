#include <string.h>
#include "module_maze_env.h"

static const rt_uint8_t s_map_0[MAZE_ENV_SIZE][MAZE_ENV_SIZE] =
{
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 0, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 0},
};

static const rt_uint8_t (*maze_env_map(rt_uint8_t map_id))[MAZE_ENV_SIZE]
{
    if (map_id == 0U)
    {
        return s_map_0;
    }

    return RT_NULL;
}

static rt_bool_t maze_env_target(const maze_env_t *env,
                                 maze_env_action_t action,
                                 int *x,
                                 int *y)
{
    *x = env->agent_x;
    *y = env->agent_y;

    switch (action)
    {
    case MAZE_ENV_ACTION_UP:    (*y)--; break;
    case MAZE_ENV_ACTION_DOWN:  (*y)++; break;
    case MAZE_ENV_ACTION_LEFT:  (*x)--; break;
    case MAZE_ENV_ACTION_RIGHT: (*x)++; break;
    default: return RT_FALSE;
    }

    return RT_TRUE;
}

rt_err_t maze_env_init(maze_env_t *env, rt_uint8_t map_id)
{
    const rt_uint8_t (*map)[MAZE_ENV_SIZE];
    rt_uint8_t x;
    rt_uint8_t y;

    RT_ASSERT(env != RT_NULL);

    map = maze_env_map(map_id);
    if (map == RT_NULL)
    {
        return -RT_EINVAL;
    }

    memset(env, 0, sizeof(*env));
    env->map_id = map_id;
    for (y = 0; y < MAZE_ENV_SIZE; y++)
    {
        for (x = 0; x < MAZE_ENV_SIZE; x++)
        {
            if (map[y][x] == MAZE_ENV_CELL_START)
            {
                env->start_x = x;
                env->start_y = y;
                env->agent_x = x;
                env->agent_y = y;
            }
            else if (map[y][x] == MAZE_ENV_CELL_GOAL)
            {
                env->goal_x = x;
                env->goal_y = y;
            }
        }
    }
    return RT_EOK;
}

rt_err_t maze_env_reset(maze_env_t *env)
{
    rt_uint8_t map_id;

    RT_ASSERT(env != RT_NULL);

    map_id = env->map_id;
    return maze_env_init(env, map_id);
}

maze_env_step_result_t maze_env_step(maze_env_t *env,
                                     maze_env_action_t action,
                                     float *reward)
{
    const rt_uint8_t (*map)[MAZE_ENV_SIZE];
    int next_x;
    int next_y;

    RT_ASSERT(env != RT_NULL);

    if (reward != RT_NULL)
    {
        *reward = 0.0f;
    }
    if (maze_env_is_done(env) ||
        !maze_env_target(env, action, &next_x, &next_y))
    {
        return MAZE_ENV_STEP_NONE;
    }

    map = maze_env_map(env->map_id);
    if (map == RT_NULL)
    {
        return MAZE_ENV_STEP_NONE;
    }

    env->total_steps++;
    if (next_x < 0 || next_x >= (int)MAZE_ENV_SIZE ||
        next_y < 0 || next_y >= (int)MAZE_ENV_SIZE ||
        map[next_y][next_x] == MAZE_ENV_CELL_WALL)
    {
        env->collision_count++;
        env->cumulative_reward -= 2.0f;
        if (reward != RT_NULL)
        {
            *reward = -2.0f;
        }
        return MAZE_ENV_STEP_COLLISION;
    }

    env->agent_x = (rt_uint8_t)next_x;
    env->agent_y = (rt_uint8_t)next_y;
    env->step_count++;

    if (maze_env_is_done(env))
    {
        env->cumulative_reward += 100.0f;
        if (reward != RT_NULL)
        {
            *reward = 100.0f;
        }
        return MAZE_ENV_STEP_GOAL;
    }
    env->cumulative_reward -= 0.1f;
    if (reward != RT_NULL)
    {
        *reward = -0.1f;
    }
    return MAZE_ENV_STEP_MOVED;
}

maze_env_cell_t maze_env_cell_at(rt_uint8_t map_id,
                                 rt_uint8_t x,
                                 rt_uint8_t y)
{
    const rt_uint8_t (*map)[MAZE_ENV_SIZE] = maze_env_map(map_id);

    if (map == RT_NULL || x >= MAZE_ENV_SIZE || y >= MAZE_ENV_SIZE)
    {
        return MAZE_ENV_CELL_INVALID;
    }

    return (maze_env_cell_t)map[y][x];
}

rt_uint16_t maze_env_state_index(rt_uint8_t x, rt_uint8_t y)
{
    if (x >= MAZE_ENV_SIZE || y >= MAZE_ENV_SIZE)
    {
        return MAZE_ENV_INVALID_STATE;
    }

    return (rt_uint16_t)(y * MAZE_ENV_SIZE + x);
}

void maze_env_get_normalized_state(const maze_env_t *env, float state[2])
{
    RT_ASSERT(env != RT_NULL);
    RT_ASSERT(state != RT_NULL);

    state[0] = env->agent_x / (float)(MAZE_ENV_SIZE - 1U);
    state[1] = env->agent_y / (float)(MAZE_ENV_SIZE - 1U);
}

rt_bool_t maze_env_is_done(const maze_env_t *env)
{
    RT_ASSERT(env != RT_NULL);

    return env->agent_x == env->goal_x && env->agent_y == env->goal_y;
}
