#include <string.h>
#include "module_q_agent.h"

#define Q_AGENT_DEFAULT_LEARNING_RATE    0.10f
#define Q_AGENT_DEFAULT_DISCOUNT_FACTOR  0.95f
#define Q_AGENT_DEFAULT_EPSILON          0.30f
#define Q_AGENT_DEFAULT_EPSILON_MIN      0.01f
#define Q_AGENT_DEFAULT_EPSILON_DECAY    0.995f
#define Q_AGENT_DEFAULT_RANDOM_SEED      0x4D415A45U

static rt_uint32_t q_agent_random(q_agent_t *agent)
{
    agent->random_state = agent->random_state * 1664525U + 1013904223U;
    return agent->random_state;
}

static float q_agent_random_unit(q_agent_t *agent)
{
    return (float)(q_agent_random(agent) >> 8) * (1.0f / 16777216.0f);
}

void q_agent_init(q_agent_t *agent, rt_uint32_t random_seed)
{
    RT_ASSERT(agent != RT_NULL);

    memset(agent, 0, sizeof(*agent));
    agent->learning_rate = Q_AGENT_DEFAULT_LEARNING_RATE;
    agent->discount_factor = Q_AGENT_DEFAULT_DISCOUNT_FACTOR;
    agent->epsilon = Q_AGENT_DEFAULT_EPSILON;
    agent->epsilon_min = Q_AGENT_DEFAULT_EPSILON_MIN;
    agent->epsilon_decay = Q_AGENT_DEFAULT_EPSILON_DECAY;
    agent->random_state = random_seed == 0U ?
                          Q_AGENT_DEFAULT_RANDOM_SEED : random_seed;
}

void q_agent_reset(q_agent_t *agent)
{
    rt_uint32_t random_seed;

    RT_ASSERT(agent != RT_NULL);

    random_seed = agent->random_state;
    q_agent_init(agent, random_seed);
}

void q_agent_seed_action(q_agent_t *agent,
                         rt_uint16_t state,
                         maze_env_action_t action,
                         float preference)
{
    float *value;

    RT_ASSERT(agent != RT_NULL);

    if (state >= MAZE_ENV_STATE_COUNT ||
        (rt_uint32_t)action >= MAZE_ENV_ACTION_COUNT)
    {
        return;
    }

    value = &agent->q_values[state][action];
    if ((preference > 0.0f && *value < preference) ||
        (preference < 0.0f && *value > preference))
    {
        *value = preference;
    }
}

static float q_agent_max_value(const q_agent_t *agent, rt_uint16_t state)
{
    rt_uint8_t action;
    float max_value;

    RT_ASSERT(agent != RT_NULL);

    if (state >= MAZE_ENV_STATE_COUNT)
    {
        return 0.0f;
    }

    max_value = agent->q_values[state][0];
    for (action = 1U; action < MAZE_ENV_ACTION_COUNT; action++)
    {
        if (agent->q_values[state][action] > max_value)
        {
            max_value = agent->q_values[state][action];
        }
    }
    return max_value;
}

maze_env_action_t q_agent_select_action(q_agent_t *agent,
                                        rt_uint16_t state,
                                        rt_bool_t explore)
{
    rt_uint8_t start_action;
    rt_uint8_t best_action;
    rt_uint8_t offset;
    rt_uint8_t action;
    float best_value;

    RT_ASSERT(agent != RT_NULL);

    if (state >= MAZE_ENV_STATE_COUNT)
    {
        return MAZE_ENV_ACTION_NONE;
    }
    if (explore && q_agent_random_unit(agent) < agent->epsilon)
    {
        return (maze_env_action_t)(q_agent_random(agent) %
                                   MAZE_ENV_ACTION_COUNT);
    }

    start_action = (rt_uint8_t)(q_agent_random(agent) %
                                MAZE_ENV_ACTION_COUNT);
    best_action = start_action;
    best_value = agent->q_values[state][best_action];
    for (offset = 1U; offset < MAZE_ENV_ACTION_COUNT; offset++)
    {
        action = (rt_uint8_t)((start_action + offset) %
                              MAZE_ENV_ACTION_COUNT);
        if (agent->q_values[state][action] > best_value)
        {
            best_action = action;
            best_value = agent->q_values[state][action];
        }
    }

    return (maze_env_action_t)best_action;
}

void q_agent_update(q_agent_t *agent,
                    rt_uint16_t state,
                    maze_env_action_t action,
                    float reward,
                    rt_uint16_t next_state,
                    rt_bool_t done)
{
    float current_value;
    float target_value;

    RT_ASSERT(agent != RT_NULL);

    if (state >= MAZE_ENV_STATE_COUNT ||
        next_state >= MAZE_ENV_STATE_COUNT ||
        (rt_uint32_t)action >= MAZE_ENV_ACTION_COUNT)
    {
        return;
    }

    current_value = agent->q_values[state][action];
    target_value = reward;
    if (!done)
    {
        target_value += agent->discount_factor *
                        q_agent_max_value(agent, next_state);
    }
    agent->q_values[state][action] =
        current_value + agent->learning_rate *
        (target_value - current_value);
}

void q_agent_decay_epsilon(q_agent_t *agent)
{
    RT_ASSERT(agent != RT_NULL);

    if (agent->epsilon > agent->epsilon_min)
    {
        agent->epsilon *= agent->epsilon_decay;
        if (agent->epsilon < agent->epsilon_min)
        {
            agent->epsilon = agent->epsilon_min;
        }
    }
}
