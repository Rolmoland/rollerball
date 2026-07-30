#ifndef __APP_MODE_MANAGER_H__
#define __APP_MODE_MANAGER_H__

#include <rtthread.h>

typedef enum
{
    APP_MODE_DEMO = 0,
    APP_MODE_TRAIN,
    APP_MODE_INFER,
    APP_MODE_COMPARE,
    APP_MODE_COUNT,
} app_mode_t;

typedef struct
{
    app_mode_t mode;
    rt_uint32_t revision;
} app_mode_state_t;

rt_err_t app_mode_set(app_mode_t mode);
rt_err_t app_mode_get_state(app_mode_state_t *state);
const char *app_mode_name(app_mode_t mode);

#endif /* __APP_MODE_MANAGER_H__ */
