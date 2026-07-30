#ifndef APP_DQN_TRAINING_H
#define APP_DQN_TRAINING_H

#include <rtthread.h>

typedef enum
{
    DQN_TRAINING_UI_NONE = 0,
    DQN_TRAINING_UI_READY,
    DQN_TRAINING_UI_TRAINING,
    DQN_TRAINING_UI_TRAINED,
    DQN_TRAINING_UI_INFERENCE,
    DQN_TRAINING_UI_INFERENCE_DONE,
} dqn_training_ui_phase_t;

typedef struct
{
    dqn_training_ui_phase_t phase;
    rt_uint8_t agent_x;
    rt_uint8_t agent_y;
    rt_bool_t inference_complete;
    rt_bool_t inference_success;
    rt_uint16_t training_steps;
    rt_uint16_t inference_steps;
    rt_uint16_t replay_count;
    rt_uint32_t current_episode;
    rt_uint32_t target_episodes;
    rt_uint32_t successful_episodes;
    rt_uint32_t train_updates;
    rt_uint32_t target_syncs;
    rt_uint32_t epsilon_milli;
    rt_uint32_t loss_milli;
    rt_int32_t training_reward_tenths;
    rt_int32_t inference_reward_tenths;
    rt_uint32_t revision;
} dqn_training_ui_state_t;

rt_err_t dqn_training_get_ui_state(dqn_training_ui_state_t *state);
rt_err_t dqn_training_reset(void);
rt_bool_t dqn_training_is_busy(void);

#endif /* APP_DQN_TRAINING_H */
