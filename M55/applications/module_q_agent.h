#ifndef __MODULE_Q_AGENT_H__
#define __MODULE_Q_AGENT_H__

#include <rtthread.h>
#include "module_maze_env.h"

typedef struct
{
    float q_values[MAZE_ENV_STATE_COUNT][MAZE_ENV_ACTION_COUNT];
    float learning_rate;
    float discount_factor;
    float epsilon;
    float epsilon_min;
    float epsilon_decay;
    rt_uint32_t random_state;
} q_agent_t;

/**
 * @brief 初始化 Q 表和 Q-learning 超参数。
 * @param[out] agent 待初始化的智能体，不得为 RT_NULL。
 * @param[in] random_seed 随机种子；传入 0 时使用模块默认种子。
 */
void q_agent_init(q_agent_t *agent, rt_uint32_t random_seed);

/**
 * @brief 清空 Q 表并保留当前随机数状态作为下一次初始化种子。
 * @param[in,out] agent 待复位的智能体，不得为 RT_NULL。
 */
void q_agent_reset(q_agent_t *agent);

/**
 * @brief 用人工演示偏好预置指定状态动作的 Q 值。
 * @param[in,out] agent Q-learning 智能体。
 * @param[in] state 当前状态索引。
 * @param[in] action 人工演示动作。
 * @param[in] preference 要注入的偏好值，仅在绝对方向更强时覆盖。
 */
void q_agent_seed_action(q_agent_t *agent,
                         rt_uint16_t state,
                         maze_env_action_t action,
                         float preference);

/**
 * @brief 使用 epsilon-greedy 策略选择动作。
 * @param[in,out] agent Q-learning 智能体，调用过程会推进随机数状态。
 * @param[in] state 当前离散迷宫状态索引。
 * @param[in] explore RT_TRUE 启用探索，RT_FALSE 仅选择当前最优动作。
 * @return 选中的动作；状态无效时返回 MAZE_ENV_ACTION_NONE。
 */
maze_env_action_t q_agent_select_action(q_agent_t *agent,
                                        rt_uint16_t state,
                                        rt_bool_t explore);

/**
 * @brief 根据一条状态转移执行一次 Q-learning 更新。
 * @param[in,out] agent Q-learning 智能体。
 * @param[in] state 更新前状态索引。
 * @param[in] action 已执行动作。
 * @param[in] reward 本次动作奖励。
 * @param[in] next_state 更新后状态索引。
 * @param[in] done 是否已经到达终点。
 */
void q_agent_update(q_agent_t *agent,
                    rt_uint16_t state,
                    maze_env_action_t action,
                    float reward,
                    rt_uint16_t next_state,
                    rt_bool_t done);

/**
 * @brief 按衰减系数降低探索率，但不低于 epsilon_min。
 * @param[in,out] agent Q-learning 智能体，不得为 RT_NULL。
 */
void q_agent_decay_epsilon(q_agent_t *agent);

#endif /* __MODULE_Q_AGENT_H__ */
