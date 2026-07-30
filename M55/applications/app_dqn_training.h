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

/**
 * @brief 获取 DQN 训练或推理状态的线程安全快照。
 * @param[out] state 接收界面状态，不得为 RT_NULL。
 * @return RT_EOK 表示成功，参数无效时返回 -RT_EINVAL。
 */
rt_err_t dqn_training_get_ui_state(dqn_training_ui_state_t *state);

/**
 * @brief 清空 DQN 网络、经验回放和训练统计，并恢复训练就绪状态。
 * @return RT_EOK 表示成功，算法任务正在运行时返回 -RT_EBUSY。
 */
rt_err_t dqn_training_reset(void);

#endif /* APP_DQN_TRAINING_H */
