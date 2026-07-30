#ifndef MODULE_DQN_AGENT_H
#define MODULE_DQN_AGENT_H

#include <rtthread.h>
#include "module_maze_env.h"

#define DQN_AGENT_HIDDEN1_SIZE       32U
#define DQN_AGENT_HIDDEN2_SIZE       16U
#define DQN_AGENT_REPLAY_CAPACITY   512U
#define DQN_AGENT_BATCH_SIZE         16U

typedef struct
{
    float input_weights[MAZE_ENV_STATE_COUNT][DQN_AGENT_HIDDEN1_SIZE];
    float input_bias[DQN_AGENT_HIDDEN1_SIZE];
    float hidden_weights[DQN_AGENT_HIDDEN1_SIZE][DQN_AGENT_HIDDEN2_SIZE];
    float hidden_bias[DQN_AGENT_HIDDEN2_SIZE];
    float output_weights[DQN_AGENT_HIDDEN2_SIZE][MAZE_ENV_ACTION_COUNT];
    float output_bias[MAZE_ENV_ACTION_COUNT];
} dqn_network_t;

typedef struct
{
    rt_uint8_t state;
    rt_uint8_t action;
    rt_uint8_t next_state;
    rt_uint8_t done;
    float reward;
} dqn_transition_t;

typedef struct
{
    dqn_network_t online_network;
    dqn_network_t target_network;
    dqn_transition_t replay[DQN_AGENT_REPLAY_CAPACITY];
    rt_uint16_t replay_count;
    rt_uint16_t replay_write_index;
    rt_uint32_t train_updates;
    rt_uint32_t target_syncs;
    rt_uint32_t random_state;
    float learning_rate;
    float discount_factor;
    float epsilon;
    float epsilon_min;
    float epsilon_decay;
} dqn_agent_t;

/**
 * @brief 初始化 DQN 在线网络、目标网络、经验回放和超参数。
 * @param[out] agent 待初始化的智能体，不得为 RT_NULL。
 * @param[in] random_seed 随机种子；传入 0 时使用模块默认种子。
 */
void dqn_agent_init(dqn_agent_t *agent, rt_uint32_t random_seed);

/**
 * @brief 重新初始化网络和训练状态，并派生一个新的随机种子。
 * @param[in,out] agent 待复位的智能体，不得为 RT_NULL。
 */
void dqn_agent_reset(dqn_agent_t *agent);

/**
 * @brief 使用 epsilon-greedy 策略选择动作。
 * @param[in,out] agent DQN 智能体，调用过程会推进随机数状态。
 * @param[in] state 当前离散迷宫状态索引。
 * @param[in] explore RT_TRUE 启用探索，RT_FALSE 仅选择当前最优动作。
 * @return 选中的动作；状态无效时返回 MAZE_ENV_ACTION_NONE。
 */
maze_env_action_t dqn_agent_select_action(dqn_agent_t *agent,
                                          rt_uint16_t state,
                                          rt_bool_t explore);

/**
 * @brief 将一条状态转移写入循环经验回放缓冲区。
 * @return RT_EOK 表示成功，参数或状态索引无效时返回 -RT_EINVAL。
 */
rt_err_t dqn_agent_remember(dqn_agent_t *agent,
                            rt_uint16_t state,
                            maze_env_action_t action,
                            float reward,
                            rt_uint16_t next_state,
                            rt_bool_t done);

/**
 * @brief 从经验回放中随机采样一个批次并更新在线网络。
 * @param[in,out] agent DQN 智能体，不得为 RT_NULL。
 * @param[out] average_loss 可选的批次平均损失输出，允许为 RT_NULL。
 * @return 完成一次更新返回 RT_TRUE，样本不足返回 RT_FALSE。
 */
rt_bool_t dqn_agent_train_batch(dqn_agent_t *agent, float *average_loss);

/**
 * @brief 按衰减系数降低探索率，但不低于 epsilon_min。
 * @param[in,out] agent DQN 智能体，不得为 RT_NULL。
 */
void dqn_agent_decay_epsilon(dqn_agent_t *agent);

#endif /* MODULE_DQN_AGENT_H */
