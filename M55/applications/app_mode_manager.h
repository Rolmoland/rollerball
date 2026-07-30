#ifndef __APP_MODE_MANAGER_H__
#define __APP_MODE_MANAGER_H__

#include <rtthread.h>

typedef enum
{
    APP_MODE_DEMO = 0,
    APP_MODE_RANDOM,
    APP_MODE_TRAIN,
    APP_MODE_INFER,
    APP_MODE_COMPARE,
    APP_MODE_DQN_TRAIN,
    APP_MODE_DQN_INFER,
    APP_MODE_COUNT,
} app_mode_t;

typedef struct
{
    app_mode_t mode;
    rt_uint32_t revision;
} app_mode_state_t;

/**
 * @brief 切换迷宫界面的当前工作模式。
 * @param[in] mode 目标模式，必须小于 APP_MODE_COUNT。
 * @return RT_EOK 表示成功，模式无效时返回 -RT_EINVAL。
 */
rt_err_t app_mode_set(app_mode_t mode);

/**
 * @brief 获取当前界面模式和模式修订号的线程安全快照。
 * @param[out] state 接收模式状态，不得为 RT_NULL。
 * @return RT_EOK 表示成功，参数无效时返回 -RT_EINVAL。
 */
rt_err_t app_mode_get_state(app_mode_state_t *state);

#endif /* __APP_MODE_MANAGER_H__ */
