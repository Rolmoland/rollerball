#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include "app_algorithm_manager.h"
#include "app_demo_collector.h"
#include "app_dqn_training.h"
#include "app_maze_ui.h"
#include "app_q_training.h"
#include "app_random_baseline.h"
#include "module_maze_env.h"

#define MAP_ALL_ROWS_MASK ((1U << MAZE_ENV_SIZE) - 1U)
#define MAP_ROWS_PER_CHUNK 5U
#define MAP_CHUNK_TEXT_LENGTH \
    (MAP_ROWS_PER_CHUNK * MAZE_ENV_SIZE + MAP_ROWS_PER_CHUNK - 1U)
#define MAP_FIRST_CHUNK_MASK ((1U << MAP_ROWS_PER_CHUNK) - 1U)

static rt_uint8_t s_staged_map[MAZE_ENV_SIZE][MAZE_ENV_SIZE];
static rt_uint16_t s_received_rows;
static rt_bool_t s_load_active;

static rt_uint8_t received_row_count(void)
{
    rt_uint16_t rows = s_received_rows;
    rt_uint8_t count = 0U;

    while (rows != 0U)
    {
        count += rows & 1U;
        rows >>= 1U;
    }
    return count;
}

static const char *validation_error(maze_env_map_validation_t result)
{
    switch (result)
    {
    case MAZE_ENV_MAP_INVALID_CELL:
        return "invalid cell value";
    case MAZE_ENV_MAP_START_COUNT:
        return "map must contain exactly one start cell (2)";
    case MAZE_ENV_MAP_GOAL_COUNT:
        return "map must contain exactly one goal cell (3)";
    case MAZE_ENV_MAP_UNREACHABLE:
        return "goal is unreachable from start";
    default:
        return "unknown validation error";
    }
}

static int commit_staged_map(void)
{
    maze_env_map_validation_t validation;
    rt_uint16_t shortest_path;
    rt_uint8_t row;

    if (!s_load_active)
    {
        rt_kprintf("[MAP] run map_begin or send map_rows 0 first\n");
        return -RT_ERROR;
    }
    if (s_received_rows != MAP_ALL_ROWS_MASK)
    {
        rt_kprintf("[MAP] missing rows:");
        for (row = 0U; row < MAZE_ENV_SIZE; row++)
        {
            if ((s_received_rows & (1U << row)) == 0U)
            {
                rt_kprintf(" %u", (unsigned int)row);
            }
        }
        rt_kprintf("\n");
        return -RT_EEMPTY;
    }

    validation = maze_env_validate_map(s_staged_map, &shortest_path);
    if (validation != MAZE_ENV_MAP_VALID)
    {
        rt_kprintf("[MAP] rejected: %s\n", validation_error(validation));
        return -RT_EINVAL;
    }
    if (app_algorithm_is_busy())
    {
        rt_kprintf("[MAP] rejected: algorithm task is busy\n");
        return -RT_EBUSY;
    }

    validation = maze_env_set_map(s_staged_map, &shortest_path);
    if (validation != MAZE_ENV_MAP_VALID)
    {
        rt_kprintf("[MAP] rejected: %s\n", validation_error(validation));
        return -RT_ERROR;
    }
    if (q_training_reset() != RT_EOK)
    {
        rt_kprintf("[MAP] accepted but Q state reset failed\n");
        return -RT_ERROR;
    }
    if (random_baseline_reset() != RT_EOK)
    {
        rt_kprintf("[MAP] accepted but random baseline reset failed\n");
        return -RT_ERROR;
    }
    if (dqn_training_reset() != RT_EOK)
    {
        rt_kprintf("[MAP] accepted but DQN state reset failed\n");
        return -RT_ERROR;
    }

    demo_collector_clear();
    maze_ui_set_demo_count(0U);
    s_load_active = RT_FALSE;
    s_received_rows = 0U;

    if (maze_ui_reload_map() != RT_EOK)
    {
        rt_kprintf("[MAP] accepted but LVGL refresh failed\n");
        return -RT_ERROR;
    }

    rt_kprintf("[MAP] accepted revision=%lu shortest_path=%u\n",
               (unsigned long)maze_env_map_revision(),
               (unsigned int)shortest_path);
    rt_kprintf("[MAP] Q table, DQN, random baseline, and demo data cleared\n");
    return RT_EOK;
}

static rt_err_t parse_map_chunk(
    const char *text,
    rt_uint8_t rows[MAP_ROWS_PER_CHUNK][MAZE_ENV_SIZE])
{
    rt_uint8_t row;
    rt_uint8_t col;
    rt_uint8_t index = 0U;

    if (strlen(text) != MAP_CHUNK_TEXT_LENGTH)
    {
        return -RT_EINVAL;
    }

    for (row = 0U; row < MAP_ROWS_PER_CHUNK; row++)
    {
        for (col = 0U; col < MAZE_ENV_SIZE; col++)
        {
            char cell = text[index++];

            if (cell < '0' || cell > '3')
            {
                return -RT_EINVAL;
            }
            rows[row][col] = (rt_uint8_t)(cell - '0');
        }
        if (row + 1U < MAP_ROWS_PER_CHUNK && text[index++] != ',')
        {
            return -RT_EINVAL;
        }
    }
    return RT_EOK;
}

static int map_begin_cmd(int argc, char **argv)
{
    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: map_begin\n");
        return -RT_EINVAL;
    }

    memset(s_staged_map, 0, sizeof(s_staged_map));
    s_received_rows = 0U;
    s_load_active = RT_TRUE;
    rt_kprintf("[MAP] load started; send rows 0-9\n");
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(map_begin_cmd, map_begin, Start a runtime map upload);

static int map_row_cmd(int argc, char **argv)
{
    rt_uint8_t parsed_row[MAZE_ENV_SIZE];
    unsigned long row;
    char *end;
    rt_uint8_t col;

    if (argc != 3)
    {
        rt_kprintf("Usage: map_row <0-9> <10 cells using 0-3>\n");
        return -RT_EINVAL;
    }
    if (!s_load_active)
    {
        rt_kprintf("[MAP] run map_begin first\n");
        return -RT_ERROR;
    }

    row = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || row >= MAZE_ENV_SIZE ||
        strlen(argv[2]) != MAZE_ENV_SIZE)
    {
        rt_kprintf("[MAP] invalid row; expected index 0-9 and 10 cells\n");
        return -RT_EINVAL;
    }

    for (col = 0U; col < MAZE_ENV_SIZE; col++)
    {
        if (argv[2][col] < '0' || argv[2][col] > '3')
        {
            rt_kprintf("[MAP] row %lu contains invalid cell '%c'\n",
                       row, argv[2][col]);
            return -RT_EINVAL;
        }
        parsed_row[col] = (rt_uint8_t)(argv[2][col] - '0');
    }

    memcpy(s_staged_map[row], parsed_row, sizeof(parsed_row));
    s_received_rows |= (rt_uint16_t)(1U << row);
    rt_kprintf("[MAP] row %lu accepted (%u/%u)\n",
               row,
               (unsigned int)received_row_count(),
               (unsigned int)MAZE_ENV_SIZE);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(map_row_cmd, map_row, Upload one runtime map row);

static int map_commit_cmd(int argc, char **argv)
{
    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: map_commit\n");
        return -RT_EINVAL;
    }
    return commit_staged_map();
}
MSH_CMD_EXPORT_ALIAS(map_commit_cmd, map_commit, Validate and activate runtime map);

static int map_rows_cmd(int argc, char **argv)
{
    rt_uint8_t parsed_rows[MAP_ROWS_PER_CHUNK][MAZE_ENV_SIZE];
    unsigned long start_row;
    char *end;
    rt_uint8_t row;

    if (argc != 3)
    {
        rt_kprintf("Usage: map_rows <0|5> <five comma-separated rows>\n");
        return -RT_EINVAL;
    }

    start_row = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' ||
        (start_row != 0UL && start_row != MAP_ROWS_PER_CHUNK) ||
        parse_map_chunk(argv[2], parsed_rows) != RT_EOK)
    {
        rt_kprintf("[MAP] invalid chunk; expected five 10-cell rows\n");
        return -RT_EINVAL;
    }

    if (start_row == 0UL)
    {
        memset(s_staged_map, 0, sizeof(s_staged_map));
        s_received_rows = 0U;
        s_load_active = RT_TRUE;
    }
    else if (!s_load_active ||
             (s_received_rows & MAP_FIRST_CHUNK_MASK) !=
             MAP_FIRST_CHUNK_MASK)
    {
        rt_kprintf("[MAP] send map_rows 0 first\n");
        return -RT_ERROR;
    }

    for (row = 0U; row < MAP_ROWS_PER_CHUNK; row++)
    {
        rt_uint8_t target_row = (rt_uint8_t)start_row + row;

        memcpy(s_staged_map[target_row], parsed_rows[row], MAZE_ENV_SIZE);
        s_received_rows |= (rt_uint16_t)(1U << target_row);
    }

    if (start_row == 0UL)
    {
        return RT_EOK;
    }
    return commit_staged_map();
}
MSH_CMD_EXPORT_ALIAS(map_rows_cmd, map_rows, Upload and activate a map in two chunks);

static int map_abort_cmd(int argc, char **argv)
{
    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: map_abort\n");
        return -RT_EINVAL;
    }

    memset(s_staged_map, 0, sizeof(s_staged_map));
    s_received_rows = 0U;
    s_load_active = RT_FALSE;
    rt_kprintf("[MAP] upload aborted\n");
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(map_abort_cmd, map_abort, Abort runtime map upload);

static int map_show_cmd(int argc, char **argv)
{
    rt_uint8_t map[MAZE_ENV_SIZE][MAZE_ENV_SIZE];
    rt_uint16_t shortest_path = 0U;
    rt_uint8_t start_row;
    rt_uint8_t row_offset;
    rt_uint8_t col;

    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: map_show\n");
        return -RT_EINVAL;
    }
    if (maze_env_copy_map(map) != RT_EOK)
    {
        rt_kprintf("[MAP] active map unavailable\n");
        return -RT_ERROR;
    }

    maze_env_validate_map(map, &shortest_path);
    rt_kprintf("[MAP] revision=%lu shortest_path=%u\n",
               (unsigned long)maze_env_map_revision(),
               (unsigned int)shortest_path);
    for (start_row = 0U; start_row < MAZE_ENV_SIZE;
         start_row += MAP_ROWS_PER_CHUNK)
    {
        rt_kprintf("map_rows %u ", (unsigned int)start_row);
        for (row_offset = 0U; row_offset < MAP_ROWS_PER_CHUNK;
             row_offset++)
        {
            rt_uint8_t row = start_row + row_offset;

            for (col = 0U; col < MAZE_ENV_SIZE; col++)
            {
                rt_kprintf("%u", (unsigned int)map[row][col]);
            }
            if (row_offset + 1U < MAP_ROWS_PER_CHUNK)
            {
                rt_kprintf(",");
            }
        }
        rt_kprintf("\n");
    }
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(map_show_cmd, map_show, Print the active runtime map);
