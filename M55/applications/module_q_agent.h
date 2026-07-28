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

void q_agent_init(q_agent_t *agent, rt_uint32_t random_seed);
void q_agent_reset(q_agent_t *agent);
void q_agent_seed_action(q_agent_t *agent,
                         rt_uint16_t state,
                         maze_env_action_t action,
                         float preference);
maze_env_action_t q_agent_select_action(q_agent_t *agent,
                                        rt_uint16_t state,
                                        rt_bool_t explore);
void q_agent_update(q_agent_t *agent,
                    rt_uint16_t state,
                    maze_env_action_t action,
                    float reward,
                    rt_uint16_t next_state,
                    rt_bool_t done);
void q_agent_decay_epsilon(q_agent_t *agent);
float q_agent_max_value(const q_agent_t *agent, rt_uint16_t state);

#endif /* __MODULE_Q_AGENT_H__ */
