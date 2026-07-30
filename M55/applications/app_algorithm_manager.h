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

rt_err_t app_algorithm_acquire(app_algorithm_t algorithm);
rt_err_t app_algorithm_release(app_algorithm_t algorithm);
rt_bool_t app_algorithm_is_busy(void);
app_algorithm_t app_algorithm_active(void);
const char *app_algorithm_name(app_algorithm_t algorithm);

#endif /* APP_ALGORITHM_MANAGER_H */
