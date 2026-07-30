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

/**
 * @brief 获取 Q-learning 训练或推理状态的线程安全快照。
 * @param[out] state 接收界面状态，不得为 RT_NULL。
 * @return RT_EOK 表示成功，参数无效时返回 -RT_EINVAL。
 */
rt_err_t q_training_get_ui_state(q_training_ui_state_t *state);

/**
 * @brief 清空 Q 表、训练统计和推理状态，并恢复训练就绪状态。
 * @return RT_EOK 表示成功，算法任务正在运行时返回 -RT_EBUSY。
 */
rt_err_t q_training_reset(void);

#endif /* __APP_Q_TRAINING_H__ */
