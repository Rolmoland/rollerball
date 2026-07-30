#include <rtthread.h>
#include "app_algorithm_manager.h"

static struct rt_mutex s_algorithm_lock;
static app_algorithm_t s_active_algorithm;

const char *app_algorithm_name(app_algorithm_t algorithm)
{
    switch (algorithm)
    {
    case APP_ALGORITHM_NONE:
        return "NONE";
    case APP_ALGORITHM_RANDOM:
        return "RANDOM";
    case APP_ALGORITHM_Q:
        return "Q";
    case APP_ALGORITHM_DQN:
        return "DQN";
    default:
        return "UNKNOWN";
    }
}

rt_err_t app_algorithm_acquire(app_algorithm_t algorithm)
{
    rt_err_t result = RT_EOK;

    if (algorithm <= APP_ALGORITHM_NONE || algorithm >= APP_ALGORITHM_COUNT)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_algorithm_lock, RT_WAITING_FOREVER);
    if (s_active_algorithm != APP_ALGORITHM_NONE)
    {
        result = -RT_EBUSY;
    }
    else
    {
        s_active_algorithm = algorithm;
    }
    rt_mutex_release(&s_algorithm_lock);
    return result;
}

rt_err_t app_algorithm_release(app_algorithm_t algorithm)
{
    rt_err_t result = RT_EOK;

    if (algorithm <= APP_ALGORITHM_NONE || algorithm >= APP_ALGORITHM_COUNT)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_algorithm_lock, RT_WAITING_FOREVER);
    if (s_active_algorithm != algorithm)
    {
        result = -RT_ERROR;
    }
    else
    {
        s_active_algorithm = APP_ALGORITHM_NONE;
    }
    rt_mutex_release(&s_algorithm_lock);
    return result;
}

rt_bool_t app_algorithm_is_busy(void)
{
    return app_algorithm_active() != APP_ALGORITHM_NONE;
}

app_algorithm_t app_algorithm_active(void)
{
    app_algorithm_t algorithm;

    rt_mutex_take(&s_algorithm_lock, RT_WAITING_FOREVER);
    algorithm = s_active_algorithm;
    rt_mutex_release(&s_algorithm_lock);
    return algorithm;
}

static int app_algorithm_manager_init(void)
{
    rt_mutex_init(&s_algorithm_lock, "algolock", RT_IPC_FLAG_PRIO);
    s_active_algorithm = APP_ALGORITHM_NONE;
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(app_algorithm_manager_init);
