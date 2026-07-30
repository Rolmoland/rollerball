#ifndef APP_ALGORITHM_MANAGER_H
#define APP_ALGORITHM_MANAGER_H

#include <rtthread.h>

typedef enum
{
    APP_ALGORITHM_NONE = 0,
    APP_ALGORITHM_RANDOM,
    APP_ALGORITHM_Q,
    APP_ALGORITHM_DQN,
    APP_ALGORITHM_COUNT,
} app_algorithm_t;

/**
 * @brief 独占取得算法执行权，防止多个训练/基线任务同时运行。
 * @param[in] algorithm 要启动的算法类型，不能为 APP_ALGORITHM_NONE。
 * @return RT_EOK 表示取得成功，-RT_EBUSY 表示已有算法运行。
 */
rt_err_t app_algorithm_acquire(app_algorithm_t algorithm);

/**
 * @brief 释放当前算法执行权。
 * @param[in] algorithm 必须与当前持有执行权的算法一致。
 * @return RT_EOK 表示释放成功，参数或持有者不匹配时返回错误。
 */
rt_err_t app_algorithm_release(app_algorithm_t algorithm);

/**
 * @brief 查询是否存在正在运行的算法任务。
 * @return 忙时返回 RT_TRUE，否则返回 RT_FALSE。
 */
rt_bool_t app_algorithm_is_busy(void);

#endif /* APP_ALGORITHM_MANAGER_H */
