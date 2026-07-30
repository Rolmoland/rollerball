#include <rtthread.h>
#include <string.h>
#include "app_mode_manager.h"

static struct rt_mutex s_mode_lock;
static app_mode_state_t s_mode_state;

const char *app_mode_name(app_mode_t mode)
{
    switch (mode)
    {
    case APP_MODE_DEMO:
        return "DEMO";
    case APP_MODE_RANDOM:
        return "RANDOM";
    case APP_MODE_TRAIN:
        return "TRAIN";
    case APP_MODE_INFER:
        return "INFER";
    case APP_MODE_COMPARE:
        return "COMPARE";
    case APP_MODE_DQN_TRAIN:
        return "DQN_TRAIN";
    case APP_MODE_DQN_INFER:
        return "DQN_INFER";
    default:
        return "UNKNOWN";
    }
}

rt_err_t app_mode_set(app_mode_t mode)
{
    if ((rt_uint32_t)mode >= APP_MODE_COUNT)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_mode_lock, RT_WAITING_FOREVER);
    if (s_mode_state.mode != mode)
    {
        s_mode_state.mode = mode;
        s_mode_state.revision++;
    }
    rt_mutex_release(&s_mode_lock);
    return RT_EOK;
}

rt_err_t app_mode_get_state(app_mode_state_t *state)
{
    if (state == RT_NULL)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_mode_lock, RT_WAITING_FOREVER);
    *state = s_mode_state;
    rt_mutex_release(&s_mode_lock);
    return RT_EOK;
}

static int maze_mode_cmd(int argc, char **argv)
{
    app_mode_t mode;

    if (argc == 1)
    {
        app_mode_state_t state;

        app_mode_get_state(&state);
        rt_kprintf("[MODE] %s\n", app_mode_name(state.mode));
        return RT_EOK;
    }
    if (argc != 2)
    {
        rt_kprintf("Usage: maze_mode [demo|random|train|infer|compare|dqn_train|dqn_infer]\n");
        return -RT_EINVAL;
    }

    if (strcmp(argv[1], "demo") == 0)
    {
        mode = APP_MODE_DEMO;
    }
    else if (strcmp(argv[1], "random") == 0)
    {
        mode = APP_MODE_RANDOM;
    }
    else if (strcmp(argv[1], "train") == 0)
    {
        mode = APP_MODE_TRAIN;
    }
    else if (strcmp(argv[1], "infer") == 0)
    {
        mode = APP_MODE_INFER;
    }
    else if (strcmp(argv[1], "compare") == 0)
    {
        mode = APP_MODE_COMPARE;
    }
    else if (strcmp(argv[1], "dqn_train") == 0)
    {
        mode = APP_MODE_DQN_TRAIN;
    }
    else if (strcmp(argv[1], "dqn_infer") == 0)
    {
        mode = APP_MODE_DQN_INFER;
    }
    else
    {
        rt_kprintf("Usage: maze_mode [demo|random|train|infer|compare|dqn_train|dqn_infer]\n");
        return -RT_EINVAL;
    }

    app_mode_set(mode);
    rt_kprintf("[MODE] %s\n", app_mode_name(mode));
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(maze_mode_cmd, maze_mode, Switch maze application mode);

static int app_mode_manager_init(void)
{
    rt_mutex_init(&s_mode_lock, "mode_lock", RT_IPC_FLAG_PRIO);
    s_mode_state.mode = APP_MODE_DEMO;
    s_mode_state.revision = 1U;
    return RT_EOK;
}
INIT_APP_EXPORT(app_mode_manager_init);
