#ifndef __MODULE_MAZE_ENGINE_H__
#define __MODULE_MAZE_ENGINE_H__

#include <stdint.h>

#define MAZE_SIZE 10

typedef enum
{
    ACTION_UP = 0,
    ACTION_DOWN,
    ACTION_LEFT,
    ACTION_RIGHT,
    ACTION_NONE,
} maze_action_t;

typedef enum
{
    CELL_EMPTY = 0,
    CELL_WALL,
    CELL_START,
    CELL_GOAL,
    CELL_AGENT,
    CELL_VISITED,
} maze_cell_t;

typedef enum
{
    MAZE_STEP_MOVED = 0,
    MAZE_STEP_COLLISION,
    MAZE_STEP_GOAL,
    MAZE_STEP_NONE,
} maze_step_result_t;

typedef struct
{
    uint8_t grid[MAZE_SIZE][MAZE_SIZE];
    uint8_t agent_x;
    uint8_t agent_y;
    uint8_t start_x;
    uint8_t start_y;
    uint8_t goal_x;
    uint8_t goal_y;
    uint16_t step_count;
    uint16_t total_steps;
    uint16_t collision_count;
    uint8_t visited[MAZE_SIZE][MAZE_SIZE];
    uint8_t current_map_id;
    float cumulative_reward;
} maze_t;

void maze_init(maze_t *maze, uint8_t map_id);
int maze_step(maze_t *maze, maze_action_t action, float *reward);
void maze_get_state(const maze_t *maze, float state[2]);
int maze_is_done(const maze_t *maze);
void maze_reset_visited(maze_t *maze);
void maze_get_neighbors(const maze_t *maze, int *valid_actions, int *count);

#endif /* __MODULE_MAZE_ENGINE_H__ */
