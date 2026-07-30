#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include "app_algorithm_manager.h"
#include "app_mode_manager.h"
#include "app_random_baseline.h"
#include "module_maze_env.h"

#define RANDOM_DEFAULT_EPISODES       100U
#define RANDOM_MAX_EPISODES         10000U
#define RANDOM_MAX_STEPS              200U
#define RANDOM_LOG_INTERVAL            25U
#define RANDOM_THREAD_STACK_SIZE      2048U
#define RANDOM_THREAD_PRIORITY          22U
#define RANDOM_THREAD_TIMESLICE         10U
#define RANDOM_DEFAULT_SEED      0x52414E44U

static random_baseline_ui_state_t s_ui_state;
static struct rt_mutex s_random_lock;
static rt_bool_t s_random_active;
static rt_uint32_t s_random_state;

static void ui_state_publish_locked(void)
{
    s_ui_state.revision++;
}

rt_err_t random_baseline_get_ui_state(random_baseline_ui_state_t *state)
{
    if (state == RT_NULL)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    *state = s_ui_state;
    rt_mutex_release(&s_random_lock);
    return RT_EOK;
}

rt_bool_t random_baseline_is_busy(void)
{
    rt_bool_t active;

    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    active = s_random_active;
    rt_mutex_release(&s_random_lock);
    return active;
}

rt_err_t random_baseline_reset(void)
{
    rt_uint32_t revision;

    if (app_algorithm_acquire(APP_ALGORITHM_RANDOM) != RT_EOK)
    {
        return -RT_EBUSY;
    }

    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    if (s_random_active)
    {
        rt_mutex_release(&s_random_lock);
        app_algorithm_release(APP_ALGORITHM_RANDOM);
        return -RT_EBUSY;
    }

    revision = s_ui_state.revision;
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    s_ui_state.revision = revision;
    s_ui_state.phase = RANDOM_BASELINE_UI_READY;
    s_random_state = RANDOM_DEFAULT_SEED ^ maze_env_map_revision();
    ui_state_publish_locked();
    rt_mutex_release(&s_random_lock);
    app_algorithm_release(APP_ALGORITHM_RANDOM);
    return RT_EOK;
}

static rt_bool_t baseline_try_start(void)
{
    rt_bool_t started = RT_FALSE;

    if (app_algorithm_acquire(APP_ALGORITHM_RANDOM) != RT_EOK)
    {
        return RT_FALSE;
    }

    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    if (!s_random_active)
    {
        s_random_active = RT_TRUE;
        started = RT_TRUE;
    }
    rt_mutex_release(&s_random_lock);
    if (!started)
    {
        app_algorithm_release(APP_ALGORITHM_RANDOM);
    }
    return started;
}

static void baseline_finish(void)
{
    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    s_random_active = RT_FALSE;
    rt_mutex_release(&s_random_lock);
    app_algorithm_release(APP_ALGORITHM_RANDOM);
}

static maze_env_action_t random_action(void)
{
    s_random_state = s_random_state * 1664525U + 1013904223U;
    return (maze_env_action_t)(s_random_state % MAZE_ENV_ACTION_COUNT);
}

static rt_err_t command_count(int argc, char **argv, rt_uint32_t *count)
{
    unsigned long value;
    char *end;

    if (argc == 1)
    {
        *count = RANDOM_DEFAULT_EPISODES;
        return RT_EOK;
    }
    if (argc != 2)
    {
        return -RT_EINVAL;
    }

    value = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || value == 0UL ||
        value > RANDOM_MAX_EPISODES)
    {
        return -RT_EINVAL;
    }
    *count = (rt_uint32_t)value;
    return RT_EOK;
}

static void random_baseline_thread_entry(void *parameter)
{
    rt_uint32_t episodes = (rt_uint32_t)(rt_ubase_t)parameter;
    rt_uint32_t successes = 0U;
    rt_uint32_t total_success_steps = 0U;
    rt_uint32_t completed_episodes = 0U;
    rt_uint16_t best_steps = 0U;
    rt_uint32_t episode;

    for (episode = 1U; episode <= episodes; episode++)
    {
        maze_env_t env;
        rt_uint16_t step;
        rt_uint16_t average_success_steps;
        rt_bool_t success;

        if (maze_env_init(&env, 0U) != RT_EOK)
        {
            rt_kprintf("[RANDOM] environment init failed\n");
            break;
        }

        for (step = 0U; step < RANDOM_MAX_STEPS; step++)
        {
            maze_env_step(&env, random_action(), RT_NULL);
            if (maze_env_is_done(&env))
            {
                break;
            }
        }

        success = maze_env_is_done(&env);
        completed_episodes = episode;
        if (success)
        {
            successes++;
            total_success_steps += env.total_steps;
            if (best_steps == 0U || env.total_steps < best_steps)
            {
                best_steps = env.total_steps;
            }
        }
        average_success_steps = successes == 0U ? 0U :
            (rt_uint16_t)(total_success_steps / successes);

        rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
        s_ui_state.phase = RANDOM_BASELINE_UI_RUNNING;
        s_ui_state.current_episode = episode;
        s_ui_state.successful_episodes = successes;
        s_ui_state.average_success_steps = average_success_steps;
        s_ui_state.best_steps = best_steps;
        ui_state_publish_locked();
        rt_mutex_release(&s_random_lock);

        if (episode == 1U || episode == episodes ||
            episode % RANDOM_LOG_INTERVAL == 0U)
        {
            rt_kprintf("[RANDOM] episode=%lu/%lu success=%lu "
                       "avg_success_steps=%u best_steps=%u\n",
                       (unsigned long)episode,
                       (unsigned long)episodes,
                       (unsigned long)successes,
                       (unsigned int)average_success_steps,
                       (unsigned int)best_steps);
        }
        if (episode % 10U == 0U)
        {
            rt_thread_mdelay(1U);
        }
    }

    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = RANDOM_BASELINE_UI_COMPLETE;
    ui_state_publish_locked();
    rt_mutex_release(&s_random_lock);
    baseline_finish();
    rt_kprintf("[RANDOM] complete episodes=%lu success=%lu\n",
               (unsigned long)completed_episodes,
               (unsigned long)successes);
}

static int random_run_cmd(int argc, char **argv)
{
    rt_uint32_t episodes;
    rt_thread_t thread;

    if (command_count(argc, argv, &episodes) != RT_EOK)
    {
        rt_kprintf("Usage: random_run [1-%u]\n",
                   (unsigned int)RANDOM_MAX_EPISODES);
        return -RT_EINVAL;
    }
    if (!baseline_try_start())
    {
        rt_kprintf("[RANDOM] Q training, inference, or baseline is busy\n");
        return -RT_EBUSY;
    }

    app_mode_set(APP_MODE_COMPARE);
    rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = RANDOM_BASELINE_UI_RUNNING;
    s_ui_state.current_episode = 0U;
    s_ui_state.target_episodes = episodes;
    s_ui_state.successful_episodes = 0U;
    s_ui_state.average_success_steps = 0U;
    s_ui_state.best_steps = 0U;
    ui_state_publish_locked();
    rt_mutex_release(&s_random_lock);

    thread = rt_thread_create("random",
                              random_baseline_thread_entry,
                              (void *)(rt_ubase_t)episodes,
                              RANDOM_THREAD_STACK_SIZE,
                              RANDOM_THREAD_PRIORITY,
                              RANDOM_THREAD_TIMESLICE);
    if (thread == RT_NULL)
    {
        rt_mutex_take(&s_random_lock, RT_WAITING_FOREVER);
        s_ui_state.phase = RANDOM_BASELINE_UI_READY;
        ui_state_publish_locked();
        rt_mutex_release(&s_random_lock);
        baseline_finish();
        rt_kprintf("[RANDOM] thread create failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    rt_kprintf("[RANDOM] started episodes=%lu max_steps=%u\n",
               (unsigned long)episodes,
               (unsigned int)RANDOM_MAX_STEPS);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(random_run_cmd, random_run, Run random-policy baseline episodes);

static int random_stats_cmd(int argc, char **argv)
{
    random_baseline_ui_state_t state;
    rt_bool_t active;

    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: random_stats\n");
        return -RT_EINVAL;
    }

    random_baseline_get_ui_state(&state);
    active = random_baseline_is_busy();
    rt_kprintf("[RANDOM] episodes=%lu/%lu success=%lu busy=%u\n",
               (unsigned long)state.current_episode,
               (unsigned long)state.target_episodes,
               (unsigned long)state.successful_episodes,
               (unsigned int)active);
    rt_kprintf("[RANDOM] avg_success_steps=%u best_steps=%u\n",
               (unsigned int)state.average_success_steps,
               (unsigned int)state.best_steps);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(random_stats_cmd, random_stats, Print random baseline statistics);

static int random_reset_cmd(int argc, char **argv)
{
    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: random_reset\n");
        return -RT_EINVAL;
    }
    if (random_baseline_reset() != RT_EOK)
    {
        rt_kprintf("[RANDOM] algorithm task is busy\n");
        return -RT_EBUSY;
    }

    app_mode_set(APP_MODE_COMPARE);
    rt_kprintf("[RANDOM] reset complete\n");
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(random_reset_cmd, random_reset, Reset random baseline statistics);

static int app_random_baseline_init(void)
{
    rt_mutex_init(&s_random_lock, "rnd_lock", RT_IPC_FLAG_PRIO);
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    s_ui_state.phase = RANDOM_BASELINE_UI_READY;
    s_random_state = RANDOM_DEFAULT_SEED ^ maze_env_map_revision();
    return RT_EOK;
}
INIT_APP_EXPORT(app_random_baseline_init);
