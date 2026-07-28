#include <string.h>
#include "module_maze_engine.h"

static const uint8_t s_map_0[MAZE_SIZE][MAZE_SIZE] =
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

static int maze_target(const maze_t *maze, maze_action_t action, int *x, int *y)
{
    *x = maze->agent_x;
    *y = maze->agent_y;

    switch (action)
    {
    case ACTION_UP:    (*y)--; break;
    case ACTION_DOWN:  (*y)++; break;
    case ACTION_LEFT:  (*x)--; break;
    case ACTION_RIGHT: (*x)++; break;
    default: return 0;
    }

    return 1;
}

void maze_init(maze_t *maze, uint8_t map_id)
{
    uint8_t x;
    uint8_t y;

    memset(maze, 0, sizeof(*maze));
    memcpy(maze->grid, s_map_0, sizeof(s_map_0));
    maze->current_map_id = 0;
    (void)map_id;

    for (y = 0; y < MAZE_SIZE; y++)
    {
        for (x = 0; x < MAZE_SIZE; x++)
        {
            if (maze->grid[y][x] == CELL_START)
            {
                maze->start_x = x;
                maze->start_y = y;
                maze->agent_x = x;
                maze->agent_y = y;
            }
            else if (maze->grid[y][x] == CELL_GOAL)
            {
                maze->goal_x = x;
                maze->goal_y = y;
            }
        }
    }

    maze->visited[maze->start_y][maze->start_x] = 1;
}

int maze_step(maze_t *maze, maze_action_t action, float *reward)
{
    int next_x;
    int next_y;
    int visited;
    int result;

    if (reward != 0)
    {
        *reward = 0.0f;
    }
    if (!maze_target(maze, action, &next_x, &next_y))
    {
        return MAZE_STEP_NONE;
    }

    maze->total_steps++;

    if (next_x < 0 || next_x >= MAZE_SIZE || next_y < 0 || next_y >= MAZE_SIZE ||
        maze->grid[next_y][next_x] == CELL_WALL)
    {
        maze->collision_count++;
        maze->cumulative_reward -= 2.0f;
        if (reward != 0) *reward = -2.0f;
        return MAZE_STEP_COLLISION;
    }

    visited = maze->visited[next_y][next_x];
    maze->agent_x = (uint8_t)next_x;
    maze->agent_y = (uint8_t)next_y;
    maze->step_count++;
    maze->visited[next_y][next_x] = 1;

    if (maze_is_done(maze))
    {
        result = MAZE_STEP_GOAL;
        maze->cumulative_reward += 100.0f;
        if (reward != 0) *reward = 100.0f;
    }
    else if (visited)
    {
        result = MAZE_STEP_MOVED;
        maze->cumulative_reward -= 0.5f;
        if (reward != 0) *reward = -0.5f;
    }
    else
    {
        result = MAZE_STEP_MOVED;
        maze->cumulative_reward -= 0.1f;
        if (reward != 0) *reward = -0.1f;
    }

    return result;
}

void maze_get_state(const maze_t *maze, float state[2])
{
    state[0] = maze->agent_x / 9.0f;
    state[1] = maze->agent_y / 9.0f;
}

int maze_is_done(const maze_t *maze)
{
    return maze->agent_x == maze->goal_x && maze->agent_y == maze->goal_y;
}

void maze_reset_visited(maze_t *maze)
{
    memset(maze->visited, 0, sizeof(maze->visited));
    maze->visited[maze->agent_y][maze->agent_x] = 1;
}

void maze_get_neighbors(const maze_t *maze, int *valid_actions, int *count)
{
    int action;
    int x;
    int y;

    *count = 0;
    for (action = ACTION_UP; action <= ACTION_RIGHT; action++)
    {
        maze_target(maze, (maze_action_t)action, &x, &y);
        if (x >= 0 && x < MAZE_SIZE && y >= 0 && y < MAZE_SIZE &&
            maze->grid[y][x] != CELL_WALL)
        {
            valid_actions[(*count)++] = action;
        }
    }
}
