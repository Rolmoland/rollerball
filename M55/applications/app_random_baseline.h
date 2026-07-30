#ifndef __APP_RANDOM_BASELINE_H__
#define __APP_RANDOM_BASELINE_H__

#include <rtthread.h>

typedef enum
{
    RANDOM_BASELINE_UI_READY = 0,
    RANDOM_BASELINE_UI_RUNNING,
    RANDOM_BASELINE_UI_COMPLETE,
} random_baseline_ui_phase_t;

typedef struct
{
    random_baseline_ui_phase_t phase;
    rt_uint32_t current_episode;
    rt_uint32_t target_episodes;
    rt_uint32_t successful_episodes;
    rt_uint16_t average_success_steps;
    rt_uint16_t best_steps;
    rt_uint32_t revision;
} random_baseline_ui_state_t;

/**
 * @brief 获取随机策略基线状态的线程安全快照。
 * @param[out] state 接收界面状态，不得为 RT_NULL。
 * @return RT_EOK 表示成功，参数无效时返回 -RT_EINVAL。
 */
rt_err_t random_baseline_get_ui_state(random_baseline_ui_state_t *state);

/**
 * @brief 清空随机策略统计并恢复就绪状态。
 * @return RT_EOK 表示成功，算法任务正在运行时返回 -RT_EBUSY。
 */
rt_err_t random_baseline_reset(void);

#endif /* __APP_RANDOM_BASELINE_H__ */
