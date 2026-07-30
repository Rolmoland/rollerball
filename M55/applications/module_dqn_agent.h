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

void dqn_agent_init(dqn_agent_t *agent, rt_uint32_t random_seed);
void dqn_agent_reset(dqn_agent_t *agent);
maze_env_action_t dqn_agent_select_action(dqn_agent_t *agent,
                                          rt_uint16_t state,
                                          rt_bool_t explore);
rt_err_t dqn_agent_remember(dqn_agent_t *agent,
                            rt_uint16_t state,
                            maze_env_action_t action,
                            float reward,
                            rt_uint16_t next_state,
                            rt_bool_t done);
rt_bool_t dqn_agent_train_batch(dqn_agent_t *agent, float *average_loss);
void dqn_agent_decay_epsilon(dqn_agent_t *agent);

#endif /* MODULE_DQN_AGENT_H */
