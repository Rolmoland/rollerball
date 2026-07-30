#ifndef __MODULE_MAZE_ENV_H__
#define __MODULE_MAZE_ENV_H__

#include <rtthread.h>

#define MAZE_ENV_SIZE                 10U
#define MAZE_ENV_MAP_COUNT            1U
#define MAZE_ENV_STATE_COUNT          (MAZE_ENV_SIZE * MAZE_ENV_SIZE)
#define MAZE_ENV_ACTION_COUNT         4U
#define MAZE_ENV_INVALID_STATE        0xFFFFU
#define MAZE_ENV_DISTANCE_UNREACHABLE 0xFFU

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
    MAZE_ENV_MAP_VALID = 0,
    MAZE_ENV_MAP_INVALID_CELL,
    MAZE_ENV_MAP_START_COUNT,
    MAZE_ENV_MAP_GOAL_COUNT,
    MAZE_ENV_MAP_UNREACHABLE,
} maze_env_map_validation_t;

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
    rt_uint16_t step_count;
    rt_uint16_t total_steps;
    rt_uint16_t collision_count;
    float cumulative_reward;
} maze_env_t;

/**
 * @brief 使用指定活动地图初始化一个独立迷宫环境实例。
 * @param[out] env 待初始化的环境，不得为 RT_NULL。
 * @param[in] map_id 地图编号，当前仅支持 0。
 * @return RT_EOK 表示成功，地图编号无效时返回 -RT_EINVAL。
 */
rt_err_t maze_env_init(maze_env_t *env, rt_uint8_t map_id);

/**
 * @brief 将环境复位到当前活动地图的起点并清空统计。
 * @param[in,out] env 待复位的环境，不得为 RT_NULL。
 * @return RT_EOK 表示成功，否则返回地图初始化错误。
 */
rt_err_t maze_env_reset(maze_env_t *env);

/**
 * @brief 在环境中执行一步动作并更新位置、碰撞、步数和累计奖励。
 * @param[in,out] env 当前迷宫环境。
 * @param[in] action 要执行的方向动作。
 * @param[out] reward 可选的本步奖励输出，允许为 RT_NULL。
 * @return 本步移动、碰撞、到达终点或无动作结果。
 */
maze_env_step_result_t maze_env_step(maze_env_t *env,
                                     maze_env_action_t action,
                                     float *reward);

/**
 * @brief 查询活动地图指定坐标的单元格类型。
 * @return 对应单元格；地图或坐标无效时返回 MAZE_ENV_CELL_INVALID。
 */
maze_env_cell_t maze_env_cell_at(rt_uint8_t map_id,
                                 rt_uint8_t x,
                                 rt_uint8_t y);

/**
 * @brief 校验 10x10 地图的单元值、唯一起终点和可达性。
 * @param[in] map 待校验地图。
 * @param[out] shortest_path 可选的最短路径步数输出，允许为 RT_NULL。
 * @return 地图校验结果。
 */
maze_env_map_validation_t maze_env_validate_map(
    const rt_uint8_t map[MAZE_ENV_SIZE][MAZE_ENV_SIZE],
    rt_uint16_t *shortest_path);

/**
 * @brief 校验并原子切换当前活动地图。
 * @param[in] map 新地图数据。
 * @param[out] shortest_path 可选的最短路径步数输出，允许为 RT_NULL。
 * @return 地图校验结果；仅 MAZE_ENV_MAP_VALID 表示切换成功。
 */
maze_env_map_validation_t maze_env_set_map(
    const rt_uint8_t map[MAZE_ENV_SIZE][MAZE_ENV_SIZE],
    rt_uint16_t *shortest_path);

/**
 * @brief 复制当前活动地图。
 * @param[out] map 接收完整 10x10 地图的缓冲区。
 * @return RT_EOK 表示成功，参数或活动地图无效时返回错误。
 */
rt_err_t maze_env_copy_map(
    rt_uint8_t map[MAZE_ENV_SIZE][MAZE_ENV_SIZE]);

/**
 * @brief 计算每个可通行状态到终点的最短距离。
 * @param[in] map_id 地图编号，当前仅支持 0。
 * @param[out] distances 长度为 MAZE_ENV_STATE_COUNT 的距离表。
 * @return RT_EOK 表示成功，参数或地图无效时返回错误。
 */
rt_err_t maze_env_build_goal_distances(
    rt_uint8_t map_id,
    rt_uint8_t distances[MAZE_ENV_STATE_COUNT]);

/**
 * @brief 获取活动地图修订号。
 * @return 初始默认地图为 1，每次成功切换地图后递增。
 */
rt_uint32_t maze_env_map_revision(void);

/**
 * @brief 将二维迷宫坐标转换为一维状态索引。
 * @return 有效状态索引；坐标越界时返回 MAZE_ENV_INVALID_STATE。
 */
rt_uint16_t maze_env_state_index(rt_uint8_t x, rt_uint8_t y);

/**
 * @brief 判断环境中的小球是否已经到达终点。
 * @param[in] env 当前迷宫环境，不得为 RT_NULL。
 * @return 到达终点返回 RT_TRUE，否则返回 RT_FALSE。
 */
rt_bool_t maze_env_is_done(const maze_env_t *env);

#endif /* __MODULE_MAZE_ENV_H__ */
