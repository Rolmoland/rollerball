#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include "app_demo_collector.h"
#include "app_q_training.h"
#include "module_maze_env.h"
#include "module_q_agent.h"

#define Q_TRAIN_DEFAULT_EPISODES       500U
#define Q_TRAIN_MAX_EPISODES           10000U
#define Q_TRAIN_MAX_STEPS              200U
#define Q_TRAIN_LOG_INTERVAL           50U
#define Q_TRAIN_THREAD_STACK_SIZE       2048U
#define Q_TRAIN_THREAD_PRIORITY         22U
#define Q_TRAIN_THREAD_TIMESLICE        10U
#define Q_INFER_UI_STEP_DELAY_MS         200U
#define Q_DEMO_MOVE_PREFERENCE          0.25f
#define Q_DEMO_COLLISION_PREFERENCE    -0.25f

typedef struct
{
    rt_uint32_t trained_episodes;
    rt_uint32_t successful_episodes;
    rt_uint32_t demonstration_seeds;
    rt_uint16_t last_steps;
    rt_int32_t last_reward_tenths;
    rt_bool_t last_success;
} q_training_stats_t;

static q_agent_t s_q_agent;
static q_training_stats_t s_training_stats;
static q_training_ui_state_t s_ui_state;
static struct rt_mutex s_training_lock;
static rt_bool_t s_training_active;

static void ui_state_publish_locked(void)
{
    s_ui_state.revision++;
}

rt_err_t q_training_get_ui_state(q_training_ui_state_t *state)
{
    if (state == RT_NULL)
    {
        return -RT_EINVAL;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    *state = s_ui_state;
    rt_mutex_release(&s_training_lock);
    return RT_EOK;
}

static rt_err_t command_count(int argc,
                              char **argv,
                              rt_uint32_t default_value,
                              rt_uint32_t max_value,
                              rt_uint32_t *count)
{
    unsigned long value;
    char *end;

    if (argc == 1)
    {
        *count = default_value;
        return RT_EOK;
    }
    if (argc != 2)
    {
        return -RT_EINVAL;
    }

    value = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || value == 0UL || value > max_value)
    {
        return -RT_EINVAL;
    }
    *count = (rt_uint32_t)value;
    return RT_EOK;
}

static rt_bool_t training_try_start(void)
{
    rt_bool_t started = RT_FALSE;

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    if (!s_training_active)
    {
        s_training_active = RT_TRUE;
        started = RT_TRUE;
    }
    rt_mutex_release(&s_training_lock);
    return started;
}

static void training_finish(void)
{
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_training_active = RT_FALSE;
    rt_mutex_release(&s_training_lock);
}

static rt_bool_t demo_transition_valid(const demo_transition_t *transition,
                                       rt_uint16_t *state,
                                       rt_uint16_t *next_state)
{
    if (transition->map_id != 0U ||
        transition->action >= MAZE_ENV_ACTION_COUNT)
    {
        return RT_FALSE;
    }

    *state = maze_env_state_index(transition->state_x, transition->state_y);
    *next_state = maze_env_state_index(transition->next_x,
                                       transition->next_y);
    return *state != MAZE_ENV_INVALID_STATE &&
           *next_state != MAZE_ENV_INVALID_STATE;
}

static int q_pretrain_cmd(int argc, char **argv)
{
    rt_uint16_t count = demo_collector_count();
    rt_uint16_t index;
    rt_uint32_t seeds = 0;

    (void)argv;

    if (argc != 1)
    {
        rt_kprintf("Usage: q_pretrain\n");
        return -RT_EINVAL;
    }
    if (!training_try_start())
    {
        rt_kprintf("[Q] training busy\n");
        return -RT_EBUSY;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = Q_TRAINING_UI_PRETRAIN;
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);

    if (count == 0U)
    {
        rt_kprintf("[Q] no demonstration data\n");
        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_ui_state.phase = Q_TRAINING_UI_READY;
        ui_state_publish_locked();
        rt_mutex_release(&s_training_lock);
        training_finish();
        return -RT_EEMPTY;
    }

    for (index = 0U; index < count; index++)
    {
        demo_transition_t transition;
        rt_uint16_t state;
        rt_uint16_t next_state;
        float preference;

        if (demo_collector_get(index, &transition) != RT_EOK ||
            !demo_transition_valid(&transition, &state, &next_state))
        {
            continue;
        }
        if (transition.result == MAZE_ENV_STEP_COLLISION)
        {
            preference = Q_DEMO_COLLISION_PREFERENCE;
        }
        else if (transition.result == MAZE_ENV_STEP_MOVED ||
                 transition.result == MAZE_ENV_STEP_GOAL)
        {
            preference = Q_DEMO_MOVE_PREFERENCE;
        }
        else
        {
            continue;
        }

        q_agent_seed_action(&s_q_agent,
                            state,
                            (maze_env_action_t)transition.action,
                            preference);
        seeds++;
    }
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_training_stats.demonstration_seeds += seeds;
    s_ui_state.phase = Q_TRAINING_UI_READY;
    s_ui_state.demonstration_seeds =
        s_training_stats.demonstration_seeds;
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    training_finish();
    rt_kprintf("[Q] pretrain complete samples=%u seeds=%u\n",
               count, seeds);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(q_pretrain_cmd, q_pretrain,
                     Seed Q table from demonstration data);

static void q_train_thread_entry(void *parameter)
{
    rt_uint32_t episodes = (rt_uint32_t)(rt_ubase_t)parameter;
    rt_uint32_t episode;
    rt_uint32_t run_successes = 0;

    for (episode = 1U; episode <= episodes; episode++)
    {
        maze_env_t env;
        rt_uint16_t step;

        if (maze_env_init(&env, 0U) != RT_EOK)
        {
            rt_kprintf("[Q] environment init failed\n");
            break;
        }

        for (step = 0U; step < Q_TRAIN_MAX_STEPS; step++)
        {
            rt_uint16_t state = maze_env_state_index(env.agent_x,
                                                     env.agent_y);
            maze_env_action_t action =
                q_agent_select_action(&s_q_agent, state, RT_TRUE);
            float reward;
            rt_uint16_t next_state;
            rt_bool_t done;

            maze_env_step(&env, action, &reward);
            next_state = maze_env_state_index(env.agent_x, env.agent_y);
            done = maze_env_is_done(&env);
            q_agent_update(&s_q_agent, state, action, reward,
                           next_state, done);
            if (done)
            {
                break;
            }
        }

        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_training_stats.trained_episodes++;
        s_training_stats.last_steps = env.total_steps;
        s_training_stats.last_reward_tenths =
            (rt_int32_t)(env.cumulative_reward * 10.0f);
        s_training_stats.last_success = maze_env_is_done(&env);
        if (s_training_stats.last_success)
        {
            s_training_stats.successful_episodes++;
            run_successes++;
        }
        q_agent_decay_epsilon(&s_q_agent);
        s_ui_state.phase = Q_TRAINING_UI_TRAINING;
        s_ui_state.current_episode = episode;
        s_ui_state.successful_episodes = run_successes;
        s_ui_state.steps = env.total_steps;
        s_ui_state.reward_tenths = s_training_stats.last_reward_tenths;
        s_ui_state.epsilon_milli =
            (rt_uint32_t)(s_q_agent.epsilon * 1000.0f);
        ui_state_publish_locked();

        if (episode == 1U || episode == episodes ||
            episode % Q_TRAIN_LOG_INTERVAL == 0U)
        {
            rt_kprintf("[Q] episode=%u/%u reward_x10=%d steps=%u "
                       "success=%u epsilon_x1000=%u\n",
                       episode, episodes,
                       s_training_stats.last_reward_tenths,
                       s_training_stats.last_steps,
                       s_training_stats.last_success,
                       (rt_uint32_t)(s_q_agent.epsilon * 1000.0f));
        }
        rt_mutex_release(&s_training_lock);
        if (episode % 10U == 0U)
        {
            rt_thread_mdelay(1);
        }
    }
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = Q_TRAINING_UI_TRAINED;
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    training_finish();
    rt_kprintf("[Q] train complete episodes=%u success=%u\n",
               episodes, run_successes);
}

static int q_train_cmd(int argc, char **argv)
{
    rt_uint32_t episodes;
    rt_thread_t thread;

    if (command_count(argc, argv, Q_TRAIN_DEFAULT_EPISODES,
                      Q_TRAIN_MAX_EPISODES, &episodes) != RT_EOK)
    {
        rt_kprintf("Usage: q_train [1-%u]\n", Q_TRAIN_MAX_EPISODES);
        return -RT_EINVAL;
    }
    if (!training_try_start())
    {
        rt_kprintf("[Q] training busy\n");
        return -RT_EBUSY;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = Q_TRAINING_UI_TRAINING;
    s_ui_state.agent_x = 0U;
    s_ui_state.agent_y = 0U;
    s_ui_state.inference_success = RT_FALSE;
    s_ui_state.steps = 0U;
    s_ui_state.current_episode = 0U;
    s_ui_state.target_episodes = episodes;
    s_ui_state.successful_episodes = 0U;
    s_ui_state.reward_tenths = 0;
    s_ui_state.epsilon_milli =
        (rt_uint32_t)(s_q_agent.epsilon * 1000.0f);
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);

    thread = rt_thread_create("q_train",
                              q_train_thread_entry,
                              (void *)(rt_ubase_t)episodes,
                              Q_TRAIN_THREAD_STACK_SIZE,
                              Q_TRAIN_THREAD_PRIORITY,
                              Q_TRAIN_THREAD_TIMESLICE);
    if (thread == RT_NULL)
    {
        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_ui_state.phase = Q_TRAINING_UI_READY;
        ui_state_publish_locked();
        rt_mutex_release(&s_training_lock);
        training_finish();
        rt_kprintf("[Q] training thread create failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    rt_kprintf("[Q] training started episodes=%u\n", episodes);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(q_train_cmd, q_train,
                     Train Q table in the local maze environment);

static int q_infer_cmd(int argc, char **argv)
{
    maze_env_t env;
    rt_uint16_t step;

    (void)argc;
    (void)argv;

    if (!training_try_start())
    {
        rt_kprintf("[Q] training busy\n");
        return -RT_EBUSY;
    }
    if (maze_env_init(&env, 0U) != RT_EOK)
    {
        training_finish();
        rt_kprintf("[Q] environment init failed\n");
        return -RT_ERROR;
    }

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = Q_TRAINING_UI_INFERENCE;
    s_ui_state.agent_x = env.agent_x;
    s_ui_state.agent_y = env.agent_y;
    s_ui_state.inference_success = RT_FALSE;
    s_ui_state.steps = 0U;
    s_ui_state.reward_tenths = 0;
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);

    for (step = 1U; step <= Q_TRAIN_MAX_STEPS; step++)
    {
        rt_uint8_t state_x = env.agent_x;
        rt_uint8_t state_y = env.agent_y;
        rt_uint16_t state = maze_env_state_index(state_x, state_y);
        maze_env_action_t action =
            q_agent_select_action(&s_q_agent, state, RT_FALSE);
        maze_env_step_result_t result;
        float reward;
        rt_bool_t done;

        result = maze_env_step(&env, action, &reward);
        done = maze_env_is_done(&env);
        rt_kprintf("Q_PATH,%u,%u,%u,%u,%u,%u,%d,%u,%u\n",
                   step, state_x, state_y, action,
                   env.agent_x, env.agent_y,
                   (rt_int32_t)(reward * 10.0f), result, done);

        rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
        s_ui_state.agent_x = env.agent_x;
        s_ui_state.agent_y = env.agent_y;
        s_ui_state.inference_success = done;
        s_ui_state.steps = env.total_steps;
        s_ui_state.reward_tenths =
            (rt_int32_t)(env.cumulative_reward * 10.0f);
        ui_state_publish_locked();
        rt_mutex_release(&s_training_lock);
        rt_thread_mdelay(Q_INFER_UI_STEP_DELAY_MS);

        if (done)
        {
            break;
        }
    }

    rt_kprintf("[Q] infer success=%u steps=%u reward_x10=%d\n",
               maze_env_is_done(&env), env.total_steps,
               (rt_int32_t)(env.cumulative_reward * 10.0f));
    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    s_ui_state.phase = Q_TRAINING_UI_INFERENCE_DONE;
    s_ui_state.inference_success = maze_env_is_done(&env);
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    training_finish();
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(q_infer_cmd, q_infer,
                     Run one greedy Q table inference episode);

static int q_stats_cmd(int argc, char **argv)
{
    q_training_stats_t stats;
    rt_uint32_t epsilon_milli;
    rt_bool_t active;

    (void)argc;
    (void)argv;

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    stats = s_training_stats;
    epsilon_milli = (rt_uint32_t)(s_q_agent.epsilon * 1000.0f);
    active = s_training_active;
    rt_mutex_release(&s_training_lock);

    rt_kprintf("[Q] episodes=%u success=%u demo_seeds=%u "
               "epsilon_x1000=%u\n",
               stats.trained_episodes,
               stats.successful_episodes,
               stats.demonstration_seeds,
               epsilon_milli);
    rt_kprintf("[Q] last success=%u steps=%u reward_x10=%d busy=%u\n",
               stats.last_success,
               stats.last_steps,
               stats.last_reward_tenths,
               active);
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(q_stats_cmd, q_stats, Print Q training statistics);

static int q_reset_cmd(int argc, char **argv)
{
    rt_uint32_t revision;

    (void)argc;
    (void)argv;

    rt_mutex_take(&s_training_lock, RT_WAITING_FOREVER);
    if (s_training_active)
    {
        rt_mutex_release(&s_training_lock);
        rt_kprintf("[Q] training busy\n");
        return -RT_EBUSY;
    }

    q_agent_reset(&s_q_agent);
    memset(&s_training_stats, 0, sizeof(s_training_stats));
    revision = s_ui_state.revision;
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    s_ui_state.revision = revision;
    s_ui_state.phase = Q_TRAINING_UI_READY;
    s_ui_state.epsilon_milli =
        (rt_uint32_t)(s_q_agent.epsilon * 1000.0f);
    ui_state_publish_locked();
    rt_mutex_release(&s_training_lock);
    rt_kprintf("[Q] reset complete\n");
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(q_reset_cmd, q_reset, Reset Q table and statistics);

static int app_q_training_init(void)
{
    rt_mutex_init(&s_training_lock, "q_lock", RT_IPC_FLAG_PRIO);
    q_agent_init(&s_q_agent, 0U);
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    return RT_EOK;
}
INIT_APP_EXPORT(app_q_training_init);
