#ifndef __APP_Q_TRAINING_H__
#define __APP_Q_TRAINING_H__

#include <rtthread.h>

typedef enum
{
    Q_TRAINING_UI_NONE = 0,
    Q_TRAINING_UI_READY,
    Q_TRAINING_UI_PRETRAIN,
    Q_TRAINING_UI_TRAINING,
    Q_TRAINING_UI_TRAINED,
    Q_TRAINING_UI_INFERENCE,
    Q_TRAINING_UI_INFERENCE_DONE,
} q_training_ui_phase_t;

typedef struct
{
    q_training_ui_phase_t phase;
    rt_uint8_t agent_x;
    rt_uint8_t agent_y;
    rt_bool_t inference_complete;
    rt_bool_t inference_success;
    rt_uint16_t training_steps;
    rt_uint16_t inference_steps;
    rt_uint32_t current_episode;
    rt_uint32_t target_episodes;
    rt_uint32_t successful_episodes;
    rt_uint32_t demonstration_seeds;
    rt_uint32_t epsilon_milli;
    rt_int32_t training_reward_tenths;
    rt_int32_t inference_reward_tenths;
    rt_uint32_t revision;
} q_training_ui_state_t;

rt_err_t q_training_get_ui_state(q_training_ui_state_t *state);

#endif /* __APP_Q_TRAINING_H__ */
