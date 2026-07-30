#include <string.h>
#include "module_dqn_agent.h"

#define DQN_DEFAULT_LEARNING_RATE       0.005f
#define DQN_DEFAULT_DISCOUNT_FACTOR     0.95f
#define DQN_DEFAULT_EPSILON             1.0f
#define DQN_DEFAULT_EPSILON_MIN         0.10f
#define DQN_DEFAULT_EPSILON_DECAY       0.998f
#define DQN_DEFAULT_RANDOM_SEED         0x44514E31U
#define DQN_INPUT_WEIGHT_SCALE          0.10f
#define DQN_HIDDEN_WEIGHT_SCALE         0.10f
#define DQN_OUTPUT_WEIGHT_SCALE         0.10f
#define DQN_TARGET_SYNC_INTERVAL        100U

static rt_uint32_t dqn_random(dqn_agent_t *agent)
{
    agent->random_state = agent->random_state * 1664525U + 1013904223U;
    return agent->random_state;
}

static float dqn_random_unit(dqn_agent_t *agent)
{
    return (float)(dqn_random(agent) >> 8) * (1.0f / 16777216.0f);
}

static float dqn_random_signed(dqn_agent_t *agent)
{
    return dqn_random_unit(agent) * 2.0f - 1.0f;
}

static void network_initialize(dqn_agent_t *agent, dqn_network_t *network)
{
    rt_uint16_t state;
    rt_uint8_t hidden1;
    rt_uint8_t hidden2;
    rt_uint8_t action;

    memset(network, 0, sizeof(*network));
    for (state = 0U; state < MAZE_ENV_STATE_COUNT; state++)
    {
        for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
        {
            network->input_weights[state][hidden1] =
                dqn_random_signed(agent) * DQN_INPUT_WEIGHT_SCALE;
        }
    }
    for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
    {
        for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
        {
            network->hidden_weights[hidden1][hidden2] =
                dqn_random_signed(agent) * DQN_HIDDEN_WEIGHT_SCALE;
        }
    }
    for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
    {
        for (action = 0U; action < MAZE_ENV_ACTION_COUNT; action++)
        {
            network->output_weights[hidden2][action] =
                dqn_random_signed(agent) * DQN_OUTPUT_WEIGHT_SCALE;
        }
    }
}

static void network_forward(const dqn_network_t *network,
                            rt_uint16_t state,
                            float hidden1_values[DQN_AGENT_HIDDEN1_SIZE],
                            float hidden2_values[DQN_AGENT_HIDDEN2_SIZE],
                            float q_values[MAZE_ENV_ACTION_COUNT])
{
    rt_uint8_t hidden1;
    rt_uint8_t hidden2;
    rt_uint8_t action;

    for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
    {
        float value = network->input_weights[state][hidden1] +
                      network->input_bias[hidden1];

        hidden1_values[hidden1] = value > 0.0f ? value : 0.0f;
    }

    for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
    {
        float value = network->hidden_bias[hidden2];

        for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
        {
            value += hidden1_values[hidden1] *
                     network->hidden_weights[hidden1][hidden2];
        }
        hidden2_values[hidden2] = value > 0.0f ? value : 0.0f;
    }

    for (action = 0U; action < MAZE_ENV_ACTION_COUNT; action++)
    {
        float value = network->output_bias[action];

        for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
        {
            value += hidden2_values[hidden2] *
                     network->output_weights[hidden2][action];
        }
        q_values[action] = value;
    }
}

static float network_max_q(const dqn_network_t *network, rt_uint16_t state)
{
    float hidden1_values[DQN_AGENT_HIDDEN1_SIZE];
    float hidden2_values[DQN_AGENT_HIDDEN2_SIZE];
    float q_values[MAZE_ENV_ACTION_COUNT];
    float max_value;
    rt_uint8_t action;

    network_forward(network, state, hidden1_values, hidden2_values, q_values);
    max_value = q_values[0];
    for (action = 1U; action < MAZE_ENV_ACTION_COUNT; action++)
    {
        if (q_values[action] > max_value)
        {
            max_value = q_values[action];
        }
    }
    return max_value;
}

static float network_train_transition(dqn_network_t *network,
                                      const dqn_transition_t *transition,
                                      float target_value,
                                      float learning_rate)
{
    float hidden1_values[DQN_AGENT_HIDDEN1_SIZE];
    float hidden2_values[DQN_AGENT_HIDDEN2_SIZE];
    float hidden1_gradients[DQN_AGENT_HIDDEN1_SIZE];
    float hidden2_gradients[DQN_AGENT_HIDDEN2_SIZE];
    float q_values[MAZE_ENV_ACTION_COUNT];
    float error;
    float output_gradient;
    float absolute_error;
    float loss;
    rt_uint8_t hidden1;
    rt_uint8_t hidden2;
    rt_uint8_t action = transition->action;

    network_forward(network, transition->state,
                    hidden1_values, hidden2_values, q_values);
    error = q_values[action] - target_value;
    absolute_error = error < 0.0f ? -error : error;
    if (absolute_error <= 1.0f)
    {
        loss = 0.5f * error * error;
        output_gradient = error;
    }
    else
    {
        loss = absolute_error - 0.5f;
        output_gradient = error < 0.0f ? -1.0f : 1.0f;
    }

    for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
    {
        hidden2_gradients[hidden2] = hidden2_values[hidden2] > 0.0f ?
            network->output_weights[hidden2][action] * output_gradient :
            0.0f;
    }
    for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
    {
        float gradient = 0.0f;

        for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
        {
            gradient += network->hidden_weights[hidden1][hidden2] *
                        hidden2_gradients[hidden2];
        }
        hidden1_gradients[hidden1] = hidden1_values[hidden1] > 0.0f ?
                                     gradient : 0.0f;
    }

    for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
    {
        network->output_weights[hidden2][action] -=
            learning_rate * output_gradient * hidden2_values[hidden2];
    }
    network->output_bias[action] -= learning_rate * output_gradient;

    for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
    {
        for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
        {
            network->hidden_weights[hidden1][hidden2] -=
                learning_rate * hidden2_gradients[hidden2] *
                hidden1_values[hidden1];
        }
    }
    for (hidden2 = 0U; hidden2 < DQN_AGENT_HIDDEN2_SIZE; hidden2++)
    {
        network->hidden_bias[hidden2] -=
            learning_rate * hidden2_gradients[hidden2];
    }

    for (hidden1 = 0U; hidden1 < DQN_AGENT_HIDDEN1_SIZE; hidden1++)
    {
        network->input_weights[transition->state][hidden1] -=
            learning_rate * hidden1_gradients[hidden1];
        network->input_bias[hidden1] -=
            learning_rate * hidden1_gradients[hidden1];
    }
    return loss;
}

void dqn_agent_init(dqn_agent_t *agent, rt_uint32_t random_seed)
{
    RT_ASSERT(agent != RT_NULL);

    memset(agent, 0, sizeof(*agent));
    agent->random_state = random_seed == 0U ?
                          DQN_DEFAULT_RANDOM_SEED : random_seed;
    agent->learning_rate = DQN_DEFAULT_LEARNING_RATE;
    agent->discount_factor = DQN_DEFAULT_DISCOUNT_FACTOR;
    agent->epsilon = DQN_DEFAULT_EPSILON;
    agent->epsilon_min = DQN_DEFAULT_EPSILON_MIN;
    agent->epsilon_decay = DQN_DEFAULT_EPSILON_DECAY;
    network_initialize(agent, &agent->online_network);
    memcpy(&agent->target_network, &agent->online_network,
           sizeof(agent->target_network));
}

void dqn_agent_reset(dqn_agent_t *agent)
{
    rt_uint32_t random_seed;

    RT_ASSERT(agent != RT_NULL);

    random_seed = agent->random_state ^ 0x9E3779B9U;
    dqn_agent_init(agent, random_seed);
}

maze_env_action_t dqn_agent_select_action(dqn_agent_t *agent,
                                          rt_uint16_t state,
                                          rt_bool_t explore)
{
    float hidden1_values[DQN_AGENT_HIDDEN1_SIZE];
    float hidden2_values[DQN_AGENT_HIDDEN2_SIZE];
    float q_values[MAZE_ENV_ACTION_COUNT];
    rt_uint8_t start_action;
    rt_uint8_t best_action;
    rt_uint8_t offset;

    RT_ASSERT(agent != RT_NULL);

    if (state >= MAZE_ENV_STATE_COUNT)
    {
        return MAZE_ENV_ACTION_NONE;
    }
    if (explore && dqn_random_unit(agent) < agent->epsilon)
    {
        return (maze_env_action_t)(dqn_random(agent) %
                                   MAZE_ENV_ACTION_COUNT);
    }

    network_forward(&agent->online_network, state,
                    hidden1_values, hidden2_values, q_values);
    start_action = (rt_uint8_t)(dqn_random(agent) % MAZE_ENV_ACTION_COUNT);
    best_action = start_action;
    for (offset = 1U; offset < MAZE_ENV_ACTION_COUNT; offset++)
    {
        rt_uint8_t action = (rt_uint8_t)((start_action + offset) %
                                         MAZE_ENV_ACTION_COUNT);

        if (q_values[action] > q_values[best_action])
        {
            best_action = action;
        }
    }
    return (maze_env_action_t)best_action;
}

rt_err_t dqn_agent_remember(dqn_agent_t *agent,
                            rt_uint16_t state,
                            maze_env_action_t action,
                            float reward,
                            rt_uint16_t next_state,
                            rt_bool_t done)
{
    dqn_transition_t *transition;

    if (agent == RT_NULL || state >= MAZE_ENV_STATE_COUNT ||
        next_state >= MAZE_ENV_STATE_COUNT ||
        (rt_uint32_t)action >= MAZE_ENV_ACTION_COUNT)
    {
        return -RT_EINVAL;
    }

    transition = &agent->replay[agent->replay_write_index];
    transition->state = (rt_uint8_t)state;
    transition->action = (rt_uint8_t)action;
    transition->next_state = (rt_uint8_t)next_state;
    transition->done = done ? 1U : 0U;
    transition->reward = reward;

    agent->replay_write_index++;
    if (agent->replay_write_index >= DQN_AGENT_REPLAY_CAPACITY)
    {
        agent->replay_write_index = 0U;
    }
    if (agent->replay_count < DQN_AGENT_REPLAY_CAPACITY)
    {
        agent->replay_count++;
    }
    return RT_EOK;
}

rt_bool_t dqn_agent_train_batch(dqn_agent_t *agent, float *average_loss)
{
    float total_loss = 0.0f;
    rt_uint8_t sample;

    RT_ASSERT(agent != RT_NULL);

    if (average_loss != RT_NULL)
    {
        *average_loss = 0.0f;
    }
    if (agent->replay_count < DQN_AGENT_BATCH_SIZE)
    {
        return RT_FALSE;
    }

    for (sample = 0U; sample < DQN_AGENT_BATCH_SIZE; sample++)
    {
        const dqn_transition_t *transition =
            &agent->replay[dqn_random(agent) % agent->replay_count];
        float target_value = transition->reward;

        if (!transition->done)
        {
            target_value += agent->discount_factor *
                network_max_q(&agent->target_network,
                              transition->next_state);
        }
        total_loss += network_train_transition(&agent->online_network,
                                               transition,
                                               target_value,
                                               agent->learning_rate);
    }

    agent->train_updates++;
    if (agent->train_updates % DQN_TARGET_SYNC_INTERVAL == 0U)
    {
        memcpy(&agent->target_network, &agent->online_network,
               sizeof(agent->target_network));
        agent->target_syncs++;
    }
    if (average_loss != RT_NULL)
    {
        *average_loss = total_loss / (float)DQN_AGENT_BATCH_SIZE;
    }
    return RT_TRUE;
}

void dqn_agent_decay_epsilon(dqn_agent_t *agent)
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
